// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <linux/counter.h>

#define COUNTER_DEVICE_NUM 0
#define STR(x) #x
#define XSTR(x) STR(x)
#define COUNTER_BASE "/sys/bus/counter/devices/counter" XSTR(COUNTER_DEVICE_NUM)
#define COUNTER_DEV "/dev/counter" XSTR(COUNTER_DEVICE_NUM)
#ifndef BIT_ULL
#define BIT_ULL(nr) (1ULL << (nr))
#endif
#define COUNTER_MAX_32BIT BIT_ULL(32) /* 2^32 */
#define COUNTER_CLOCK_HZ 400000000UL
#define BUF_SIZE 64
#define NSEC_PER_SEC 1000000000ULL
#define SYSFS_VAL_1 "1"
#define DEFAULT_MODE "normal"
#define SYSFS_DIRECTION_FORWARD "forward"
#define DEFAULT_VAL 0
#define DEFAULT_MAX_EVENTS 10U

unsigned long long prev_timestamp;

/* Helper to write a string to sysfs */
int write_sysfs_str(const char *path, const char *value)
{
	int fd = open(path, O_WRONLY);
	char buf[BUF_SIZE];

	if (fd < 0) {
		fprintf(stderr, "Error opening sysfs path for writing: %s\n",
			path);
		perror("Reason");
		return -1;
	}

	snprintf(buf, sizeof(buf), "%s\n", value);
	if (write(fd, buf, strlen(buf)) < 0) {
		perror("Error writing to sysfs");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

/* Helper to write an unsigned long to sysfs */
int write_sysfs_ul(const char *path, unsigned long value)
{
	char buf[BUF_SIZE];

	snprintf(buf, sizeof(buf), "%lu", value);
	return write_sysfs_str(path, buf);
}

/* Helper to read an unsigned long from sysfs.
 * Returns 0 on success, -1 on error.
 */
int read_sysfs(const char *path, unsigned long *val)
{
	char buf[BUF_SIZE];
	int fd = open(path, O_RDONLY);
	ssize_t ret;

	if (fd < 0) {
		fprintf(stderr, "Error opening sysfs path for reading: %s\n",
			path);
		perror("Reason");
		return -1;
	}

	ret = read(fd, buf, sizeof(buf) - 1);
	if (ret < 0) {
		perror("Error reading sysfs path");
		close(fd);
		return -1;
	}
	if (ret == 0) {
		fprintf(stderr, "Empty read from sysfs path: %s\n",
			path);
		close(fd);
		return -1;
	}

	buf[ret] = '\0';
	*val = strtoul(buf, NULL, 10);
	close(fd);
	return 0;
}

/* Helper to read a string from sysfs.
 * Returns 0 on success, -1 on error.
 */
int read_sysfs_string(const char *path, char *buf, size_t size)
{
	int fd = open(path, O_RDONLY);
	ssize_t ret;

	if (fd < 0) {
		perror(path);
		return -1;
	}
	ret = read(fd, buf, size - 1);
	if (ret > 0) {
		/* Strip trailing newline for clean printing */
		if (buf[ret - 1] == '\n')
			buf[ret - 1] = '\0';
		else
			buf[ret] = '\0';
	} else {
		if (ret < 0)
			perror(path);
		else
			fprintf(stderr, "Empty read from sysfs path: %s\n",
				path);
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

/* Function to make sure the hardware is enabled and running */
void start_counter_hardware(int channel)
{
	char enable_path[PATH_MAX];
	char running_path[PATH_MAX];

	snprintf(enable_path, sizeof(enable_path),
		 COUNTER_BASE "/count%d/enable", channel);
	snprintf(running_path, sizeof(running_path),
		 COUNTER_BASE "/count%d/running", channel);

	printf("-> Auto-enabling and starting hardware for Channel %d...\n",
	       channel);

	/* Write "1" to enable and running sysfs nodes */
	if (write_sysfs_str(enable_path, SYSFS_VAL_1) < 0) {
		fprintf(stderr,
			"Error: failed to enable counter on channel %d\n",
			channel);
		exit(EXIT_FAILURE);
	}
	if (write_sysfs_str(running_path, SYSFS_VAL_1) < 0) {
		fprintf(stderr,
			"Error: failed to start counter on channel %d\n",
			channel);
		exit(EXIT_FAILURE);
	}
}

int test_alarm(int channel, unsigned long alarm_ticks,
	       unsigned int max_events, int is_default_events)
{
	int fd;
	struct counter_watch watch;
	struct pollfd pfd;
	struct counter_event event_data;

	char compare_path[PATH_MAX];
	char count_path[PATH_MAX];
	char direction_path[PATH_MAX];
	char function_path[PATH_MAX];
	char mode_path[PATH_MAX];
	char action_path[PATH_MAX];

	char direction[PATH_MAX];
	char function[PATH_MAX];
	char mode[PATH_MAX];
	char action[PATH_MAX];

	unsigned long current_val;
	unsigned long target_val;
	unsigned long now_val;
	unsigned long long next_alarm_ticks;
	int ret;
	ssize_t read_ret;
	double diff_sec;
	unsigned int events_received = 0;
	int loop_error = 0;

	/* Construct the sysfs paths for the specified channel */
	snprintf(compare_path, sizeof(compare_path),
		 COUNTER_BASE "/count%d/compare_value", channel);
	snprintf(count_path, sizeof(count_path),
		 COUNTER_BASE "/count%d/count", channel);

	/* Construct paths for configuration properties. */
	snprintf(direction_path, sizeof(direction_path),
		 COUNTER_BASE "/count%d/direction_rw", channel);
	snprintf(function_path, sizeof(function_path),
		 COUNTER_BASE "/count%d/function", channel);
	snprintf(mode_path, sizeof(mode_path),
		 COUNTER_BASE "/count%d/count_mode", channel);
	snprintf(action_path, sizeof(action_path),
		 COUNTER_BASE "/count%d/signal0_action", channel);

	/* Read properties directly from your driver */
	if (read_sysfs_string(direction_path, direction,
			      sizeof(direction)) < 0 ||
	    read_sysfs_string(function_path, function,
			      sizeof(function)) < 0 ||
	    read_sysfs_string(mode_path, mode, sizeof(mode)) < 0 ||
	    read_sysfs_string(action_path, action, sizeof(action)) < 0) {
		fprintf(stderr,
			"Critical: Could not read channel %d properties\n",
			channel);
		return -1;
	}

	/* Print out the requested info */
	printf("========================================\n");
	printf("Channel Name         = Channel %d\n", channel);
	printf("Channel Direction    = %s\n", direction);
	printf("Channel Function     = %s\n", function);
	printf("Count Mode           = %s\n", mode);
	printf("Signal Action        = %s\n", action);
	printf("Seconds: 0 = forced immediate interrupt,\n");
	printf(">0 = hardware match\n");
	if (is_default_events) {
		printf("events: num alarm events to receive (default %u)\n",
		       DEFAULT_MAX_EVENTS);
		printf("Program will exit after receiving default events.\n");
	} else {
		printf("events: number of alarm events to receive (%u)\n",
		       max_events);
		printf("Program will exit after receiving given events.\n");
	}
	printf("Note: Alarm interval will double on each of next alarm.\n");
	printf("========================================\n\n");

	/* Tell the user if the mode is unsupported and exit */
	if (strcmp(mode, DEFAULT_MODE) != 0) {
		printf("ERROR: Current Count Mode is '%s'.\n", mode);
		printf("This alarm program ONLY works when the mode is '%s',\n",
		       DEFAULT_MODE);
		printf("because it relies on 32-bit overflow (wrap at 2^32).\n");
		printf("Other modes (like 'range limit') won't work right.\n");
		printf("Please configure counter to 'normal' mode and retry.\n");
		return -1; /* Exit safely before starting anything */
	}

	/* Tell the user if the direction is unsupported and exit */
	if (strcmp(direction, SYSFS_DIRECTION_FORWARD) != 0) {
		printf("ERROR: Current Direction is '%s'.\n", direction);
		printf("This alarm program ONLY works in the '%s' direction.\n",
		       SYSFS_DIRECTION_FORWARD);
		printf("Please change the direction to '%s' and try again.\n",
		       SYSFS_DIRECTION_FORWARD);
		return -1; /* Exit safely before starting anything */
	}

	/* 0. Enable and Run the counter first! */
	start_counter_hardware(channel);

	/* 1. Open the counter character device */
	fd = open(COUNTER_DEV, O_RDWR);
	if (fd < 0) {
		perror("Failed to open " COUNTER_DEV " (Driver loaded?)");
		return -1;
	}

	/* 2. Configure the watch for the THRESHOLD (Alarm) event */
	memset(&watch, 0, sizeof(watch));
	watch.component.type = COUNTER_COMPONENT_NONE;
	watch.component.scope = COUNTER_SCOPE_COUNT;
	watch.component.parent = channel; /* Use the dynamic channel */

	watch.event = COUNTER_EVENT_THRESHOLD;
	watch.channel = channel;          /* Watch for this specific channel */

	if (ioctl(fd, COUNTER_ADD_WATCH_IOCTL, &watch) < 0) {
		perror("Failed to add watch ioctl");
		close(fd);
		return -1;
	}

	/* 3. Enable events */
	if (ioctl(fd, COUNTER_ENABLE_EVENTS_IOCTL) < 0) {
		perror("Failed to enable events ioctl");
		close(fd);
		return -1;
	}

	/* 4. Set the initial target! */
	if (read_sysfs(count_path, &current_val) < 0) {
		fprintf(stderr, "Critical: Could not read counter value\n");
		close(fd);
		return -1;
	}

	if (alarm_ticks == 0) {
		/* FORCED IMMEDIATE PATH: write 0 to bypass hardware match */
		target_val = DEFAULT_VAL;
		printf("Counter currently at: %lu (%.4f seconds)\n",
		       current_val, (double)current_val / COUNTER_CLOCK_HZ);
		printf("FORCED: Writing compare_value=%d to trigger immediate ",
		       DEFAULT_VAL);
		printf("interrupt (no hardware match)\n");
	} else {
		/* NORMAL PATH: set target in the future with 32-bit modulo! */
		target_val = (current_val + alarm_ticks) % COUNTER_MAX_32BIT;
		printf("Counter currently at: %lu (%.4f seconds)\n",
		       current_val, (double)current_val / COUNTER_CLOCK_HZ);
		printf("Set alarm to %lu (%.4f sec / %lu ticks from now)\n",
		       target_val, (double)alarm_ticks /
				      COUNTER_CLOCK_HZ,
		       alarm_ticks);
	}

	/* Write to the sysfs node to arm the alarm */
	if (write_sysfs_ul(compare_path,
			   target_val) < 0) {
		close(fd);
		return -1;
	}

	/* 5. Set up poll() to wait for the interrupt */
	pfd.fd = fd;
	pfd.events = POLLIN;

	while (events_received < max_events) {
		if (alarm_ticks == 0)
			printf("Waiting for FORCED immediate interrupt...\n");
		else
			printf("Sleeping in poll() waiting for hw interrupt...\n");

		ret = poll(&pfd, 1, -1);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror("poll failed");
			break;
		}
		if (ret == 0)
			continue;

		if (pfd.revents & POLLIN) {
			read_ret = read(fd, &event_data, sizeof(event_data));
			if (read_ret != (ssize_t)sizeof(event_data)) {
				if (read_ret < 0 && errno == EINTR)
					continue;
				perror("Error reading counter event");
				continue;
			}

			if (event_data.watch.event == COUNTER_EVENT_THRESHOLD) {
				if (read_sysfs(count_path, &now_val) < 0) {
					fprintf(stderr,
						"Error: Could not read count\n");
					continue;
				}

				/* Double the wait for next alarm
				 * and wrap to 32 bits
				 */
				next_alarm_ticks =
					((unsigned long long)
					 alarm_ticks * 2ULL) %
					COUNTER_MAX_32BIT;
				if (next_alarm_ticks == 0)
					printf("wrap 0: verify imm. detect\n");
				alarm_ticks = (unsigned long)next_alarm_ticks;
				target_val = (now_val + alarm_ticks) %
					COUNTER_MAX_32BIT;

				/**
				 * Arm next alarm immediately before printing
				 * to avoid
				 * blocking!
				 */
				if (write_sysfs_ul(compare_path,
						   target_val) < 0) {
					loop_error = 1;
					break;
				}

				/* NORMAL hardware match interrupt */
				printf("\n!!! Alarm Interrupt on Channel %d !!!\n",
				       channel);
				printf("Count: %lu (%lu sec) (Kernel Time: %llu ns)\n",
				       now_val, (unsigned long)(now_val /
					  COUNTER_CLOCK_HZ),
				       event_data.timestamp);

				if (prev_timestamp > 0) {
					diff_sec =
						(double)(event_data.timestamp -
							 prev_timestamp) /
						(double)NSEC_PER_SEC;
					printf("Time diff from last alarm: %.6g sec\n",
					       diff_sec);
				}
				prev_timestamp = event_data.timestamp;

				printf("Next alarm: %lu (%.4g sec / %lu ticks from now)\n\n",
				       target_val, (double)alarm_ticks /
				      COUNTER_CLOCK_HZ,
				       alarm_ticks);
				events_received++;
			}
		}
	}

	close(fd);
	return loop_error ? -1 : 0;
}

int main(int argc, char *argv[])
{
	int channel;
	long channel_long;
	unsigned long current_ticks;
	unsigned long seconds;
	unsigned long max_events_ul = DEFAULT_MAX_EVENTS;
	unsigned int max_events = DEFAULT_MAX_EVENTS;
	int is_default_events = 1;
	char *endptr;

	/* Explicitly initialize global timestamp */
	prev_timestamp = 0;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s <channel> <seconds> [events]\n",
			argv[0]);
		fprintf(stderr, "Channel must be between 0 to 11\n");
		fprintf(stderr, "Seconds: 0 = forced immediate interrupt,\n");
		fprintf(stderr, ">0 = hardware match\n");
		fprintf(stderr, "events: num alarm events to recv (default %u)\n",
			DEFAULT_MAX_EVENTS);
		return EXIT_FAILURE;
	}
	errno = 0;
	channel_long = strtol(argv[1], &endptr, 10);
	if (errno || endptr == argv[1] || *endptr != '\0') {
		fprintf(stderr, "Invalid channel number (0-11)\n");
		return EXIT_FAILURE;
	}

	if (channel_long < 0 || channel_long > 11) {
		fprintf(stderr, "Invalid channel number (0-11)\n");
		return EXIT_FAILURE;
	}
	channel = channel_long;
	errno = 0;
	seconds = strtoul(argv[2], &endptr, 10);
	if (errno || endptr == argv[2] || *endptr != '\0') {
		fprintf(stderr, "Invalid seconds value\n");
		return EXIT_FAILURE;
	}
	if (seconds > ULONG_MAX / COUNTER_CLOCK_HZ) {
		fprintf(stderr, "Seconds value too large\n");
		return EXIT_FAILURE;
	}
	current_ticks = COUNTER_CLOCK_HZ * seconds;

	if (argc < 4 && current_ticks > 0) {
		unsigned long long ticks = current_ticks;
		unsigned int i;

		for (i = 1; i <= 32; i++) {
			ticks = (ticks * 2ULL) % COUNTER_MAX_32BIT;
			if (ticks == 0) {
				max_events = i + DEFAULT_MAX_EVENTS;
				break;
			}
		}
	}

	if (argc >= 4) {
		is_default_events = 0;
		errno = 0;
		max_events_ul = strtoul(argv[3], &endptr, 10);
		if (errno || endptr == argv[3] || *endptr != '\0' ||
		    max_events_ul == 0 || max_events_ul > UINT_MAX) {
			fprintf(stderr, "Invalid events value\n");
			return EXIT_FAILURE;
		}
		max_events = (unsigned int)max_events_ul;
	}

	if (test_alarm(channel, current_ticks, max_events,
		       is_default_events) < 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
