#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include "services_lib_a32.h"

void service_get_heartbeat(int fd_semhu0_ept, service_header_t *ptr, uint32_t snd_phy_addr) {
    int status;
    uint32_t rec_phy_addr;
    memset(ptr, 0, sizeof(service_header_t));
    *(volatile uint16_t *)&(ptr->send_service_id) = SERVICE_MAINTENANCE_HEARTBEAT_ID;
    //snd_phy_addr = MHU_SERVICES_DATA_LOC;
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

void service_get_rnd_num(int fd_semhu0_ept, get_rnd_svc_t *ptr, uint32_t snd_phy_addr) {
    int i,status;
    uint32_t rec_phy_addr;
    memset(ptr, 0, sizeof(get_rnd_svc_t));
    *(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_CRYPTOCELL_GET_RND;
    *(volatile uint32_t *)&(ptr->send_rnd_length) = 8;
    /* Request RND by sending physical address of get_rnd_svc_t */
    //snd_phy_addr = MHU_SERVICES_DATA_LOC;
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

void service_get_lcs(int fd_semhu0_ept, get_lcs_svc_t *ptr, uint32_t snd_phy_addr) {
    int status;
    uint32_t rec_phy_addr;
    memset(ptr, 0, sizeof(get_lcs_svc_t));
    *(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_CRYPTOCELL_GET_LCS;
    /* Request LCS by sending physical address of get_lcs_svc_t */
    //snd_phy_addr = MHU_SERVICES_DATA_LOC;
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
void service_get_toc_version(int fd_semhu0_ept, get_toc_version_svc_t *ptr, uint32_t snd_phy_addr) {
    int status;
    uint32_t rec_phy_addr;
    memset(ptr, 0, sizeof(get_toc_version_svc_t));
    *(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_SYSTEM_MGMT_GET_TOC_VERSION;
    /* Request TOC version by sending physical address of get_toc_version_svc_t */
    //snd_phy_addr = MHU_SERVICES_DATA_LOC;
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
void service_get_toc_number(int fd_semhu0_ept, get_toc_number_svc_t *ptr, uint32_t snd_phy_addr) {
    int status;
    uint32_t rec_phy_addr;
    memset(ptr, 0, sizeof(get_toc_number_svc_t));
    *(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_SYSTEM_MGMT_GET_TOC_NUMBER;
    /* Request TOC number by sending physical address of get_toc_number_svc_t */
    //snd_phy_addr = MHU_SERVICES_DATA_LOC;
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

void service_uart_write(int fd_semhu0_ept, uart_write_svc_t *ptr, uint32_t snd_phy_addr) {
    int status;
    uint32_t rec_phy_addr;
    memset(ptr, 0, sizeof(uart_write_svc_t));
    char message[256] = "Hello from A32\n";
    *(volatile uint16_t *)&(ptr->header.send_service_id) = SERVICE_APPLICATION_UART_WRITE_ID;
    /* Request TOC version by sending physical address of get_toc_version_svc_t */
    //snd_phy_addr = MHU_SERVICES_DATA_LOC;
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
