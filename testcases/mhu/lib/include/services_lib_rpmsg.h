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

#define RPMSG_CREATE_EPT_IOCTL _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)
#define RPMSG_DESTROY_EPT_IOCTL _IO(0xb5, 0x2)
#define MHU_SERVICES_DATA_LOC 0x0827F000

struct rpmsg_endpoint_info semhu0_eptinfo = {"txdb4", 0XFFFFFFFF, 0xFFFFFFFF};
struct rpmsg_endpoint_info semhu1_eptinfo = {"rxdb4", 0XFFFFFFFF, 0xFFFFFFFF};
int fd_semhu0_ept, fd_semhu1_ept;
#endif /* __SERVICES_LIB_RPMSG_H__ */
