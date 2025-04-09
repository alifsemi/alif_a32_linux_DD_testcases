/* Copyright (C) 2022 Alif Semiconductor - All Rights Reserved.
 * Use, distribution and modification of this code is permitted under the
 * terms stated in the Alif Semiconductor Software License Agreement
 *
 * You should have received a copy of the Alif Semiconductor Software
 * License Agreement with this file. If not, please write to:
 * contact@alifsemi.com, or visit: https://alifsemi.com/license
 *
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
#include <pthread.h>
#include <signal.h>
#include "services_lib_a32.h"
#include "services_lib_rpmsg.h"
#include <string.h>
#include <sys/mman.h>

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
int fd, mem_fd;
uint32_t mapped_size;
void *map_base;
extern int errno;

void sigintHandler(int sig_num)
{
	int status;
	status = ioctl(fd_semhu0_ept, RPMSG_DESTROY_EPT_IOCTL);
	if (status == -1) {
		printf("Unable to destroy fd_semhu0_ept endpoint correctly \n");
	}
	close(fd_semhu0_ept);
	if (munmap(map_base, mapped_size) == -1) {
		printf("Unable to unmap the mapped address region \n");
	}
	close(mem_fd);
	close(fd);
	printf("Closed opened files \n");
	exit(0);
}

int main()
{
	uint32_t page_size, offset_in_page;
	void *virt_addr;
	int status;

	printf("MHU0 TEST BETWEEN A32 and SE cores for RND\n");
	printf("==========================================\n");
	printf("Size of struct all_services_svc_t = %d\n", sizeof(all_services_svc_t));
	printf("Size of union all_services_svc_t = %d\n", sizeof(all_services_svc_union_t));
	fd = open("/dev/rpmsg_ctrl0", O_RDWR);
	if (fd == -1) {
		perror("open:/dev/rpmsg_ctrl0");
		exit(1);
	}

	status = ioctl(fd, RPMSG_CREATE_EPT_IOCTL, &semhu0_eptinfo);
	if (status == -1) {
		perror("ioctl:create rpmsg");
		exit(1);
	}
	status = ioctl(fd, RPMSG_CREATE_EPT_IOCTL, &semhu1_eptinfo);
	if (status == -1) {
		perror("ioctl:create rpmsg");
		exit(1);
	}

	/* Create Endpoint to send MHU data */
	fd_semhu0_ept = open("/dev/rpmsg0", O_RDWR);
	if (fd_semhu0_ept == -1) {
		perror("open:/dev/rpmsg0");
		exit(1);
	}
	/* Create Endpoint to received MHU data */
	fd_semhu1_ept = open("/dev/rpmsg1", O_RDWR);
	if (fd_semhu1_ept == -1) {
		perror("open:/dev/rpmsg1");
		exit(1);
	}

	/*Register signal handler */
	signal(SIGINT, sigintHandler);


	/* Open /dev/mem file and map MHU_SERVICES_DATA_LOC
	 * to virtual address to get MHU services from SE
	*/
	mem_fd = open("/dev/mem", (O_RDWR | O_SYNC), 0666);
	if (mem_fd == -1) {
		perror("open:/dev/mem");
		exit(1);
	}
	page_size = getpagesize();
	/* Map two pages as services structure can go beyond the size
	 * of the page. */
	mapped_size = page_size * 2;

	map_base = mmap(NULL,
			mapped_size,
			(PROT_READ | PROT_WRITE),
			(MAP_SHARED),
			mem_fd,
			MHU_SERVICES_DATA_LOC & ~(off_t)(page_size - 1));
	if (map_base == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	offset_in_page = (uint32_t) MHU_SERVICES_DATA_LOC & (page_size - 1);

	printf("Memory mapped at address %p.\n", map_base);
	printf("Memory mapped at address %x %x %x\n", page_size, MHU_SERVICES_DATA_LOC, offset_in_page);
	virt_addr = (char *)map_base + offset_in_page;
	all_services_svc_t *ptr = virt_addr;

	service_get_heartbeat(&(ptr->service_header_t_v),
				((char *)&(ptr->service_header_t_v) - (char *)ptr));
	service_get_rnd_num(&(ptr->get_rnd_svc_t_v),
				((char *)&(ptr->get_rnd_svc_t_v) - (char *)ptr));
	service_get_lcs(&(ptr->get_lcs_svc_t_v),
				((char *)&(ptr->get_lcs_svc_t_v) - (char *)ptr));
	service_get_toc_version(&(ptr->get_toc_version_svc_t_v),
				((char *)&(ptr->get_toc_version_svc_t_v) - (char *)ptr));
	service_get_toc_number(&(ptr->get_toc_number_svc_t_v),
				((char *)&(ptr->get_toc_number_svc_t_v) - (char *)ptr));
	service_uart_write(&(ptr->uart_write_svc_t_v),
				((char *)&(ptr->uart_write_svc_t_v) - (char *)ptr));
	status = ioctl(fd_semhu0_ept, RPMSG_DESTROY_EPT_IOCTL);
	if (status == -1) {
		perror("ioctl:destroy rpmsg0");
	}
	status = ioctl(fd_semhu1_ept, RPMSG_DESTROY_EPT_IOCTL);
	if (status == -1) {
		perror("ioctl:destroy rpmsg1");
	}
	close(fd_semhu0_ept);
	close(fd_semhu1_ept);
	if (munmap(map_base, mapped_size) == -1) {
		perror("munmap");
	}
	close(mem_fd);
	close(fd);
	printf("Closed opened files\n");
	exit(0);
}
