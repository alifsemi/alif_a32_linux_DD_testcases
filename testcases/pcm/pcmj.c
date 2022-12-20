/* Copyright (C) 2022 Alif Semiconductor - All Rights Reserved.
 * Use, distribution and modification of this code is permitted under the
 * terms stated in the Alif Semiconductor Software License Agreement
 *
 * You should have received a copy of the Alif Semiconductor Software
 * License Agreement with this file. If not, please write to:
 * contact@alifsemi.com, or visit: https://alifsemi.com/license
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define DEVICE_NAME "/dev/alif-pcm"
#define MAX_SIZE 90

#define OPTION_START_RECORD (1)
#define OPTION_STOP_RECORD (2)
#define OPTION_READ_DATA (3)

struct data_items {
	unsigned int audio_out[4];
};

struct data_items global_buf[MAX_SIZE];

void print_data(struct data_items *buf, int n)
{
	int i;

	for(i=0; i<n; i++) {
		printf("%x %x %x %x\n", buf[i].audio_out[0], buf[i].audio_out[1], buf[i].audio_out[2], buf[i].audio_out[3]);
	}

	return;
}

int parse_input(int argc, char *argv[])
{
	int option;

	if(argc < 2) {
		printf("%s <start | stop | read>\n", argv[0]);
		return -1;
	}

	if(strcmp(argv[1], "start") == 0) {
		option = OPTION_START_RECORD;
	} else if(strcmp(argv[1], "stop") == 0) {
		option = OPTION_STOP_RECORD;
	} else if(strcmp(argv[1], "read") == 0) {
		option = OPTION_READ_DATA;
	} else {
		printf("please give valid option\n");
		return -1;
	}

	return option;

}

int main(int argc, char *argv[])
{
	int fd;
	int num_units;
	int option;
	int data;
	int n_bytes;

	option = parse_input(argc, argv);

	if(option <= 0) {
		return 0;
	}

	fd = open(DEVICE_NAME, O_RDWR);

	if (fd < 0) {
		perror("open failed");
		return 0;
	}

	if(option == OPTION_READ_DATA) {

		num_units = MAX_SIZE;

		n_bytes = read(fd, global_buf, num_units * sizeof(struct data_items));

		if (n_bytes < 0) {
			perror("read");
			return 0;
		}

		if(n_bytes == 0) {
			printf("returned 0 bytes\n");
			return 0;
		}

		print_data(global_buf, n_bytes / sizeof(struct data_items));

	} else {

		if(option == OPTION_START_RECORD)
			data = 1; //start recording
		else
			data = 0;

		n_bytes = write(fd, &data, sizeof(data));

		if(n_bytes < 0) {
			perror("write");
		}
	}

out:
	close(fd);

	return 0;
}
