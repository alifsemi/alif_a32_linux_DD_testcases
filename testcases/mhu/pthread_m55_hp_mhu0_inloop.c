/* Copyright (C) 2026 Alif Semiconductor - All Rights Reserved.
 * Use, distribution and modification of this code is permitted under the
 * terms stated in the Alif Semiconductor Software License Agreement
 *
 * You should have received a copy of the Alif Semiconductor Software
 * License Agreement with this file. If not, please write to:
 * contact@alifsemi.com, or visit: https://alifsemi.com/license
 *
 */
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

void *send_thread(void *);
void *recv_thread(void *);

struct rpmsg_endpoint_info {
    char name[32];
    uint32_t src;
    uint32_t dst;
};

#define ITERATIONS 10
#define RPMSG_CREATE_EPT_IOCTL _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)
#define RPMSG_DESTROY_EPT_IOCTL _IO(0xb5, 0x2)

/* RPMSG Device Paths */
#define RPMSG_CTRL0_DEV         "/dev/rpmsg_ctrl0"
#define RPMSG0_DEV              "/dev/rpmsg0"
#define RPMSG1_DEV              "/dev/rpmsg1"

/* Endpoints for 6.12 Kernel */
struct rpmsg_endpoint_info ept_tx_info = {"txdb0", 0xFFFFFFFF, 0xFFFFFFFF};
struct rpmsg_endpoint_info ept_rx_info = {"rxdb0", 0xFFFFFFFF, 0xFFFFFFFF};

int fd_ctrl = -1;
int fd_tx = -1;
int fd_rx = -1;

volatile sig_atomic_t stop_flag = 0;

static void sigint_handler(int sig_num)
{
    (void)sig_num;
    stop_flag = 1;
}

static void cleanup(void)
{
    if (fd_tx != -1) {
        ioctl(fd_tx, RPMSG_DESTROY_EPT_IOCTL);
        close(fd_tx);
        fd_tx = -1;
    }
    if (fd_rx != -1) {
        ioctl(fd_rx, RPMSG_DESTROY_EPT_IOCTL);
        close(fd_rx);
        fd_rx = -1;
    }
    if (fd_ctrl != -1) {
        close(fd_ctrl);
        fd_ctrl = -1;
    }
    printf("\nClosed files, exiting\n");
}

int main()
{
    pthread_t sender_thread, receiver_thread;
    int ioctl_ret;

    printf("==========================================\n");
    printf("A32 <-> M55 HP MHU0 Test (6.12 Kernel)\n");
    printf("==========================================\n");

    /* Open control device */
    fd_ctrl = open(RPMSG_CTRL0_DEV, O_RDWR);
    if (fd_ctrl == -1) {
        printf("ERROR: Cannot open %s: %s\n", RPMSG_CTRL0_DEV, strerror(errno));
        return -1;
    }

    /* Create TX endpoint (txdb0) -> /dev/rpmsg0 */
    printf("Creating TX endpoint '%s'...\n", ept_tx_info.name);
    ioctl_ret = ioctl(fd_ctrl, RPMSG_CREATE_EPT_IOCTL, &ept_tx_info);
    if (ioctl_ret == -1) {
        printf("ERROR: TX IOCTL failed: %s\n", strerror(errno));
        close(fd_ctrl);
        return -1;
    }

    /* Create RX endpoint (rxdb0) -> /dev/rpmsg1 */
    printf("Creating RX endpoint '%s'...\n", ept_rx_info.name);
    ioctl_ret = ioctl(fd_ctrl, RPMSG_CREATE_EPT_IOCTL, &ept_rx_info);
    if (ioctl_ret == -1) {
        printf("ERROR: RX IOCTL failed: %s\n", strerror(errno));
        close(fd_ctrl);
        return -1;
    }

    /* Open TX endpoint */
    fd_tx = open(RPMSG0_DEV, O_RDWR);
    if (fd_tx == -1) {
        printf("ERROR: Cannot open %s: %s\n", RPMSG0_DEV, strerror(errno));
        return -1;
    }
    printf("Opened TX: %s\n", RPMSG0_DEV);

    /* Open RX endpoint */
    fd_rx = open(RPMSG1_DEV, O_RDWR);
    if (fd_rx == -1) {
        printf("ERROR: Cannot open %s: %s\n", RPMSG1_DEV, strerror(errno));
        return -1;
    }
    printf("Opened RX: %s\n\n", RPMSG1_DEV);

    /* Register signal handler — only sets a flag (async-signal-safe) */
    signal(SIGINT, sigint_handler);

    /* Create threads */
    ioctl_ret = pthread_create(&sender_thread, NULL, send_thread, NULL);
    if (ioctl_ret != 0) {
        fprintf(stderr, "ERROR: Failed to create send_thread: %s\n", strerror(ioctl_ret));
        cleanup();
        return -1;
    }

    ioctl_ret = pthread_create(&receiver_thread, NULL, recv_thread, NULL);
    if (ioctl_ret != 0) {
        fprintf(stderr, "ERROR: Failed to create recv_thread: %s\n", strerror(ioctl_ret));
        stop_flag = 1;
        pthread_join(sender_thread, NULL);
        cleanup();
        return -1;
    }

    pthread_join(sender_thread, NULL);
    pthread_join(receiver_thread, NULL);

    cleanup();
    return 0;
}

void *send_thread(void *arg)
{
    uint32_t tx_payload[2];
    int iterator;

    (void)arg;
    printf("SEND thread started\n");
    tx_payload[0] = 0xCAFECAFE;
    tx_payload[1] = 0xDEADDEAD;

    for (iterator = 0; iterator < ITERATIONS && !stop_flag; iterator++) {
        printf("\n=== Iteration %d ===\n", iterator + 1);
        printf("A32: Sending 0x%x, 0x%x\n", tx_payload[0], tx_payload[1]);

        if (write(fd_tx, tx_payload, sizeof(tx_payload)) == -1) {
            printf("ERROR: Write failed: %s\n", strerror(errno));
        }

        sleep(1);
    }

    printf("SEND thread done\n");
    return NULL;
}

void *recv_thread(void *arg)
{
    uint32_t rx_word;
    int iterator, read_bytes;

    (void)arg;
    printf("RECV thread started\n");
    sleep(1);  /* Let send start first */

    for (iterator = 0; iterator < ITERATIONS && !stop_flag; iterator++) {
        /* Read first word from RX endpoint */
        read_bytes = read(fd_rx, &rx_word, sizeof(rx_word));
        if (read_bytes > 0) {
            printf("A32: Received[0] = 0x%x\n", rx_word);
        } else {
            printf("ERROR: Read failed: %s\n", strerror(errno));
        }

        /* Read second word from RX endpoint */
        read_bytes = read(fd_rx, &rx_word, sizeof(rx_word));
        if (read_bytes > 0) {
            printf("A32: Received[1] = 0x%x\n", rx_word);
        } else {
            printf("ERROR: Read failed: %s\n", strerror(errno));
        }

        sleep(1);
    }

    printf("RECV thread done\n");
    return NULL;
}
