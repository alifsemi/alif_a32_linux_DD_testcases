/* Copyright (C) 2022 Alif Semiconductor - All Rights Reserved.
 * Use, distribution and modification of this code is permitted under the
 * terms stated in the Alif Semiconductor Software License Agreement
 *
 * You should have received a copy of the Alif Semiconductor Software
 * License Agreement with this file. If not, please write to:
 * contact@alifsemi.com, or visit: https://alifsemi.com/license
 *
 */

#ifndef __SERVICES_LIB_RPMSG_H__
#define __SERVICES_LIB_RPMSG_H__
#include <sys/types.h>
struct rpmsg_endpoint_info
{
    char name[32];
    uint32_t src;
    uint32_t dst;
};

#define RPMSG_CTRL "/dev/rpmsg_ctrl0"
#define RPMSG_CREATE_EPT_IOCTL _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)
#define RPMSG_DESTROY_EPT_IOCTL _IO(0xb5, 0x2)
#define MHU_SERVICES_DATA_LOC 0x0827F000
#define RPMSG_MHU_PNT "/dev/rpmsg0"
#define MEM_FILE "/dev/mem"


struct rpmsg_endpoint_info semhu0_eptinfo = {"se_mhu0", 0XFFFFFFFF, 0xFFFFFFFF};

int rpmsg_init();
void map_mem();
void rpmsg_get_heartbeat();
void rpmsg_get_rnd_num();
void rpmsg_get_lcs();
void rpmsg_get_toc_version();
void rpmsg_get_toc_number();
void rpmsg_uart_write();
void mem_destroy();
void rpmsg_destroy();
#endif /* __SERVICES_LIB_RPMSG_H__ */
