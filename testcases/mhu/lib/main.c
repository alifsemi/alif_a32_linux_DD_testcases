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
#include <string.h>
#include <sys/mman.h>
#include "services_lib_a32.h"


/**
 * struct rpmsg_endpoint_info - endpoint info representation
 * @name: name of service
 * @src: local address
 * @dst: destination address
 */
struct rpmsg_endpoint_info
{
    char name[32];
    u_int32_t src;
    u_int32_t dst;
};

#define RPMSG_CREATE_EPT_IOCTL _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)
#define RPMSG_DESTROY_EPT_IOCTL _IO(0xb5, 0x2)
#define MHU_SERVICES_DATA_LOC 0x0827F000


struct rpmsg_endpoint_info semhu0_eptinfo = {"se_mhu0", 0XFFFFFFFF, 0xFFFFFFFF};
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
int fd, fd_semhu0_ept, mem_fd;
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
    uint32_t page_size, mapped_size, offset_in_page;
    void *virt_addr;
    int status;

    printf("MHU0 TEST BETWEEN A32 and SE cores for RND\n");
    printf("==========================================\n");
    fd = open("/dev/rpmsg_ctrl0", O_RDWR);
    if(fd == -1) {
       perror("open:/dev/rpmsg_ctrl0");
       exit(1);
    }

      status = ioctl(fd, RPMSG_CREATE_EPT_IOCTL, &semhu0_eptinfo);
      if (status == -1) {
          perror("ioctl:create rpmsg");
          exit(1);
      }

     /* Create Endpoint to receive/send MHU data */
     fd_semhu0_ept = open("/dev/rpmsg0", O_RDWR);
     if (fd_semhu0_ept == -1) {
         perror("open:/dev/rpmsg0");
         exit(1);
     }

    /*Register signal handler */
    signal(SIGINT, sigintHandler);


    /* Open /dev/mem file and map MHU_SERVICES_DATA_LOC
     * to virtual address to get MHU services from SE */
    mem_fd = open("/dev/mem", (O_RDWR | O_SYNC),0666);
    if(mem_fd == -1) {
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

    //mlock(map_base,mapped_size);
    printf("Memory mapped at address %p.\n", map_base);
    printf("Memory mapped at address %x %x %x\n", page_size, MHU_SERVICES_DATA_LOC, offset_in_page);
    virt_addr = (char*)map_base + offset_in_page;
    printf("base address contents %x\n", *(volatile uint32_t *)virt_addr);
    //all_services_svc_t *ptr = virt_addr;
    //printf("base + get_rnd_svc_t_v %p\n", &(ptr->get_rnd_svc_t_v));

    service_get_heartbeat(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);
    service_get_rnd_num(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);
    service_get_lcs(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);
    service_get_toc_version(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);
    service_get_toc_number(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);
    service_uart_write(fd_semhu0_ept, virt_addr, MHU_SERVICES_DATA_LOC);

    status = ioctl(fd_semhu0_ept, RPMSG_DESTROY_EPT_IOCTL);
    if (status == -1) {
        perror("ioctl:destroy rpmsg");
    }
    close(fd_semhu0_ept);
    if (munmap(map_base, mapped_size) == -1) {
        perror("munmap");
    }
    close(mem_fd);
    close(fd);
    printf("Closed opened files \n");
    exit(0);
}
