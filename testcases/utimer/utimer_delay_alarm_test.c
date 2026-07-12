// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#define COUNTER_SYSFS "/sys/bus/counter/devices/counter%d/count%ld"
#include <errno.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <linux/counter.h>
#include <linux/types.h>
#include <string.h>

#define NSEC_PER_SEC 1000000000ULL
#define BUF_SIZE 64
#define SYSFS_VAL_0 "0"
#define SYSFS_VAL_1 "1"
#define CHANNEL_MIN 0
#define CHANNEL_MAX 11
#define COUNTER_DEVICE_NUM 0
#define TIMEOUT_MARGIN_SEC 5
#define MSEC_PER_SEC 1000

/**
 * User Space API: Set a Delay-Time Alarm
 * delay_ns is the relative time (in nanoseconds) from NOW
 * when you want the alarm to fire.
 */
int set_delaytime_alarm(int sysfs_fd, __u64 delay_ns)
{
	char buffer[BUF_SIZE];

	printf("DELAYTIME ALARM: Request alarm for %.2f sec (%llu ns)..\n",
	       (double)delay_ns / NSEC_PER_SEC, delay_ns);

	snprintf(buffer, sizeof(buffer), "%llu\n", delay_ns);

	{
		ssize_t ret = write(sysfs_fd, buffer, strlen(buffer));

		if (ret < 0) {
			perror("Failed to write to sysfs delay_alarm");
			return -1;
		}
		if ((size_t)ret < strlen(buffer)) {
			fprintf(stderr,
				"Failed to write complete buffer to delay_alarm\n");
			return -1;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	long channel;
	unsigned long long sec;
	char *endptr;
	int dev_fd, sysfs_fd;
	struct counter_watch watch;
	struct counter_event event;
	__u64 delay_ns;
	char path[PATH_MAX];
	int run_fd;
	int enable_fd;
	int flags;

	if (argc < 3) {
		fprintf(stderr,
			"invalid argument!!! Usage: %s <channel> <seconds>\n",
			argv[0]);
		return EXIT_FAILURE;
	}

	errno = 0;
	channel = strtol(argv[1], &endptr, 10);
	if (errno || endptr == argv[1] || *endptr != '\0' ||
	    channel < CHANNEL_MIN || channel > CHANNEL_MAX) {
		fprintf(stderr, "Invalid channel number (0-11)\n");
		return EXIT_FAILURE;
	}

	errno = 0;
	sec = strtoull(argv[2], &endptr, 10);
	if (errno || endptr == argv[2] || *endptr != '\0') {
		fprintf(stderr, "Invalid seconds value\n");
		return EXIT_FAILURE;
	}
	/* Prevent integer overflow in poll() timeout */
	if (sec > (INT_MAX / MSEC_PER_SEC) - TIMEOUT_MARGIN_SEC) {
		fprintf(stderr, "Seconds value too large for poll()\n");
		return EXIT_FAILURE;
	}

	/* 1. Setup Counter Device framework for catching the event */
	{
		char dev_path[PATH_MAX];

		if (snprintf(dev_path, sizeof(dev_path), "/dev/counter%d",
			     COUNTER_DEVICE_NUM) < 0 ||
		    (size_t)snprintf(dev_path, sizeof(dev_path),
				     "/dev/counter%d",
				     COUNTER_DEVICE_NUM) >= sizeof(dev_path)) {
			fprintf(stderr, "Error: device path too long\n");
			return EXIT_FAILURE;
		}
		dev_fd = open(dev_path, O_RDWR);
		if (dev_fd < 0) {
			perror("Failed to open counter device");
			return EXIT_FAILURE;
		}
	}

	memset(&watch, 0, sizeof(watch));
	watch.component.type = COUNTER_COMPONENT_NONE;
	watch.component.scope = COUNTER_SCOPE_COUNT;
	/* FIX: Set parent to the selected channel (Count ID) */
	watch.component.parent = channel;
	watch.event = COUNTER_EVENT_THRESHOLD;
	watch.channel = channel;

	if (ioctl(dev_fd, COUNTER_ADD_WATCH_IOCTL, &watch) < 0) {
		perror("Failed to ADD_WATCH");
		close(dev_fd);
		return EXIT_FAILURE;
	}

	if (ioctl(dev_fd, COUNTER_ENABLE_EVENTS_IOCTL) < 0) {
		perror("Failed to ENABLE_EVENTS");
		close(dev_fd);
		return EXIT_FAILURE;
	}

	/* 2. Open our delay_alarm extension */
	if (snprintf(path, sizeof(path),
		     COUNTER_SYSFS "/delay_alarm",
		     COUNTER_DEVICE_NUM, channel) < 0 ||
	    (size_t)snprintf(path, sizeof(path),
			     COUNTER_SYSFS "/delay_alarm",
			     COUNTER_DEVICE_NUM,
			     channel) >= sizeof(path)) {
		fprintf(stderr, "Error: sysfs path too long for delay_alarm\n");
		close(dev_fd);
		return EXIT_FAILURE;
	}
	sysfs_fd = open(path, O_WRONLY);
	if (sysfs_fd < 0) {
		perror("Failed to open sysfs delay_alarm node");
		close(dev_fd);
		return EXIT_FAILURE;
	}

	/* 2b. Stop, Enable and Start the Counter */
	if (snprintf(path, sizeof(path),
		     COUNTER_SYSFS "/running",
		     COUNTER_DEVICE_NUM, channel) < 0 ||
	    (size_t)snprintf(path, sizeof(path),
			     COUNTER_SYSFS "/running",
			     COUNTER_DEVICE_NUM,
			     channel) >= sizeof(path)) {
		fprintf(stderr, "Error: sysfs path too long for running\n");
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}
	run_fd = open(path, O_WRONLY);
	if (run_fd < 0) {
		perror("Failed to open running sysfs");
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}
	if (write(run_fd, SYSFS_VAL_0, 1) < 0) {
		perror("Error writing to running sysfs");
		close(run_fd);
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}
	close(run_fd);

	if (snprintf(path, sizeof(path),
		     COUNTER_SYSFS "/enable",
		     COUNTER_DEVICE_NUM, channel) < 0 ||
	    (size_t)snprintf(path, sizeof(path),
			     COUNTER_SYSFS "/enable",
			     COUNTER_DEVICE_NUM,
			     channel) >= sizeof(path)) {
		fprintf(stderr, "Error: sysfs path too long for enable\n");
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}
	enable_fd = open(path, O_WRONLY);
	if (enable_fd < 0) {
		perror("Failed to open enable sysfs");
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}
	if (write(enable_fd, SYSFS_VAL_1, 1) < 0) {
		perror("Error writing to enable sysfs");
		close(enable_fd);
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}
	close(enable_fd);

	if (snprintf(path, sizeof(path),
		     COUNTER_SYSFS "/running",
		     COUNTER_DEVICE_NUM, channel) < 0 ||
	    (size_t)snprintf(path, sizeof(path),
			     COUNTER_SYSFS "/running",
			     COUNTER_DEVICE_NUM,
			     channel) >= sizeof(path)) {
		fprintf(stderr, "Error: sysfs path too long for running\n");
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}
	run_fd = open(path, O_WRONLY);
	if (run_fd < 0) {
		perror("Failed to open running sysfs");
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}
	if (write(run_fd, SYSFS_VAL_1, 1) < 0) {
		perror("Error writing to running sysfs");
		close(run_fd);
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}
	close(run_fd);

	/* Flush any stale events left in the kernel queue */
	flags = fcntl(dev_fd, F_GETFL);
	if (flags < 0) {
		perror("Failed to get file flags");
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}
	if (fcntl(dev_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		perror("Failed to set non-blocking mode");
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}
	while (read(dev_fd, &event, sizeof(event)) == sizeof(event))
		;
	if (fcntl(dev_fd, F_SETFL, flags) < 0) {
		perror("Failed to restore file flags");
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}

	/* Convert seconds to nanoseconds */
	delay_ns = (sec * NSEC_PER_SEC);

	/* Trigger our Delay Time API */
	if (set_delaytime_alarm(sysfs_fd, delay_ns) < 0) {
		close(sysfs_fd);
		close(dev_fd);
		return EXIT_FAILURE;
	}

	/* Wait for the event! */
	printf("\nWaiting for hardware to trigger event...\n");

	{
		struct pollfd pfd = { .fd = dev_fd, .events = POLLIN };
		int timeout = (sec + TIMEOUT_MARGIN_SEC) * MSEC_PER_SEC;
		int ret = poll(&pfd, 1, timeout);

		if (ret < 0) {
			perror("Poll failed");
			close(sysfs_fd);
			close(dev_fd);
			return EXIT_FAILURE;
		}
		if (ret == 0) {
			fprintf(stderr,
				"Timeout: no event recv within %llu sec\n",
			       sec + TIMEOUT_MARGIN_SEC);
			close(sysfs_fd);
			close(dev_fd);
			return EXIT_FAILURE;
		}
	}
	if (read(dev_fd, &event, sizeof(event)) == sizeof(event))
		printf("\nAlarm event received successfully.\n");
	else
		perror("Read event failed");

	close(sysfs_fd);
	close(dev_fd);
	return EXIT_SUCCESS;
}
