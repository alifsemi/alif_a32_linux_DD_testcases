/* Copyright (C) 2026 Alif Semiconductor - All Rights Reserved.
 * Use, distribution and modification of this code is permitted under the
 * terms stated in the Alif Semiconductor Software License Agreement
 *
 * You should have received a copy of the Alif Semiconductor Software
 * License Agreement with this file. If not, please write to:
 * contact@alifsemi.com, or visit: https://alifsemi.com/license
 *
 * MHU Doorbell + Shared SRAM1 corner-case test: A32 <-> M55-HP  (MHU1)
 *
 * 25 iterations, each with a DIFFERENT data pattern and/or length:
 *   A. Normal patterns (15):
 *   [0]  All zeros (16 B)          [8]  Nibble-swap (16 B)
 *   [1]  All ones  (16 B)          [9]  Pseudo-random (16 B)
 *   [2]  Alternating bits (16 B)   [10] Single byte (1 B)
 *   [3]  Incrementing (16 B)       [11] Odd length (7 B)
 *   [4]  Decrementing (16 B)       [12] Max payload (240 B)
 *   [5]  Walking ones (16 B)       [13] Same byte repeated (16 B)
 *   [6]  Walking zeros (16 B)      [14] Back-to-back stress (16 B)
 *   [7]  Byte boundary (16 B)
 *   B. Payload length edge cases (5):
 *   [15] 2-byte min-even           [18] 128-byte power-of-2
 *   [16] 3-byte min-odd>1          [19] 239-byte max-1
 *   [17] 64-byte cache-line
 *   C. Additional corner cases (5):
 *   [20] 240-byte MAX_PAYLOAD      [23] All-ones 0xFF x 240
 *   [21] 1-byte minimum            [24] Sequential counter 0x00-0xEF
 *   [22] Alternating 0x00/0xFF
 *
 * M55-HP echoes each byte XOR'd with (iteration + 0x42); A32 verifies.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <dirent.h>

/* ---- Shared memory constants (must match Zephyr HP MHU1 side) ---- */
#define SHARED_MEM_BASE    0x027DDA00
#define A32_TO_HP_OFFSET   0x0000
#define HP_TO_A32_OFFSET   0x0100
#define SHARED_MEM_SIZE    0x0200

#define MAGIC_A32          0xA32F055E
#define MAGIC_HP           0xF055EA32
#define MAX_PAYLOAD        240
#define MAX_ERRORS         10

struct shared_msg {
	uint32_t magic;
	uint32_t msg_id;
	uint32_t data_len;
	uint8_t  data[MAX_PAYLOAD];
	uint32_t checksum;
};

#define ITERATIONS 25
#define RX_TIMEOUT_MS  10000  /* 10s timeout waiting for HP response */

struct test_vector {
	const char *name;
	uint32_t    len;
	uint8_t     pattern[MAX_PAYLOAD];
};

static struct test_vector test_vectors[ITERATIONS];

static void init_test_vectors(void)
{
	int j;

	test_vectors[0].name = "All-zeros(16B)";
	test_vectors[0].len  = 16;
	memset(test_vectors[0].pattern, 0x00, 16);

	test_vectors[1].name = "All-ones(16B)";
	test_vectors[1].len  = 16;
	memset(test_vectors[1].pattern, 0xFF, 16);

	test_vectors[2].name = "Alternating(AA/55)";
	test_vectors[2].len  = 16;
	for (j = 0; j < 16; j++)
		test_vectors[2].pattern[j] = (j & 1) ? 0x55 : 0xAA;

	test_vectors[3].name = "Incrementing(00-0F)";
	test_vectors[3].len  = 16;
	for (j = 0; j < 16; j++)
		test_vectors[3].pattern[j] = j;

	test_vectors[4].name = "Decrementing(FF-F0)";
	test_vectors[4].len  = 16;
	for (j = 0; j < 16; j++)
		test_vectors[4].pattern[j] = 0xFF - j;

	test_vectors[5].name = "Walking-ones";
	test_vectors[5].len  = 16;
	for (j = 0; j < 16; j++)
		test_vectors[5].pattern[j] = 1 << (j % 8);

	test_vectors[6].name = "Walking-zeros";
	test_vectors[6].len  = 16;
	for (j = 0; j < 16; j++)
		test_vectors[6].pattern[j] = ~(1 << (j % 8));

	test_vectors[7].name = "Byte-boundary";
	test_vectors[7].len  = 16;
	{
		uint8_t boundary_vals[] = {0x00,0x01,0x7E,0x7F,0x80,0x81,0xFE,0xFF,
				0x00,0x01,0x7E,0x7F,0x80,0x81,0xFE,0xFF};
		memcpy(test_vectors[7].pattern, boundary_vals, 16);
	}

	test_vectors[8].name = "Nibble-swap(0F/F0)";
	test_vectors[8].len  = 16;
	for (j = 0; j < 16; j++)
		test_vectors[8].pattern[j] = (j & 1) ? 0xF0 : 0x0F;

	test_vectors[9].name = "Pseudo-random(16B)";
	test_vectors[9].len  = 16;
	{
		uint8_t pseudo_rand[] = {0xA5,0x5A,0x3C,0xC3,0x69,0x96,0x1E,0xE1,
				0xB4,0x4B,0x78,0x87,0xD2,0x2D,0xF6,0x6F};
		memcpy(test_vectors[9].pattern, pseudo_rand, 16);
	}

	test_vectors[10].name = "Single-byte(1B)";
	test_vectors[10].len  = 1;
	test_vectors[10].pattern[0] = 0x42;

	test_vectors[11].name = "Odd-length(7B)";
	test_vectors[11].len  = 7;
	for (j = 0; j < 7; j++)
		test_vectors[11].pattern[j] = 0x10 + j;

	test_vectors[12].name = "Max-payload(240B)";
	test_vectors[12].len  = 240;
	for (j = 0; j < 240; j++)
		test_vectors[12].pattern[j] = (j * 7 + 0x33) & 0xFF;

	test_vectors[13].name = "Same-byte(0x5A x16)";
	test_vectors[13].len  = 16;
	memset(test_vectors[13].pattern, 0x5A, 16);

	test_vectors[14].name = "Stress-no-sleep(16B)";
	test_vectors[14].len  = 16;
	for (j = 0; j < 16; j++)
		test_vectors[14].pattern[j] = (j ^ 0xAB) & 0xFF;

	/* ---- B. Payload Length Edge Cases ---- */

	test_vectors[15].name = "2-byte(min-even)";
	test_vectors[15].len  = 2;
	test_vectors[15].pattern[0] = 0xDE;
	test_vectors[15].pattern[1] = 0xAD;

	test_vectors[16].name = "3-byte(min-odd>1)";
	test_vectors[16].len  = 3;
	test_vectors[16].pattern[0] = 0xCA;
	test_vectors[16].pattern[1] = 0xFE;
	test_vectors[16].pattern[2] = 0x42;

	test_vectors[17].name = "64-byte(cache-line)";
	test_vectors[17].len  = 64;
	for (j = 0; j < 64; j++)
		test_vectors[17].pattern[j] = (j * 3 + 0x11) & 0xFF;

	test_vectors[18].name = "128-byte(power-of-2)";
	test_vectors[18].len  = 128;
	for (j = 0; j < 128; j++)
		test_vectors[18].pattern[j] = (j * 5 + 0x77) & 0xFF;

	test_vectors[19].name = "239-byte(max-1)";
	test_vectors[19].len  = 239;
	for (j = 0; j < 239; j++)
		test_vectors[19].pattern[j] = (j * 11 + 0x23) & 0xFF;

	/* ---- C. Additional Corner Cases ---- */

	test_vectors[20].name = "240-byte(MAX)";
	test_vectors[20].len  = 240;
	for (j = 0; j < 240; j++)
		test_vectors[20].pattern[j] = (j * 13 + 0x07) & 0xFF;

	test_vectors[21].name = "1-byte(min)";
	test_vectors[21].len  = 1;
	test_vectors[21].pattern[0] = 0x7E;

	test_vectors[22].name = "Alt-00FF(16B)";
	test_vectors[22].len  = 16;
	for (j = 0; j < 16; j++)
		test_vectors[22].pattern[j] = (j & 1) ? 0xFF : 0x00;

	test_vectors[23].name = "All-ones(0xFF x240)";
	test_vectors[23].len  = 240;
	memset(test_vectors[23].pattern, 0xFF, 240);

	test_vectors[24].name = "SeqCounter(0-EF)";
	test_vectors[24].len  = 240;
	for (j = 0; j < 240; j++)
		test_vectors[24].pattern[j] = (uint8_t)j;
}

struct rpmsg_endpoint_info {
	char name[32];
	uint32_t src;
	uint32_t dst;
};

#define RPMSG_CREATE_EPT_IOCTL  _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)
#define RPMSG_DESTROY_EPT_IOCTL _IO(0xb5, 0x2)

#define DEV_MEM           "/dev/mem"
#define RPMSG_CTRL0_DEV   "/dev/rpmsg_ctrl0"

/* HP MHU1 endpoints */
#define EPT_TX_NAME       "txdb1"
#define EPT_RX_NAME       "rxdb1"

struct rpmsg_endpoint_info ept_tx_info = {EPT_TX_NAME, 0xFFFFFFFF, 0xFFFFFFFF};
struct rpmsg_endpoint_info ept_rx_info = {EPT_RX_NAME, 0xFFFFFFFF, 0xFFFFFFFF};

int fd_ctrl = -1, fd_tx = -1, fd_rx = -1, mem_fd = -1;
void *sram_base = MAP_FAILED;
uint32_t mapped_size;

volatile sig_atomic_t stop_flag = 0;

static void sigint_handler(int sig) { (void)sig; stop_flag = 1; }

/**
 * find_rpmsg_dev() - Discover /dev/rpmsgX node by endpoint name via sysfs
 * @ept_name: Endpoint name to match (e.g. "txdb1")
 * @dev_path: Buffer to store discovered device path (e.g. "/dev/rpmsg0")
 * @path_len: Size of dev_path buffer
 *
 * Scans /sys/class/rpmsg/rpmsgX/name to find the device node whose
 * endpoint name matches @ept_name.
 *
 * Return: 0 on success, -1 if no matching device found
 */
static int find_rpmsg_dev(const char *ept_name, char *dev_path, size_t path_len)
{
	DIR *dir;
	struct dirent *entry;
	char sysfs_name_path[256];
	char ept_name_buf[64];
	int sysfs_fd;
	ssize_t bytes_read;

	dir = opendir("/sys/class/rpmsg");
	if (!dir)
		return -1;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;

		snprintf(sysfs_name_path, sizeof(sysfs_name_path),
			 "/sys/class/rpmsg/%s/name", entry->d_name);

		sysfs_fd = open(sysfs_name_path, O_RDONLY);
		if (sysfs_fd < 0)
			continue;

		bytes_read = read(sysfs_fd, ept_name_buf,
				  sizeof(ept_name_buf) - 1);
		close(sysfs_fd);

		if (bytes_read <= 0)
			continue;

		/* Remove trailing newline */
		if (ept_name_buf[bytes_read - 1] == '\n')
			bytes_read--;
		ept_name_buf[bytes_read] = '\0';

		if (strcmp(ept_name_buf, ept_name) == 0) {
			snprintf(dev_path, path_len, "/dev/%s",
				 entry->d_name);
			closedir(dir);
			return 0;
		}
	}

	closedir(dir);
	return -1;
}

static uint32_t calc_checksum(const uint8_t *buf, uint32_t len)
{
	uint32_t sum = 0, i;
	for (i = 0; i < len; i++) sum += buf[i];
	return sum;
}

static void cleanup_resources(void)
{
	if (fd_tx >= 0) {
		ioctl(fd_tx, RPMSG_DESTROY_EPT_IOCTL);
		close(fd_tx);
	}
	if (fd_rx >= 0) {
		ioctl(fd_rx, RPMSG_DESTROY_EPT_IOCTL);
		close(fd_rx);
	}
	if (fd_ctrl >= 0)
		close(fd_ctrl);
	if (sram_base != MAP_FAILED)
		munmap(sram_base, mapped_size);
	if (mem_fd >= 0)
		close(mem_fd);
	printf("Cleaned up\n");
}

int main(void)
{
	int ret, iter;
	uint32_t page_size, offset_in_page;
	volatile struct shared_msg *tx_region, *rx_region;
	int pass_count = 0, fail_count = 0;

	init_test_vectors();

	printf("==========================================\n");
	printf("A32 <-> M55-HP : MHU1 Corner-Case Test\n");
	printf("  SRAM base:         0x%08x\n", SHARED_MEM_BASE);
	printf("  A32->HP region:    0x%08x\n", SHARED_MEM_BASE + A32_TO_HP_OFFSET);
	printf("  HP->A32 region:    0x%08x\n", SHARED_MEM_BASE + HP_TO_A32_OFFSET);
	printf("  25 vectors (15 normal + 10 corner-case)\n");
	printf("==========================================\n");

	mem_fd = open(DEV_MEM, O_RDWR | O_SYNC);
	if (mem_fd == -1) {
		perror("open " DEV_MEM);
		return -1;
	}

	page_size = getpagesize();
	mapped_size = page_size;
	offset_in_page = SHARED_MEM_BASE & (page_size - 1);

	sram_base = mmap(NULL, mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED,
			 mem_fd, SHARED_MEM_BASE & ~(off_t)(page_size - 1));
	if (sram_base == MAP_FAILED) {
		perror("mmap");
		cleanup_resources();
		return -1;
	}

	/* Validate mmap covers the entire shared memory region */
	if (offset_in_page + SHARED_MEM_SIZE > mapped_size) {
		printf("ERROR: mmap bounds: offset(0x%x) + size(0x%x) "
		       "> mapped(0x%x)\n",
		       offset_in_page, SHARED_MEM_SIZE, mapped_size);
		cleanup_resources();
		return -1;
	}

	tx_region = (volatile struct shared_msg *)
			((char *)sram_base + offset_in_page + A32_TO_HP_OFFSET);
	rx_region = (volatile struct shared_msg *)
			((char *)sram_base + offset_in_page + HP_TO_A32_OFFSET);

	printf("SRAM mapped: virt=%p (page_off=0x%x)\n", sram_base, offset_in_page);
	memset((void *)tx_region, 0, sizeof(struct shared_msg));

	fd_ctrl = open(RPMSG_CTRL0_DEV, O_RDWR);
	if (fd_ctrl == -1) { perror("open rpmsg_ctrl0"); cleanup_resources(); return -1; }

	printf("Creating TX endpoint '%s'...\n", ept_tx_info.name);
	ret = ioctl(fd_ctrl, RPMSG_CREATE_EPT_IOCTL, &ept_tx_info);
	if (ret == -1) { perror("ioctl TX"); cleanup_resources(); return -1; }

	printf("Creating RX endpoint '%s'...\n", ept_rx_info.name);
	ret = ioctl(fd_ctrl, RPMSG_CREATE_EPT_IOCTL, &ept_rx_info);
	if (ret == -1) { perror("ioctl RX"); cleanup_resources(); return -1; }

	/* Discover TX device via sysfs by matching endpoint name */
	{
		char rpmsg_dev_path[64];

		if (find_rpmsg_dev(EPT_TX_NAME, rpmsg_dev_path,
				   sizeof(rpmsg_dev_path)) != 0) {
			printf("ERROR: Cannot find rpmsg dev for '%s'\n",
			       EPT_TX_NAME);
			cleanup_resources();
			return -1;
		}
		fd_tx = open(rpmsg_dev_path, O_RDWR);
		if (fd_tx == -1) {
			perror("open TX rpmsg");
			cleanup_resources();
			return -1;
		}
		printf("Opened TX: %s (matched '%s')\n",
		       rpmsg_dev_path, EPT_TX_NAME);

		/* Discover RX device via sysfs */
		if (find_rpmsg_dev(EPT_RX_NAME, rpmsg_dev_path,
				   sizeof(rpmsg_dev_path)) != 0) {
			printf("ERROR: Cannot find rpmsg dev for '%s'\n",
			       EPT_RX_NAME);
			cleanup_resources();
			return -1;
		}
		fd_rx = open(rpmsg_dev_path, O_RDWR);
		if (fd_rx == -1) {
			perror("open RX rpmsg");
			cleanup_resources();
			return -1;
		}
		printf("Opened RX: %s (matched '%s')\n",
		       rpmsg_dev_path, EPT_RX_NAME);
	}

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);
	printf("Endpoints ready. Starting %d corner-case iterations...\n\n",
	       ITERATIONS);

	for (iter = 0; iter < ITERATIONS && !stop_flag; iter++) {
		uint32_t doorbell_tx, doorbell_rx, byte_idx;
		int nbytes;
		uint8_t xor_key = (uint8_t)(iter + 0x42);
		uint32_t tx_len = test_vectors[iter].len;

		/* Zero-init TX and RX regions to prevent leftover data */
		memset((void *)tx_region, 0, sizeof(struct shared_msg));
		memset((void *)rx_region, 0, sizeof(struct shared_msg));

		printf("=== Iteration %d: %s (len=%u) ===\n",
		       iter + 1, test_vectors[iter].name, tx_len);

		/* Load test pattern */
		tx_region->magic    = MAGIC_A32;
		tx_region->msg_id   = iter;
		tx_region->data_len = tx_len;
		memcpy((void *)tx_region->data, test_vectors[iter].pattern, tx_len);
		tx_region->checksum = calc_checksum(
				(const uint8_t *)tx_region->data, tx_len);

		printf("A32 TX: ");
		{
			uint32_t print_len = (tx_len > 16) ? 16 : tx_len;
			for (byte_idx = 0; byte_idx < print_len; byte_idx++)
				printf("0x%02x ", tx_region->data[byte_idx]);
			if (tx_len > 16)
				printf("... (%u more)", tx_len - 16);
		}
		printf(" (cksum=0x%x)\n", tx_region->checksum);

		doorbell_tx = SHARED_MEM_BASE + A32_TO_HP_OFFSET;
		nbytes = write(fd_tx, &doorbell_tx, sizeof(doorbell_tx));
		if (nbytes == -1) { perror("write TX"); break; }
		if (nbytes != sizeof(doorbell_tx)) {
			printf("ERROR: Partial TX write: %d/%zu bytes\n",
			       nbytes, sizeof(doorbell_tx));
			fail_count++;
			break;
		}

		printf("A32: Waiting for HP response...\n");

		if (iter > 0) {
			struct pollfd pfd = { .fd = fd_rx, .events = POLLIN };
			int pret = poll(&pfd, 1, RX_TIMEOUT_MS);
			if (pret == 0) {
				printf("ERROR: Timeout waiting for HP response "
				       "(%d ms)\n", RX_TIMEOUT_MS);
				fail_count++;
				break;
			}
			if (pret < 0) { perror("poll RX"); break; }
		}

		nbytes = read(fd_rx, &doorbell_rx, sizeof(doorbell_rx));
		if (nbytes <= 0) { perror("read RX"); break; }
		if (nbytes != sizeof(doorbell_rx)) {
			printf("ERROR: Partial RX read: %d/%zu bytes\n",
			       nbytes, sizeof(doorbell_rx));
			fail_count++;
			break;
		}

		/* Validate doorbell RX address */
		if (doorbell_rx != (SHARED_MEM_BASE + HP_TO_A32_OFFSET)) {
			printf("ERROR: Unexpected doorbell addr 0x%08x "
			       "(expected 0x%08x)\n",
			       doorbell_rx,
			       SHARED_MEM_BASE + HP_TO_A32_OFFSET);
			fail_count++;
			continue;
		}

		/* Stale RX check: detect if response hasn't been updated */
		if (rx_region->msg_id != (uint32_t)iter) {
			printf("WARN: Possible stale RX (msg_id=%u, "
			       "expected %d)\n",
			       rx_region->msg_id, iter);
		}

		if (rx_region->magic != MAGIC_HP) {
			printf("ERROR: Bad magic 0x%08x\n", rx_region->magic);
			fail_count++;
			continue;
		}

		if (rx_region->msg_id != (uint32_t)iter) {
			printf("ERROR: msg_id mismatch: got %u, expected %d\n",
			       rx_region->msg_id, iter);
			fail_count++;
			continue;
		}

		/* Bounds-check response data_len */
		uint32_t rx_len = rx_region->data_len;
		if (rx_len == 0 && tx_len > 0) {
			printf("ERROR: Zero-length response for "
			       "non-zero TX (sent %u bytes)\n", tx_len);
			fail_count++;
			continue;
		}
		if (rx_len > MAX_PAYLOAD) {
			printf("ERROR: response data_len %u > MAX_PAYLOAD\n",
			       rx_len);
			fail_count++;
			continue;
		}

		uint32_t computed_cksum = calc_checksum(
				(const uint8_t *)rx_region->data, rx_len);

		printf("HP RX: ");
		{
			uint32_t print_len = (rx_len > 16) ? 16 : rx_len;
			for (byte_idx = 0; byte_idx < print_len; byte_idx++)
				printf("0x%02x ", rx_region->data[byte_idx]);
			if (rx_len > 16)
				printf("... (%u more)", rx_len - 16);
		}
		printf(" (cksum=0x%x/0x%x)\n", computed_cksum, rx_region->checksum);

		/* Verify: response[j] == sent[j] ^ xor_key */
		int data_valid = 1;
		if (rx_len != tx_len) {
			printf("  LENGTH MISMATCH: sent %u, got %u\n",
			       tx_len, rx_len);
			data_valid = 0;
		} else {
			for (byte_idx = 0; byte_idx < tx_len; byte_idx++) {
				uint8_t expected = test_vectors[iter].pattern[byte_idx] ^ xor_key;
				if (rx_region->data[byte_idx] != expected) {
					printf("  MISMATCH [%u]: got 0x%02x, "
					       "expected 0x%02x (0x%02x ^ 0x%02x)\n",
					       byte_idx, rx_region->data[byte_idx], expected,
					       test_vectors[iter].pattern[byte_idx], xor_key);
					data_valid = 0;
					if (byte_idx > 3) {
						printf("  ... (stopping after 4 mismatches)\n");
						break;
					}
				}
			}
		}

		if (computed_cksum == rx_region->checksum && data_valid) {
			printf("[%d] PASS: %s\n\n", iter + 1, test_vectors[iter].name);
			pass_count++;
		} else {
			printf("[%d] FAIL: %s\n\n", iter + 1, test_vectors[iter].name);
			fail_count++;
		}

		if (fail_count >= MAX_ERRORS) {
			printf("ERROR: Too many failures (%d), aborting\n",
			       fail_count);
			break;
		}

		if (iter < ITERATIONS - 1)
			sleep(1);
	}

	printf("==========================================\n");
	printf("Results: %d PASS, %d FAIL out of %d\n",
	       pass_count, fail_count, pass_count + fail_count);
	printf("==========================================\n");

	cleanup_resources();
	return (fail_count > 0) ? 1 : 0;
}
