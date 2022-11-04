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
#include <string.h>
#include <sys/mman.h>
#include "services_lib_a32.h"
#include "services_lib_rpmsg.h"

extern int errno;

int fd, fd_semhu0_ept, mem_fd;
uint32_t map_base, mapped_size;
void *virt_addr;

int rpmsg_init() {
   int status;
   fd = open(RPMSG_CTRL, O_RDWR);
   if(fd == -1) {
      perror("rpmsg_init:open:/dev/rpmsg_ctrl0");
      exit(1);
   }

   status = ioctl(fd, RPMSG_CREATE_EPT_IOCTL, &semhu0_eptinfo);
   if (status == -1) {
      perror("rpmsg_init:ioctl:create rpmsg");
      exit(1);
   }

   /* Create Endpoint to receive/send MHU data */
   fd_semhu0_ept = open(RPMSG_MHU_PNT, O_RDWR);
   if (fd_semhu0_ept == -1) {
      perror("rpmsg_init:open:/dev/rpmsg0");
      exit(1);
   }
   return 0;
}


void map_mem() {
   uint32_t page_size, offset_in_page;
   mem_fd = open(MEM_FILE, (O_RDWR | O_SYNC),0666);
   if(mem_fd == -1) {
      perror("map_mem:open:/dev/mem");
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

   //mlock(map_base,mapped_size);
   printf("Memory mapped at address %p.\n", map_base);
   printf("Memory mapped at address %x %x %x\n", page_size, MHU_SERVICES_DATA_LOC, offset_in_page);
   virt_addr = (char*)map_base + offset_in_page;
}

void rpmsg_get_heartbeat() {
   service_get_heartbeat(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);
}

void rpmsg_get_rnd_num() {
   service_get_rnd_num(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);
}

void rpmsg_get_lcs() {
   service_get_lcs(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);
}

void rpmsg_get_toc_version() {
   service_get_toc_version(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);
}

void rpmsg_get_toc_number() {
   service_get_toc_number(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);
}

void rpmsg_uart_write() {
   service_uart_write(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);
}


void mem_destroy() {
   if (munmap(map_base, mapped_size) == -1) {
      perror("mem_destroy:munmap");
   }
   close(mem_fd);
}

void rpmsg_destroy() {
   int status;
   status = ioctl(fd_semhu0_ept, RPMSG_DESTROY_EPT_IOCTL);
   if (status == -1) {
      perror("mem_destroy:ioctl:destroy rpmsg");
   }
   close(fd_semhu0_ept);
   close(fd);
   printf("Closed opened files.\n");
}
