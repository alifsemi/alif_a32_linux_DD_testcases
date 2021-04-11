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

#define ITERATIONS 10
void *print_message_function_send( void * );
void *print_message_function_receive( void * );

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


struct rpmsg_endpoint_info m55_hp_mhu1_eptinfo = {"m55_hp_mhu1", 0XFFFFFFFF, 0xFFFFFFFF};
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
int fd_m55_hp_mhu1_ept;
int snd_count = 0;
int rec_count = 0;

extern int errno;

int main()
{
   pthread_t thread1, thread2;
   char *message1 = "Thread 1";
   char *message2 = "Thread 2";
   int  iret1, iret2;
   int fd, status;

   printf("MHU1 TEST BETWEEN A32 and M55_HP cores \n");
   printf("===================================== \n");

   fd = open("/dev/rpmsg_ctrl0", O_RDWR);
   if(fd == -1) {
      printf("Failed to open /dev/rpmsg_ctrl0 \n");
      return -1;
   }

   status = ioctl(fd, RPMSG_CREATE_EPT_IOCTL, &m55_hp_mhu1_eptinfo);
   if (status == -1) {
       printf("SEND IOCTL error status = 0x%x \n", status);
       return -1;
   }

  /* Create Endpoint to receive/send MHU data */
  fd_m55_hp_mhu1_ept = open("/dev/rpmsg1", O_RDWR);// | O_NONBLOCK);
  if (fd_m55_hp_mhu1_ept == -1) {
    printf("Unable to open file /dev/rpmsg1, please try again ....\n");
    exit(errno);
  }

   /* Create independent threads each of which will execute function */
   iret1 = pthread_create( &thread1, NULL, print_message_function_send, (void*) message1);
   iret2 = pthread_create( &thread2, NULL, print_message_function_receive, (void*) message2);

   /* Wait till threads are complete before main continues. Unless we  */
   /* wait we run the risk of executing an exit which will terminate   */
   /* the process and all threads before the threads have completed.   */
   pthread_join( thread1, NULL);
   pthread_join( thread2, NULL);

   printf("Thread 1 returns: %d\n",iret1);
   printf("Thread 2 returns: %d\n",iret2);

   status = ioctl(fd_m55_hp_mhu1_ept, RPMSG_DESTROY_EPT_IOCTL);
   if (status == -1) {
       printf("Unable to destroy fd_m55_hp_mhu1_ept endpoint correctly \n");
   }
   close(fd_m55_hp_mhu1_ept);
   close(fd);
   printf("Closed opened files \n");
   exit(0);
}

void * print_message_function_send( void *ptr )
{
     char *message;
     message = (char *) ptr;
     int status;
     int i;
     printf("Starting SEND: %s \n", message);

     /* data for two channels */
     unsigned int data[2];
     data[0] = 0xCAFECAFE;
     data[1] = 0xDEADDEAD;

     for (i =0; i < ITERATIONS ; ++i) {
        printf("\nSEND: Sending message values 0x%x 0x%x ... ", data[0], data[1]);

	/* Lock, Write/send, unlock */
	pthread_mutex_lock( &mutex1 );
        status = write(fd_m55_hp_mhu1_ept, &data, sizeof(data));
	if(status == -1) {
	    printf("Unable send data\n");
	}
	else {
	    printf("Done\n");
	}
	printf("\nSEND COUNT = %d \n", ++snd_count);
	pthread_mutex_unlock( &mutex1 );
	sleep(5);
  }

}

void * print_message_function_receive( void *ptr )
{
     int i = 0;
     char *message;
     message = (char *) ptr;
     int status;
     sleep(1);
     printf("Starting RECV: %s \n", message);
     /* data to recieve */
     int data;


     for (i =0; i < ITERATIONS ; ++i) {
	/* Lock, receive/read, and unlock */
	pthread_mutex_lock( &mutex1 );
        printf("RECV: Reading data ..... \n");
	status = read(fd_m55_hp_mhu1_ept, &data, sizeof(data));
	if (status == -1 && errno == EAGAIN) {
		printf("No data available \n");
	}

	printf("RECV: First data is 0x%x \n", data);
	status = read(fd_m55_hp_mhu1_ept, &data, sizeof(data));
	if (status == -1 && errno == EAGAIN) {
		printf("No data available \n");
	}
	printf("RECV: Second data is 0x%x \n", data);
        pthread_mutex_unlock( &mutex1 );
	printf("\n RECEIVE COUNT = %d\n", ++rec_count);
	sleep(1);
     }
}