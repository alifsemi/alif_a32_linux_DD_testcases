// SPDX-License-Identifier: GPL-2.0-only

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))
#define ITER			102
#define CEILING_10M		"10000000"
#define CEILING_100M		"100000000"
#define CEILING_1B		"1000000000"
#define SYSFS_BASE_MAX		128
#define BUF_SIZE		64
#define USLEEP_DELAY_US		100000
#define MICROSECONDS_PER_SECOND	1000000
#define UINT32_MAX_STR		"4294967295"
#define SYSFS_VAL_0		"0"
#define SYSFS_VAL_1		"1"
#define DEFAULT_VAL		"0"
#define DEFAULT_MODE		"normal"
#define DIRECTION_FORWARD	"0"
#define CHANNEL_MIN		0
#define CHANNEL_MAX		11
#define COUNTER_DEVICE_NUM	0

static char sysfs_path[SYSFS_BASE_MAX] = "";
static const char *channel_num;

struct test_mode {
	const char *mode;
	const char *ceiling;
};

static int write_file(const char *file, const char *value)
{
	char path[PATH_MAX];
	ssize_t ret;
	int fd;

	ret = snprintf(path, sizeof(path), "%s%s", sysfs_path, file);
	if (ret < 0 || (size_t)ret >= sizeof(path))
		return -1;

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		perror(path);
		return -1;
	}

	ret = write(fd, value, strlen(value));
	if (ret < 0) {
		fprintf(stderr, "Error: failed to write '%s' to %s: %s\n",
			value, path, strerror(errno));
		close(fd);
		return -1;
	}
	if ((size_t)ret < strlen(value)) {
		fprintf(stderr,
			"Error: partial write to %s (%zd of %zu bytes)\n",
			path, ret, strlen(value));
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

static int read_file(const char *file, char *buf, size_t len)
{
	char path[PATH_MAX];
	ssize_t ret;
	int fd;

	ret = snprintf(path, sizeof(path), "%s%s", sysfs_path, file);
	if (ret < 0 || (size_t)ret >= sizeof(path))
		return -1;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		return -1;
	}

	if (len == 0) {
		close(fd);
		return -1;
	}

	ret = read(fd, buf, len - 1);
	if (ret < 0) {
		perror(path);
		close(fd);
		return -1;
	}

	if (ret > 0)
		buf[ret] = '\0';
	else
		buf[0] = '\0';

	close(fd);
	return 0;
}

static void strip_newline(char *str)
{
	char *nl;

	if (!str)
		return;
	nl = strchr(str, '\n');
	if (nl)
		*nl = '\0';
}

static int monitor_counter(void)
{
	char buf[BUF_SIZE];
	int i;

	for (i = 0; i < ITER; i++) {
		if (read_file("count", buf, sizeof(buf)) < 0)
			return -1;
		strip_newline(buf);
		printf("   [CH%s] Live Count [%3d/%d]: %s\n",
		       channel_num, i + 1, ITER, buf);

		usleep(USLEEP_DELAY_US);
	}
	return 0;
}

static int test(const struct test_mode *t)
{
	char buf[BUF_SIZE];
	int elapsed_seconds = (ITER * USLEEP_DELAY_US) /
				MICROSECONDS_PER_SECOND;

	printf("\n======================================================\n");
	printf("TESTING MODE: %s on CHANNEL: %s\n", t->mode, channel_num);
	printf("======================================================\n");

	/* 1. Stop the counter */
	if (write_file("running", SYSFS_VAL_0) < 0) {
		fprintf(stderr, "FAIL: Could not stop counter\n");
		return -1;
	}

	/* 2. Disable the counter before reconfiguring */
	if (write_file("enable", SYSFS_VAL_0) < 0) {
		fprintf(stderr, "FAIL: Could not disable counter\n");
		return -1;
	}

	/* 3. Set the mode */
	if (write_file("count_mode", t->mode) < 0) {
		fprintf(stderr, "FAIL: Could not set count_mode to '%s'\n",
			t->mode);
		return -1;
	}

	/* 4. Verify the mode was actually applied */
	if (read_file("count_mode", buf, sizeof(buf)) < 0)
		return -1;
	strip_newline(buf);
	if (strcmp(buf, t->mode) != 0) {
		fprintf(stderr,
			"FAIL: count_mode mismatch! Wrote '%s' but read '%s'\n",
			t->mode, buf);
		return -1;
	}
	printf("[Verification] Mode set to: %s\n", buf);

	/* 5. Configure direction, count, and ceiling */
	if (write_file("direction_rw", DIRECTION_FORWARD) < 0) {
		fprintf(stderr, "FAIL: Could not set direction to forward\n");
		return -1;
	}
	if (write_file("count", DEFAULT_VAL) < 0) {
		fprintf(stderr, "FAIL: Could not reset count\n");
		return -1;
	}

	if (write_file("ceiling", t->ceiling) < 0) {
		fprintf(stderr, "FAIL: Could not set ceiling to '%s'\n",
			t->ceiling);
		return -1;
	}

	if (read_file("ceiling", buf, sizeof(buf)) < 0) {
		fprintf(stderr, "FAIL: Could not read ceiling value\n");
		return -1;
	}
	strip_newline(buf);
	printf("[Verification] Ceiling reads as: %s\n", buf);

	printf("-> Starting counter...\n");

	/* 6. Enable then start */
	if (write_file("enable", SYSFS_VAL_1) < 0) {
		fprintf(stderr, "FAIL: Could not enable counter\n");
		return -1;
	}
	if (write_file("running", SYSFS_VAL_1) < 0) {
		fprintf(stderr, "FAIL: Could not start counter\n");
		return -1;
	}

	if (monitor_counter() < 0) {
		fprintf(stderr, "FAIL: Counter monitoring failed\n");
		return -1;
	}

	if (read_file("running", buf, sizeof(buf)) < 0) {
		fprintf(stderr, "FAIL: Could not read running status\n");
		return -1;
	}
	strip_newline(buf);
	printf("-> Finished %d seconds. Is it still running? %s (%s)\n",
	       elapsed_seconds, buf,
		strcmp(buf, SYSFS_VAL_1) == 0 ? "1=running" : "0=stopped");
	return 0;
}

int main(int argc, char *argv[])
{
	const struct test_mode tests[] = {
		{
			.mode = "normal",
			.ceiling = UINT32_MAX_STR,
		},
		{
			.mode = "non-recycle",
			.ceiling = CEILING_10M,
		},
		{
			.mode = "modulo-n",
			.ceiling = CEILING_100M,
		},
		{
			.mode = "range limit",
			.ceiling = CEILING_1B,
		},
	};
	char *endptr;
	long channel;
	int failures = 0;
	size_t i;
	int ret;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <channel(0-11)>\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (strcmp("--help", argv[1]) == 0) {
		printf("Usage: %s <channel_number>\n", argv[0]);
		printf("\n");
		printf("channel_number : UTimer channel number (0 - 11)\n");
		return EXIT_SUCCESS;
	}

	/* Validate channel number using strtol for safe parsing */
	errno = 0;
	channel = strtol(argv[1], &endptr, 10);
	if (errno || endptr == argv[1] || *endptr != '\0') {
		fprintf(stderr, "Invalid channel number: '%s'\n", argv[1]);
		return EXIT_FAILURE;
	}

	if (channel < CHANNEL_MIN || channel > CHANNEL_MAX) {
		fprintf(stderr, "Channel number out of range (%d-%d)\n",
			CHANNEL_MIN, CHANNEL_MAX);
		return EXIT_FAILURE;
	}

	channel_num = argv[1];

	ret = snprintf(sysfs_path, sizeof(sysfs_path),
		       "/sys/bus/counter/devices/counter%d/count%ld/",
			COUNTER_DEVICE_NUM, channel);
	if (ret < 0 || (size_t)ret >= sizeof(sysfs_path)) {
		fprintf(stderr, "Error: sysfs base path too long\n");
		return EXIT_FAILURE;
	}

	for (i = 0; i < ARRAY_SIZE(tests); i++) {
		if (test(&tests[i]) < 0)
			failures++;
	}

	/* Cleanup: Reset counter to NORMAL mode and U32_MAX ceiling */
	printf("\n======================================================\n");
	printf("CLEANUP: Resetting to NORMAL mode and max ceiling\n");
	printf("======================================================\n");
	if (write_file("running", SYSFS_VAL_0) < 0)
		fprintf(stderr,
			"WARNING: Could not stop counter during cleanup\n");
	if (write_file("enable", SYSFS_VAL_0) < 0)
		fprintf(stderr,
			"WARNING: Could not disable counter during cleanup\n");
	if (write_file("count_mode", DEFAULT_MODE) < 0)
		fprintf(stderr,
			"WARNING: Could not reset count_mode during cleanup\n");
	if (write_file("ceiling", UINT32_MAX_STR) < 0)
		fprintf(stderr,
			"WARNING: Could not reset ceiling during cleanup\n");

	if (failures > 0) {
		fprintf(stderr, "\n%d out of %zu tests FAILED\n",
			failures, ARRAY_SIZE(tests));
		return EXIT_FAILURE;
	}

	printf("\nAll %zu tests PASSED\n", ARRAY_SIZE(tests));
	return EXIT_SUCCESS;
}
