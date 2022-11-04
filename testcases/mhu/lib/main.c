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
#include "services_lib_rpmsg.h"

int main()
{
   printf("MHU0 TEST BETWEEN A32 and SE cores for RND\n");
   printf("==========================================\n");
   rpmsg_init();
   map_mem();
 
   rpmsg_get_heartbeat();
   rpmsg_get_rnd_num();
   rpmsg_get_lcs();
   rpmsg_get_toc_version();
   rpmsg_get_toc_number();
   rpmsg_uart_write();

   mem_destroy();
   rpmsg_destroy();
}
