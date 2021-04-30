/*
 * Test kernel watchdog driver functionality
 * from userspace application.
 * execute application to set timeout and enable wdt
 * inputs: i for keepalive, x for quit/stop
 * application will be terminated once x is provided
 * as input
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <linux/watchdog.h>

#define WATCHDOGDEV "/dev/watchdog"

int main(int argc, char **argv)
{
   int fd;         /* File handler for watchdog */
   int interval;      /* Watchdog timeout interval (in secs) */
   int bootstatus;      /* Wathdog last boot status */
   char *dev;      /* Watchdog default device file */

   int next_option;   /* getopt iteration var */
   char kick_watchdog;   /* kick_watchdog options */

   /* Init variables */
   dev = WATCHDOGDEV;
   interval = 500;
   kick_watchdog = 0;

   /* Once the watchdog device file is open, the watchdog will be activated by
      the driver */
   fd = open(dev, O_RDWR);
   if (-1 == fd) {
      fprintf(stderr, "Error: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
   }

   if (ioctl(fd, WDIOC_SETTIMEOUT, &interval) != 0) {
         fprintf(stderr,
            "Error: Set watchdog interval failed\n");
         exit(EXIT_FAILURE);
      }

   /* Display current watchdog interval */
   if (ioctl(fd, WDIOC_GETTIMEOUT, &interval) == 0) {
      fprintf(stdout, "Current watchdog interval is [%ld]\n", interval);
   } else {
      fprintf(stderr, "Error: Cannot read watchdog interval\n");
      exit(EXIT_FAILURE);
   }
     int enable = WDIOS_ENABLECARD;
     ioctl(fd, WDIOC_SETOPTIONS, &enable);
     int timeleft;
     ioctl(fd, WDIOC_GETTIMELEFT, &timeleft);
     printf("The timeout was is [%ld] seconds\n", timeleft);

    //ioctl(fd, WDIOC_KEEPALIVE, NULL);

    do {
      kick_watchdog = getchar();
      switch (kick_watchdog) {
      case 'w':
         write(fd, "w", 1);
         fprintf(stdout,
            "Kick watchdog through writing over device file\n");
         break;
      case 'i':
         ioctl(fd, WDIOC_KEEPALIVE, NULL);
         fprintf(stdout, "Kick watchdog through IOCTL\n");
         break;
      case 'x':
         fprintf(stdout, "Goodbye !\n");
         break;
      default:
         fprintf(stdout, "Unknown command\n");
         break;
      }
   } while (kick_watchdog != 'x');

  /* The 'V' value needs to be written into watchdog device file to indicate
      that we intend to close/stop the watchdog. Otherwise, debug message
      'Watchdog timer closed unexpectedly' will be printed
    */
   write(fd, "V", 1);

   /* Closing the watchdog device will deactivate the watchdog. */
   close(fd);
}
