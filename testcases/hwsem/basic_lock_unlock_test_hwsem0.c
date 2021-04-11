/* Test lock/unlock functionality of
Alif hardware semaphore 0 (HWSEM0) */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>

#define HWSEM0          "/dev/hwsem0"

#define MASTER_ID       0xA1B2C3D4
#define HWSEM_CNT       1
#define HWSEM_DV_FMT    "/dev/hwsem%d"
#define HWSEM_DV_LEN    13

enum hwsem_command
{
   HWSEM_INVALID = 0,
   HWSEM_RD_MASTER_ID,
   HWSEM_RD_COUNT = 3,
   HWSEM_LOCK,
   HWSEM_UNLOCK
};

int main(int argc, char* argvp[])
{
   int retCode = 0;
   int j, i, hwsemFd[HWSEM_CNT];
   char hwsem_name[HWSEM_DV_LEN];

   printf("Tests HWSEM 0 with 3 times lock, read count value,"
          " and unlock \n");
   printf("===============================================================\n");
   for (i = 0 ; i < HWSEM_CNT ; ++i) {
      snprintf(hwsem_name, HWSEM_DV_LEN, HWSEM_DV_FMT, i);
      hwsemFd[i] = open(hwsem_name, O_RDWR);
      if(hwsemFd[i] < 0)
      {
         fprintf(stderr, "Cannot open /dev/hwsem%d: %s\n", i, strerror(errno));
         return -1;
      }

      printf("calling ioctl for lock \n");
      if(ioctl(hwsemFd[i], HWSEM_LOCK)) {
         printf("ERROR: IOCTL lock failed\n");
         exit(-1);
      }
      printf("HWSEM_LOCK succeed \n");

      printf("Sleep for 10 seconds\n");
      sleep(10);
      printf("Sleep for 10 seconds is over\n");

      if(ioctl(hwsemFd[i], HWSEM_LOCK)) {
         printf("ERROR: IOCTL lock failed\n");
         exit(-1);
      }

      printf("Sleep for 20 seconds\n");
      sleep(20);
      printf("Sleep for 20 seconds is over\n");

      if(ioctl(hwsemFd[i], HWSEM_LOCK)) {
         printf("ERROR: IOCTL lock failed\n");
         exit(-1);
      }

      printf("The total count value of HWSEM%d = %d \n", i,
               ioctl(hwsemFd[i], HWSEM_RD_COUNT));

      for(j=0 ; j < 3 ; ++j) {
         if(ioctl(hwsemFd[i], HWSEM_UNLOCK)) {
            printf("ERROR: IOCTL unlock failed\n");
            exit(-1);
         }
      }

      printf("The total count value of HWSEM%d = %d \n", i,
               ioctl(hwsemFd[i], HWSEM_RD_COUNT));

      close(hwsemFd[i]);
   }

   return retCode;
}
