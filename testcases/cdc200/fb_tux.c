/*
 * To test that the Linux framebuffer is set up correctly, and that the device permissions
 * are correct, use the program below which opens the frame buffer and draws a gradient-
 * filled red square:
 * retrieved from:
 * Testing the Linux Framebuffer for Qtopia Core (qt4-x11-4.2.2)
 * http://cep.xor.aps.anl.gov/software/qt4-x11-4.2.2/qtopiacore-testingframebuffer.html
 *
 * Modified for Alif internal test use by Harith George (harith.g@alif.semi.com)
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "tux.h"

int main(int argc, char *argv[])
{
    int fbfd = 0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long int screensize = 0, location = 0;
    char *fbp = 0;
    int x = 0, y = 0, index = 0;

    // Open the file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        perror("Error: cannot open framebuffer device");
        exit(1);
    }
    printf("The framebuffer device was opened successfully.\n");

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("Error reading fixed information");
        exit(2);
    }

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("Error reading variable information");
        exit(3);
    }

    printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    printf("xoffset %d yoffset %d \n", vinfo.xoffset, vinfo.yoffset);
    printf("Line length = %d\n", finfo.line_length);

    // Figure out the size of the screen in bytes
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

    // Map the device to memory
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((int)fbp == -1) {
        perror("Error: failed to map framebuffer device to memory");
        exit(4);
    }
    printf("The framebuffer device was mapped to memory successfully.\n");

    // Figure out where in memory to put the pixel
#if XRGB8888
    /* Tux image width 166, height 196, 32 bit, */
    for (y = 0; y < 196; y++) {
        location = y * finfo.line_length;
        printf("location %d index %d\n", location, index);
        for (x = 0; x < 166; x++) {
            //  location = (x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) +
            //             (y+vinfo.yoffset) * finfo.line_length;
            fbp[location++] = tux_map[index++];
            fbp[location++] = tux_map[index++];
            fbp[location++] = tux_map[index++];
            fbp[location++] = tux_map[index++];
        }
    }
#else
    for (y = 0; y < 196; y++) {
        location = y * finfo.line_length;
        for (x = 0; x < 166; x++) {
                //unsigned char red, green, blue, alpha;
                unsigned char red, green, blue;
                unsigned short val;

                blue = ((tux_map[index++]) >> 3) & 0x1F;
                green = ((tux_map[index++]) >> 2) & 0x3F;
                red = ((tux_map[index++]) >> 3) & 0x1F;
                //alpha = tux_map[index++];
		index++; //for alpha
                val = (red << 11)| (green << 5) | (blue);
                *((unsigned short *)(&fbp[location])) = val;
                location += 2;
        }
    }
    printf("location %ld index %d\n", location, index);
#endif
    printf("bytes written = %d\n", index);

    munmap(fbp, screensize);
    close(fbfd);
    return 0;
}
