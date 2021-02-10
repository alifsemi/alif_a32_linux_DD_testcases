/*
 *
 * Copyright (c) 2019, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 * File containing various unit tests for the Corstone-700 test platform.
 * The contents of this file utilize handling of the transmitted commands
 * on respectively the ES and SE sides.
 * Test verification is not performed by this program. Instead, all test
 * variables are printed to the console, which will then be used to externally
 * verify the test.
 * Tests may be selected by providing the test index as an argument of this
 * program.
*/

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#define HWSEM_NUM 16
#define HWSEM_FILE_NAME_LEN 13

enum test_apps
{
	INVALID,
	HWSEM0_TEST,
};

enum hwsem_command
{

	HWSEM_INVALID = 0,
	HWSEM_RD_MASTER_ID,
	HWSEM_RD_COUNT = 3,
	HWSEM_ACQUIRE,
	HWSEM_RELEASE,
	HWSEM_RESET
};

extern int errno;
char str[HWSEM_FILE_NAME_LEN];

int hwsem_test()
{
	int i, status = -1;
	int fd = -1;

	for (i = 0 ; i < HWSEM_NUM ; ++i)
	{
		printf("===== Staring HWSEM %d Test ===== \n", i);
		sprintf(str, "/dev/hwsem%d", i);

		fd = open(str, O_RDWR);
		if (fd == -1)
		{
			printf("Failed to open /dev/hwsem%d\n",i);
			return -1;
		}

		printf("Current MASTER ID = 0x%x \n", ioctl(fd, HWSEM_RD_MASTER_ID, NULL));
		usleep(100);
		printf("Current count is = 0x%x \n", ioctl(fd, HWSEM_RD_COUNT, NULL));
		usleep(100);
		if (ioctl(fd, HWSEM_ACQUIRE, NULL))
			printf("FAILED to acquire HWSEM%d\n", i);
		else
			printf("Acquiring HWSEM%d is successful\n", i);
		usleep(100);
		printf("Latest count is = 0x%x \n", ioctl(fd, HWSEM_RD_COUNT, NULL));
		usleep(100);
		if (ioctl(fd, HWSEM_RELEASE, NULL))
			printf("FAILED to release HWSEM%d \n", i);
		else
			printf("Release of HWSEM%d is successful\n", i);
		usleep(100);
		printf("Latest count is = 0x%x \n", ioctl(fd, HWSEM_RD_COUNT, NULL));
		usleep(100);
		if (ioctl(fd, HWSEM_ACQUIRE, NULL))
			printf("FAILED to acquire HWSEM%d\n", i);
		else
			printf("Acquiring HWSEM%d is successful\n", i);
		usleep(100);
		printf("Latest count is = 0x%x \n", ioctl(fd, HWSEM_RD_COUNT, NULL));
		usleep(100);
		printf("Resetting the HWSEM%d, this generates an interrupt "
                "and the IRQ writes MASTER ID to register offset 0x0, which "
                "results in count of 1\n", i);
                if(ioctl(fd, HWSEM_RESET, NULL))
			printf("FAILED to reset of HWSEM%d\n",i);
		else
			printf("Reset of HWSEM%d is successful\n",i);
		usleep(100);
		printf("Latest count is = 0x%x \n", ioctl(fd, HWSEM_RD_COUNT, NULL));
		usleep(100);
		close(fd);
	}
	return status;
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: ./test-app_hwsem0 <test app number>\n");
		printf("  test app number : 1  Test HWSEM 0\n");

		return 1;
	}

	switch (atoi(argv[1]))
	{
	case INVALID:
		printf("Invalid test app specified\n");
		break;
	case HWSEM0_TEST:
		return hwsem_test();
                break;
	}

	return 0;
}
