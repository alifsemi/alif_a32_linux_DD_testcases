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
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/mman.h>
#include "services_lib_a32.h"
#include "services_lib_rpmsg.h"
#include <linux/ioctl.h>

#define RPMSG_IOCTL_MAGIC  'k'
#define RPMSG_IOCTL_SETVAL _IOW(RPMSG_IOCTL_MAGIC, 1, unsigned int)
#define RPMSG_IOCTL_GETVAL _IOR(RPMSG_IOCTL_MAGIC, 2, unsigned int)
#define RPMSG_IOCTL_MAXNR 2

void rpmsg_set_read(uint32_t rec_phy_addr, uint32_t *rec_phy_addr_val)
{
	int fd, status, ret;
	uint32_t value;
	fd = open("/dev/rpmsg_device", O_RDWR);
	// Set value
	value = rec_phy_addr;
	ret = ioctl(fd, RPMSG_IOCTL_SETVAL, &value);
	if (ret < 0) {
		perror("SET_VALUE failed");
		close(fd);
		return EXIT_FAILURE;
	}
	// Get value
	ret = ioctl(fd, RPMSG_IOCTL_GETVAL, &value);
	if (ret < 0) {
		perror("GET_VALUE failed");
		close(fd);
		return EXIT_FAILURE;
	}
	*rec_phy_addr_val = value;
}

void service_get_heartbeat(service_header_t *ptr, int offset)
{
	int status, ret;
	uint32_t snd_phy_addr, rec_phy_addr, rec_phy_addr_val;
	*(volatile uint16_t *)&(ptr->send_service_id) = SERVICE_MAINTENANCE_HEARTBEAT_ID;
	snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;

	status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
	if (status == -1) {
		perror("write: heartbeat:");
		exit(1);
	}

	status = read(fd_semhu1_ept, &rec_phy_addr, sizeof(rec_phy_addr));
	if (status == -1) {
		perror("read: heartbeat:");
		exit(1);
	}

	rpmsg_set_read(rec_phy_addr, &rec_phy_addr_val);

	if (rec_phy_addr_val == snd_phy_addr) {
		printf("The heartbeat test is done\n");
	} else {
		printf("The heartbeat test is failed\n");
	}
}

void service_get_rnd_num(get_rnd_svc_t *ptr, int offset)
{
	int i, status;
	uint32_t snd_phy_addr, rec_phy_addr, rec_phy_addr_val;
	*(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_CRYPTOCELL_GET_RND;
	*(volatile uint32_t *)&(ptr->send_rnd_length) = 8;
	/* Request RND by sending physical address of get_rnd_svc_t */
	snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;
	status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
	if (status == -1) {
		perror("write: rnd:");
		exit(1);
	}
	status = read(fd_semhu1_ept, &rec_phy_addr, sizeof(rec_phy_addr));
	if (status == -1) {
		perror("read: rnd:");
		exit(1);
	}

	rpmsg_set_read(rec_phy_addr, &rec_phy_addr_val);

	if (rec_phy_addr_val == snd_phy_addr) {
		printf("The RND values are: ");
		for (i = 0; i < ptr->send_rnd_length; ++i)
			printf("0x%02x ", (volatile uint8_t)ptr->resp_rnd[i]);
		printf("\n");
	} else {
		printf("The RND value is failed\n");
	}
}

void service_get_lcs(get_lcs_svc_t *ptr, int offset)
{
	int status;
	uint32_t snd_phy_addr, rec_phy_addr, rec_phy_addr_val;
	*(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_CRYPTOCELL_GET_LCS;
	/* Request LCS by sending physical address of get_lcs_svc_t */
	snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;
	status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
	if (status == -1) {
		perror("write: lcs:");
		exit(1);
	}
	status = read(fd_semhu1_ept, &rec_phy_addr, sizeof(rec_phy_addr));
	if (status == -1) {
		perror("read: lcs:");
		exit(1);
	}

	rpmsg_set_read(rec_phy_addr, &rec_phy_addr_val);

	if (rec_phy_addr_val == snd_phy_addr) {
		printf("The LCS response is: 0x%x\n", ptr->resp_lcs);
	} else {
		printf("The LCS  is failed\n");
	}
}
void service_get_toc_version(get_toc_version_svc_t *ptr, int offset)
{
	int status;
	uint32_t snd_phy_addr, rec_phy_addr, rec_phy_addr_val;
	*(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_SYSTEM_MGMT_GET_TOC_VERSION;
	/* Request TOC version by sending physical address of get_toc_version_svc_t */
	snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;
	status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
	if (status == -1) {
		perror("write: toc version:");
		exit(1);
	}
	status = read(fd_semhu1_ept, &rec_phy_addr, sizeof(rec_phy_addr));
	if (status == -1) {
		perror("read: toc version:");
		exit(1);
	}

	rpmsg_set_read(rec_phy_addr, &rec_phy_addr_val);

	if (rec_phy_addr_val == snd_phy_addr)
		printf("The TOC version is %d\n", ptr->resp_version);
	else
		printf("The TOC version is failed\n");
}
void service_get_toc_number(get_toc_number_svc_t *ptr, int offset)
{
	int status;
	uint32_t snd_phy_addr, rec_phy_addr, rec_phy_addr_val;
	*(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_SYSTEM_MGMT_GET_TOC_NUMBER;
	/* Request TOC number by sending physical address of get_toc_number_svc_t */
	snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;
	status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
	if (status == -1) {
		perror("write: toc number:");
		exit(1);
	}
	status = read(fd_semhu1_ept, &rec_phy_addr, sizeof(rec_phy_addr));
	if (status == -1) {
		perror("read: toc number:");
		exit(1);
	}

	rpmsg_set_read(rec_phy_addr, &rec_phy_addr_val);

	if (rec_phy_addr_val == snd_phy_addr) {
		printf("The TOC number is %d\n", ptr->resp_number_of_toc);
	} else {
		printf("The TOC number is failed\n");
	}
}

void service_uart_write(uart_write_svc_t *ptr, int offset)
{
	int status;
	uint32_t snd_phy_addr, rec_phy_addr, rec_phy_addr_val;
	char message[256] = "Hello from A32\n";
	*(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_APPLICATION_UART_WRITE_ID;
	/* Request TOC version by sending physical address of get_toc_version_svc_t */
	snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;
	memcpy((volatile char *)&(ptr->string_contents), message, sizeof(message));
	status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
	if (status == -1) {
		perror("write: uart write:");
		exit(1);
	}
	status = read(fd_semhu1_ept, &rec_phy_addr, sizeof(rec_phy_addr));
	if (status == -1) {
		perror("read: uart write:");
		exit(1);
	}

	rpmsg_set_read(rec_phy_addr, &rec_phy_addr_val);

	if (rec_phy_addr_val == snd_phy_addr)
		printf("The UART write is successful\n");
	else
		printf("The UART write is failed\n");
}
