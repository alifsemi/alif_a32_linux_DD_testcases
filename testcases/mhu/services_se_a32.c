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
#include "services_lib_public.h"
#include "services_lib_protocol.h"
#include <string.h>
#include <sys/mman.h>

/* Collective structure with all the services */
typedef struct {
    service_header_t service_header_t_v;
    get_rnd_svc_t get_rnd_svc_t_v;
    get_lcs_svc_t get_lcs_svc_t_v;
    mbedtls_crypto_init_svc_t mbedtls_crypto_init_svc_t_v;
    mbedtls_aes_set_key_svc_t mbedtls_aes_set_key_svc_t_v;
    mbedtls_aes_crypt_svc_t mbedtls_aes_crypt_svc_t_v;
    mbedtls_sha_svc_t mbedtls_sha_svc_t_v;
    mbedtls_ccm_set_key_svc_t mbedtls_ccm_set_key_svc_t_v;
    mbedtls_chacha20_crypt_svc_t mbedtls_chacha20_crypt_svc_t_v;
    mbedtls_chachapoly_crypt_svc_t mbedtls_chachapoly_crypt_svc_t_v;
    mbedtls_poly1305_crypt_svc_t mbedtls_poly1305_crypt_svc_t_v;
    mbedtls_trng_hardware_poll_svc_t mbedtls_trng_hardware_poll_svc_t_v;
    process_toc_entry_svc_t process_toc_entry_svc_t_v;
    boot_cpu_svc_t boot_cpu_svc_t_v;
    control_cpu_svc_t control_cpu_svc_t_v;
    pinmux_svc_t pinmux_svc_t_v;
    pad_control_svc_t pad_control_svc_t_v;
    uart_write_svc_t uart_write_svc_t_v;
    get_toc_version_svc_t get_toc_version_svc_t_v;
    get_toc_number_svc_t get_toc_number_svc_t_v;
    get_toc_via_name_svc_t get_toc_via_name_svc_t_v;
    get_toc_via_cpu_id_svc_t get_toc_via_cpu_id_svc_t_v;
    get_device_part_svc_t get_device_part_svc_t_v;
    set_services_capabilities_t set_services_capabilities_t_v;
    stop_mode_request_svc_t stop_mode_request_svc_t_v;
} all_services_svc_t;


void service_get_heartbeat(service_header_t *, int );
void service_get_rnd_num(get_rnd_svc_t *, int );
void service_get_lcs(get_lcs_svc_t *, int );
void service_get_toc_version(get_toc_version_svc_t *,int);
void service_get_toc_number(get_toc_number_svc_t *,int);
void service_uart_write(uart_write_svc_t *, int );

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
    all_services_svc_t *ptr = virt_addr;
    printf("base + get_rnd_svc_t_v %p\n", &(ptr->get_rnd_svc_t_v));

    service_get_heartbeat(&(ptr->service_header_t_v),((char *)&(ptr->service_header_t_v) - (char *)ptr));
    service_get_rnd_num(&(ptr->get_rnd_svc_t_v),((char *)&(ptr->get_rnd_svc_t_v) - (char *)ptr));
    service_get_lcs(&(ptr->get_lcs_svc_t_v),((char *)&(ptr->get_lcs_svc_t_v) - (char *)ptr));
    service_get_toc_version(&(ptr->get_toc_version_svc_t_v),((char *)&(ptr->get_toc_version_svc_t_v) - (char *)ptr));
    service_get_toc_number(&(ptr->get_toc_number_svc_t_v),((char *)&(ptr->get_toc_number_svc_t_v) - (char *)ptr));
    service_uart_write(&(ptr->uart_write_svc_t_v),((char *)&(ptr->uart_write_svc_t_v) - (char *)ptr));
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

void service_get_heartbeat(service_header_t *ptr, int offset) {
    int status;
    uint32_t snd_phy_addr, rec_phy_addr;
    *(volatile uint16_t *)&(ptr->send_service_id) = SERVICE_MAINTENANCE_HEARTBEAT_ID;
    snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;
    status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
    if(status == -1) {
        perror("write: heartbeat:");
        exit(1);
    }
    status = read(fd_semhu0_ept, &rec_phy_addr, sizeof(rec_phy_addr));
    if (status == -1) {
        perror("read: heartbeat:");
        exit(1);
    }
    if (rec_phy_addr == snd_phy_addr) {
        printf("The heartbeat test is done\n");
    }
}

void service_get_rnd_num(get_rnd_svc_t *ptr, int offset) {
    int i,status;
    uint32_t snd_phy_addr, rec_phy_addr;
    *(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_CRYPTOCELL_GET_RND;
    *(volatile uint32_t *)&(ptr->send_rnd_length) = 8;
    /* Request RND by sending physical address of get_rnd_svc_t */
    snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;
    status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
    if(status == -1) {
        perror("write: rnd:");
        exit(1);
    }
    status = read(fd_semhu0_ept, &rec_phy_addr, sizeof(rec_phy_addr));
    if (status == -1) {
        perror("read: rnd:");
        exit(1);
    }
    if (rec_phy_addr == snd_phy_addr) {
        printf("The RND values are: ");
        for(i=0; i< ptr->send_rnd_length;++i)
            printf("0x%02x ", (volatile uint8_t)ptr->resp_rnd[i]);
        printf("\n");
    }
}

void service_get_lcs(get_lcs_svc_t *ptr, int offset) {
    int status;
    uint32_t snd_phy_addr, rec_phy_addr;
    *(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_CRYPTOCELL_GET_LCS;
    /* Request LCS by sending physical address of get_lcs_svc_t */
    snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;
    status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
    if(status == -1) {
        perror("write: lcs:");
        exit(1);
    }
    status = read(fd_semhu0_ept, &rec_phy_addr, sizeof(rec_phy_addr));
    if (status == -1) {
        perror("read: lcs:");
        exit(1);
    }
    if (rec_phy_addr == snd_phy_addr) {
        printf("The LCS response is: 0x%x\n", ptr->resp_lcs);
    }
}
void service_get_toc_version(get_toc_version_svc_t *ptr, int offset) {
    int status;
    uint32_t snd_phy_addr, rec_phy_addr;
    *(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_SYSTEM_MGMT_GET_TOC_VERSION;
    /* Request TOC version by sending physical address of get_toc_version_svc_t */
    snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;
    status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
    if(status == -1) {
        perror("write: toc version:");
        exit(1);
    }
    status = read(fd_semhu0_ept, &rec_phy_addr, sizeof(rec_phy_addr));
    if (status == -1) {
        perror("read: toc version:");
        exit(1);
    }
    if (rec_phy_addr == snd_phy_addr) {
        printf("The TOC version is %d\n", ptr->resp_version);
    }
}
void service_get_toc_number(get_toc_number_svc_t *ptr, int offset) {
    int status;
    uint32_t snd_phy_addr, rec_phy_addr;
    *(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_SYSTEM_MGMT_GET_TOC_NUMBER;
    /* Request TOC number by sending physical address of get_toc_number_svc_t */
    snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;
    status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
    if(status == -1) {
        perror("write: toc number:");
        exit(1);
    }
    status = read(fd_semhu0_ept, &rec_phy_addr, sizeof(rec_phy_addr));
    if (status == -1) {
        perror("read: toc number:");
        exit(1);
    }
    if (rec_phy_addr == snd_phy_addr) {
        printf("The TOC number is %d\n", ptr->resp_number_of_toc);
    }
}

void service_uart_write(uart_write_svc_t *ptr, int offset) {
    int status;
    uint32_t snd_phy_addr, rec_phy_addr;
    char message[256] = "Hello from A32\n";
    *(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_APPLICATION_UART_WRITE_ID;
    /* Request TOC version by sending physical address of get_toc_version_svc_t */
    snd_phy_addr = MHU_SERVICES_DATA_LOC + offset;
    memcpy((volatile char *)&(ptr->string_contents), message, sizeof(message));
    status = write(fd_semhu0_ept, &snd_phy_addr, sizeof(snd_phy_addr));
    if(status == -1) {
        perror("write: uart write:");
        exit(1);
    }
    status = read(fd_semhu0_ept, &rec_phy_addr, sizeof(rec_phy_addr));
    if (status == -1) {
        perror("read: uart write:");
        exit(1);
    }
    if (rec_phy_addr == snd_phy_addr) {
        printf("The UART write is successful\n");
    }
}
