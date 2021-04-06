/* Test to verify that threads within the
same process lock/unlock HWSEM without blocking.
Example: HWSEM 0 */

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

int fd;
void *print_message_function_thread1 (void *);
void *print_message_function_thread2 (void *);

enum hwsem_command {
    HWSEM_INVALID = 0,
    HWSEM_RD_MASTER_ID,
    HWSEM_RD_COUNT = 3,
    HWSEM_LOCK,
    HWSEM_UNLOCK
};

int main()
{
   pthread_t thread1, thread2;
   char *message1 = "Thread 1";
   char *message2 = "Thread 2";
   int  iret1, iret2;
   int status;

   fd = open("/dev/hwsem0", O_RDWR);
   if(fd == -1) {
      printf("Failed to open /dev/hwsem0 \n");
      return -1;
   }

   /* Create independent threads each of which will execute function */
    iret1 = pthread_create( &thread1, NULL, print_message_function_thread1, (void*) message1);
    iret2 = pthread_create( &thread2, NULL, print_message_function_thread2, (void*) message2);

    /* Wait till threads are complete before main continues. Unless we  */
    /* wait we run the risk of executing an exit which will terminate   */
    /* the process and all threads before the threads have completed.   */
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    printf("pthread_join completed \n");
    printf("The total count value is %d \n", ioctl(fd, HWSEM_RD_COUNT));

    printf("Unlocking .. \n");
    ioctl(fd, HWSEM_UNLOCK);
    ioctl(fd, HWSEM_UNLOCK);
    printf("The total count value is %d \n", ioctl(fd, HWSEM_RD_COUNT));

    printf("Thread 1 returns: %d\n",iret1);
    printf("Thread 2 returns: %d\n",iret2);
    exit(0);
}

void * print_message_function_thread1( void *ptr )
{
   int status;
   printf("Thread 1: Trying to lock \n");
   status = ioctl(fd, HWSEM_LOCK);
   if(status == -1) {
       printf("Unable to lock the hwsem0");
       return -1;
   }
   printf(" lock succeeded \n");
   printf("The master ID is 0x%x \n", ioctl(fd, HWSEM_RD_MASTER_ID));
}

void * print_message_function_thread2( void *ptr )
{
   int status;
   printf("Thread 2: Trying to lock \n");
   status = ioctl(fd, HWSEM_LOCK);
   if(status == -1) {
       printf("Unable to lock the hwsem0");
       return -1;
   }
   printf("lock succeeded \n");
   printf("The master ID is 0x%x \n", ioctl(fd, HWSEM_RD_MASTER_ID));
}
