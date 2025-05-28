#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <stdint.h>
#include <getopt.h>
#define MAX_CHANNELS		12
#define MAX_ITERATIONS		5
#define BUFFER_SIZE		2048
#define STATUS_DELAY_SEC	1
char buffer[BUFFER_SIZE];
struct channel_config {
	const char *name;
	const char *path;
	unsigned long ceiling_up;
	unsigned long ceiling_down;
	unsigned long initial_count_down;
};
static const struct channel_config channels[MAX_CHANNELS] = {
	{"count0", "/sys/bus/counter/devices/counter0/count0", 200000000, 400000000, 400000000},
	{"count1", "/sys/bus/counter/devices/counter0/count1", 200000000, 400000000, 400000000},
	{"count2", "/sys/bus/counter/devices/counter0/count2", 200000000, 400000000, 400000000},
	{"count3", "/sys/bus/counter/devices/counter0/count3", 200000000, 400000000, 400000000},
	{"count4", "/sys/bus/counter/devices/counter0/count4", 200000000, 400000000, 400000000},
	{"count5", "/sys/bus/counter/devices/counter0/count5", 200000000, 400000000, 400000000},
	{"count6", "/sys/bus/counter/devices/counter0/count6", 200000000, 400000000, 400000000},
	{"count7", "/sys/bus/counter/devices/counter0/count7", 200000000, 400000000, 400000000},
	{"count8", "/sys/bus/counter/devices/counter0/count8", 200000000, 400000000, 400000000},
	{"count9", "/sys/bus/counter/devices/counter0/count9", 200000000, 400000000, 400000000},
	{"count10", "/sys/bus/counter/devices/counter0/count10", 200000000, 400000000, 400000000},
	{"count11", "/sys/bus/counter/devices/counter0/count11", 200000000, 400000000, 400000000}
};

void write_to_file(const char *path, const char *value)
{
	int fd = open(path, O_WRONLY);

	if (fd < 0) {
		perror("Error opening file for writing");
		return;
	}
	if (write(fd, value, strlen(value)) < 0)
		perror("Error writing to file");
	close(fd);
}

int read_counter_status(const char *path)
{
	printf("\n%s\n", path);
	fflush(stdout);
	/*Small delay 10 ms*/
	usleep(10000);
	int fd = open(path, O_RDONLY);

	if (fd < 0) {
		perror("Error opening file for reading");
		return -1;
	}
	ssize_t bytes_read = read(fd, buffer, BUFFER_SIZE - 1);

	if (bytes_read < 0) {
		perror("Error reading from file");
		close(fd);
		return -1;
	}
	buffer[bytes_read] = '\0';
	printf("%s", buffer);
	close(fd);
	return 0;
}

void read_from_file(const char *path)
{
	int fd = open(path, O_RDONLY);

	if (fd < 0) {
		perror("Error opening file for reading");
		return;
	}
	ssize_t bytes_read = read(fd, buffer, BUFFER_SIZE - 1);

	if (bytes_read < 0)
		perror("Error reading from file");
	else {
		buffer[bytes_read] = '\0';
		printf("%s: %s", path, buffer);
	}
	close(fd);
}

void print_utimer_info(void)
{
	printf("\n=== UTIMER Basic Information ===\n");
	int fd;

	/*Utimer Name*/
	printf("1. Utimer Name\n");
	fd = open("/sys/bus/counter/devices/counter0/name", O_RDONLY);
	if (fd < 0) {
		perror("Error");
		exit(1);
	}
	read(fd, buffer, sizeof(buffer));
	printf("Utimer name= %s\n", buffer);
	sleep(1);
	close(fd);
	memset(buffer, 0, BUFFER_SIZE);

	/*Utimer channel count*/
	printf("2. Utimer Channel Count\n");
	fd = open("/sys/bus/counter/devices/counter0/num_counts", O_RDONLY);
	if (fd < 0) {
		perror("Error");
		exit(1);
	}
	read(fd, buffer, sizeof(buffer));
	printf("Utimer num_counts= %s\n", buffer);
	sleep(1);
	close(fd);
	memset(buffer, 0, BUFFER_SIZE);

	/*Utimer signal count*/
	printf("3. Utimer Signal Count\n");
	fd = open("/sys/bus/counter/devices/counter0/num_signals", O_RDONLY);
	if (fd < 0) {
		perror("Error");
		exit(1);
	}
	read(fd, buffer, sizeof(buffer));
	printf("Utimer num_signals= %s\n", buffer);
	sleep(1);
	close(fd);
	memset(buffer, 0, BUFFER_SIZE);
}

void print_channel_info(int channel)
{
	char path[256];
	int fd;

	printf("\n=== Channel %d Information ===\n", channel);
	/*Channel name*/
	snprintf(path, sizeof(path),
			"/sys/bus/counter/devices/counter0/count%d/name", channel);
	printf("1. Channel Name\n");
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("Error");
		exit(1);
	}
	read(fd, buffer, sizeof(buffer));
	printf("Channel name= %s\n", buffer);
	sleep(1);
	close(fd);
	memset(buffer, 0, BUFFER_SIZE);

	/*Channel function*/
	snprintf(path, sizeof(path),
			"/sys/bus/counter/devices/counter0/count%d/function", channel);
	printf("2. Channel Function\n");
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("Error");
		exit(1);
	}
	read(fd, buffer, sizeof(buffer));
	printf("Channel function= %s\n", buffer);
	sleep(1);
	close(fd);
	memset(buffer, 0, BUFFER_SIZE);

	/*Channel count mode*/
	snprintf(path, sizeof(path),
			"/sys/bus/counter/devices/counter0/count%d/count_mode", channel);
	printf("3. Channel Count Mode\n");
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("Error");
		exit(1);
	}
	read(fd, buffer, sizeof(buffer));
	printf("Channel count_mode= %s\n", buffer);
	sleep(1);
	close(fd);
	memset(buffer, 0, BUFFER_SIZE);

	/*Channel direction*/
	snprintf(path, sizeof(path),
			"/sys/bus/counter/devices/counter0/count%d/direction_rw", channel);
	printf("4. Channel Direction\n");
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("Error");
		exit(1);
	}
	read(fd, buffer, sizeof(buffer));
	printf("Channel direction= %s\n", buffer);
	sleep(1);
	close(fd);
	memset(buffer, 0, BUFFER_SIZE);
	/*Signal0_action*/
	snprintf(path, sizeof(path),
			"/sys/bus/counter/devices/counter0/count%d/signal0_action", channel);
	printf("5. Counter Signal Action\n");
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("Error");
		exit(1);
	}
	read(fd, buffer, sizeof(buffer));
	printf("Counter Signal Action= %s\n", buffer);
	sleep(1);
	close(fd);
	memset(buffer, 0, BUFFER_SIZE);
}

void test_up_counter(int channel)
{
	if (channel < 0 || channel >= MAX_CHANNELS) {
		printf("Invalid channel number: %d\n", channel);
		return;
	}

	const struct channel_config *cfg = &channels[channel];
	char path[256];

	printf("\n=== Testing UP COUNTER on channel %d ===\n", channel);

	/*Set ceiling*/
	snprintf(path, sizeof(path), "%s/ceiling", cfg->path);
	snprintf(buffer, sizeof(buffer), "%lu", cfg->ceiling_up);
	write_to_file(path, buffer);
	read_from_file(path);

	/*Enable counter*/
	snprintf(path, sizeof(path), "%s/enable", cfg->path);
	write_to_file(path, "1");
	read_from_file(path);

	/*Set direction to up (0)*/
	snprintf(path, sizeof(path), "%s/direction_rw", cfg->path);
	write_to_file(path, "0");
	read_from_file(path);

	/*Set initial count*/
	snprintf(path, sizeof(path), "%s/count", cfg->path);
	write_to_file(path, "0");
	read_from_file(path);

	/*Start running*/
	snprintf(path, sizeof(path), "%s/running", cfg->path);
	write_to_file(path, "1");
	read_from_file(path);
	sleep(STATUS_DELAY_SEC);

	/*Read status*/
	snprintf(path, sizeof(path), "%s/counter_status", cfg->path);
	for (int i = 0; i < MAX_ITERATIONS; i++) {
		read_counter_status(path);
		sleep(1);
	};

	/*Read count values in a loop*/
	snprintf(path, sizeof(path), "%s/count", cfg->path);
	for (int i = 0; i < MAX_ITERATIONS; i++) {
		read_from_file(path);
		sleep(1);
	}

	/*Stop running*/
	snprintf(path, sizeof(path), "%s/running", cfg->path);
	write_to_file(path, "0");
	read_from_file(path);

	/*Disable counter*/
	snprintf(path, sizeof(path), "%s/enable", cfg->path);
	write_to_file(path, "0");
	read_from_file(path);

	snprintf(path, sizeof(path), "%s/count", cfg->path);
	for (int i = 0; i < MAX_ITERATIONS; i++) {
		read_from_file(path);
		sleep(1);
	}
}

void test_down_counter(int channel)
{
	if (channel < 0 || channel >= MAX_CHANNELS) {
		printf("Invalid channel number: %d\n", channel);
		return;
	}
	const struct channel_config *cfg = &channels[channel];
	char path[256];

	printf("\n=== Testing DOWN COUNTER on channel %d ===\n", channel);
	/*Set ceiling*/
	snprintf(path, sizeof(path), "%s/ceiling", cfg->path);
	snprintf(buffer, sizeof(buffer), "%lu", cfg->ceiling_down);
	write_to_file(path, buffer);
	read_from_file(path);

	/*Enable counter*/
	snprintf(path, sizeof(path), "%s/enable", cfg->path);
	write_to_file(path, "1");
	read_from_file(path);

	/*Set direction to down (1)*/
	snprintf(path, sizeof(path), "%s/direction_rw", cfg->path);
	write_to_file(path, "1");
	read_from_file(path);

	/*Set initial count*/
	snprintf(path, sizeof(path), "%s/count", cfg->path);
	snprintf(buffer, sizeof(buffer), "%lu", cfg->initial_count_down);
	write_to_file(path, buffer);
	read_from_file(path);

	/*Start running*/
	snprintf(path, sizeof(path), "%s/running", cfg->path);
	write_to_file(path, "1");
	read_from_file(path);
	sleep(STATUS_DELAY_SEC);

	/*Read status*/
	snprintf(path, sizeof(path), "%s/counter_status", cfg->path);
	for (int i = 0; i < MAX_ITERATIONS; i++) {
		read_counter_status(path);
		sleep(1);
	};

	/*Read count values in a loop*/
	snprintf(path, sizeof(path), "%s/count", cfg->path);
	for (int i = 0; i < MAX_ITERATIONS; i++) {
		read_from_file(path);
		sleep(1);
	}

	/*Stop running*/
	snprintf(path, sizeof(path), "%s/running", cfg->path);
	write_to_file(path, "0");
	read_from_file(path);

	/*Disable counter*/
	snprintf(path, sizeof(path), "%s/enable", cfg->path);
	write_to_file(path, "0");
	read_from_file(path);

	snprintf(path, sizeof(path), "%s/count", cfg->path);
	for (int i = 0; i < MAX_ITERATIONS; i++) {
		read_from_file(path);
		sleep(1);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s -[a-p]\n", argv[0]);
		fprintf(stderr, "Options:\n");
		for (int i = 0; i < MAX_CHANNELS; i++)
			fprintf(stderr, "  -%c  Test Channel %d\n", 'a' + i, i);
	return 1;
	}
	char option = argv[1][1];
	int channel = -1;

	if (option >= 'a' && option <= 'p')
		channel = option - 'a';
	else {
		fprintf(stderr, "Invalid option: %s\n", argv[1]);
		return 1;
	}
	/*Print UTIMER and channel information first*/
	print_utimer_info();
	print_channel_info(channel);
	/*Run Counter Test*/
	test_up_counter(channel);
	test_down_counter(channel);
	return 0;
}
