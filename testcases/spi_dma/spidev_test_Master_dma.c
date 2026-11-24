// SPDX-License-Identifier: GPL-2.0-only /* Alif SPI master DMA v7 */

#define _GNU_SOURCE
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/spi/spidev.h>

typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;
typedef __u64 u64;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

enum spi_transfer_dir {
	SPI_TRANSFER_TXRX,
	SPI_TRANSFER_TX,
	SPI_TRANSFER_RX,
};

#define SPI_SLAVE_READY_FILE "/tmp/spi.slave.ready"
#define SPI_SLAVE_TIMING_FILE "/tmp/spi.slave.timing"
#define SPI_SLAVE_SPEED_FILE "/tmp/spi.slave.speed"
#define SPI_XFER_LEN (8 * 1024 * 1024)
#define SPI_XFER_DEFAULT 4096
#define SPI_LOOPBACK_SPEED_HZ 1000000U
#define SPI_LOG_EDGE 64
#define SPI_HEX_BYTES_PER_LINE 16
#define SPI_DW_FIFO_ENTRIES 16
#define SPI_BAUDR_CFG_CAP_HZ 100000000U
#define SPI_DW_BAUDR_DIV_MAX 65534U
#define SPI_DW_BAUDR_DIV_MIN 2U
#define SPI3_SSI_CLK_HZ 200000000U
#define SPI_SLAVE_READY_TIMEOUT_MS 5000
#define SPI_SLAVE_TIMING_TIMEOUT_MS 1000
#define SPI_SLAVE_FILE_POLL_US 1000
#define SPI_DEFAULT_DEVICE "/dev/spidev1.0"
#define SPI_DEFAULT_MODE_NUM 3
#define SPI_BITS_PER_WORD_DEFAULT 8
#define SPI_BITS_PER_WORD_MIN 4
#define SPI_BITS_PER_WORD_MAX 32
#define SPI_CRC32_POLY 0xedb88320U
#define SPI_CRC32_XOROUT 0xffffffffU
#define SPI_RX_PREFIX_VERIFY_LEN 4

static const char *device = SPI_DEFAULT_DEVICE;
static int spi_mode_num = SPI_DEFAULT_MODE_NUM;
static int spi_dump_full_data;
static int spi_spidev_slave_loopback;
static int spi_external_rx_check;
static u8 spi_external_rx_prefix[4] = { 0xaa, 0xbb, 0xcc, 0x11 };
static u32 mode;
static u8 bits = SPI_BITS_PER_WORD_DEFAULT;
static char *input_tx;
static u32 speed = SPI_LOOPBACK_SPEED_HZ;
static u32 speed_hz_req;
static u16 delay;
static size_t transfer_size;
static enum spi_transfer_dir transfer_dir = SPI_TRANSFER_TXRX;

static const char *spi_transfer_dir_label(enum spi_transfer_dir dir)
{
	static const char *const names[] = { "TX+RX", "TX only", "RX only" };

	return (unsigned int)dir < ARRAY_SIZE(names) ? names[dir] : "?";
}

static long long spi_diff_ns(const struct timespec *t0, const struct timespec *t1)
{
	return (t1->tv_sec - t0->tv_sec) * 1000000000LL +
	       (t1->tv_nsec - t0->tv_nsec);
}

static inline long long spi_elapsed_ms(const struct timespec *t0,
				       const struct timespec *t1)
{
	return (t1->tv_sec - t0->tv_sec) * 1000LL +
	       (t1->tv_nsec - t0->tv_nsec) / 1000000LL;
}

/* Mirror dw_spi: roundup_pow_of_two(ceil(bpw/8)) */
static unsigned int spi_dw_n_bytes(u8 bpw)
{
	unsigned int r = (bpw + 7U) / 8U, n = 1;

	while (n < r)
		n <<= 1;
	return n;
}

static int spi_bpw_odd_slot(u8 bpw)
{
	return bpw != spi_dw_n_bytes(bpw) * 8U;
}

static u32 spi_bpw_mask(u8 bpw)
{
	return bpw >= 32 ? UINT32_MAX : ((1U << bpw) - 1U);
}

static u32 spi_unpack_word(const u8 *s, unsigned int nb, u8 bpw)
{
	u32 w = 0;

	memcpy(&w, s, nb);
	return w & spi_bpw_mask(bpw);
}

static void spi_pack_word(u8 *d, u32 w, unsigned int nb, u8 bpw)
{
	w &= spi_bpw_mask(bpw);
	memcpy(d, &w, nb);
}

static const u8 spi_pat_master_hdr[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x40, 0x00, 0x00, 0x00, 0x00, 0x95,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0x0D, 0x33, 0x40, 0x41, 0x42, 0x43,
	0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B,
	0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73,
	0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B,
	0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E
};

static const u8 spi_pat_slave_hdr[] = {
	0xBA, 0xAD, 0xCA, 0xFE, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xBA, 0xBB, 0xBC,
	0xBD, 0xBE, 0xBF, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xDA, 0xDB, 0xDC,
	0xDD, 0xDE, 0xDF, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0x33, 0x40, 0x41, 0x42,
	0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E,
	0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,
	0x5B, 0x5C, 0x5D, 0x5E, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83,
	0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B,
	0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
	0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF
};

static void spi_build_pattern(u8 *buf, size_t len, u8 bpw, int slave_pat)
{
	const u8 *hdr = slave_pat ? spi_pat_slave_hdr : spi_pat_master_hdr;
	size_t hlen = slave_pat ? sizeof(spi_pat_slave_hdr) :
				  sizeof(spi_pat_master_hdr);
	unsigned int nb = spi_dw_n_bytes(bpw);
	size_t nwords = len / nb;
	size_t i;

	if (!buf || !len)
		return;

	if (!spi_bpw_odd_slot(bpw)) {
		memcpy(buf, hdr, hlen < len ? hlen : len);
		for (i = hlen; i < len; i++)
			buf[i] = slave_pat ? (u8)(0x70 + (i - 64)) : (u8)i;
		return;
	}

	for (i = 0; i < nwords; i++) {
		u32 v = i < hlen ? hdr[i] :
			slave_pat ? 0x70U + (u32)(i - 64) : (u32)i;

		spi_pack_word(buf + i * nb, v, nb, bpw);
	}
	if (len % nb)
		memset(buf + nwords * nb, 0, len % nb);
}

static u32 spi_crc32_run(u32 crc, u8 byte)
{
	unsigned int b;

	crc ^= byte;
	for (b = 0; b < 8; b++)
		crc = (crc >> 1) ^ (SPI_CRC32_POLY & (u32)(-(crc & 1)));
	return crc;
}

static u32 spi_crc32_buf_bpw(const u8 *data, size_t len, u8 bpw)
{
	unsigned int nb = spi_dw_n_bytes(bpw);
	size_t i, n = spi_bpw_odd_slot(bpw) ? len / nb : len;
	u32 crc = SPI_CRC32_XOROUT;

	for (i = 0; i < n; i++) {
		if (spi_bpw_odd_slot(bpw)) {
			u32 w = spi_unpack_word(data + i * nb, nb, bpw);
			u8 b[4];

			memcpy(b, &w, 4);
			crc = spi_crc32_run(crc, b[0]);
			crc = spi_crc32_run(crc, b[1]);
			crc = spi_crc32_run(crc, b[2]);
			crc = spi_crc32_run(crc, b[3]);
		} else {
			crc = spi_crc32_run(crc, data[i]);
		}
	}
	return crc ^ SPI_CRC32_XOROUT;
}

static int spi_verify_peer_rx(const u8 *rx, size_t len, u8 bpw,
			      int peer_slave_pat, size_t max_len,
			      u32 *peer_crc, u32 *rx_crc)
{
	u8 *expect;
	unsigned int nb = spi_dw_n_bytes(bpw);
	size_t nw = len / nb;
	size_t i;

	if (len > max_len)
		return 0;
	expect = malloc(len);
	if (!expect)
		return 0;
	spi_build_pattern(expect, len, bpw, peer_slave_pat);
	*peer_crc = spi_crc32_buf_bpw(expect, len, bpw);
	*rx_crc = spi_crc32_buf_bpw(rx, len, bpw);
	for (i = 0; i < nw; i++) {
		if (spi_unpack_word(expect + i * nb, nb, bpw) !=
		    spi_unpack_word(rx + i * nb, nb, bpw)) {
			free(expect);
			return 0;
		}
	}
	free(expect);
	return 1;
}

static inline u32 dw_spi_effective_hz(u32 max_freq, u32 req_hz)
{
	u64 clk_div;

	if (!req_hz)
		req_hz = 1;
	if (req_hz > SPI_BAUDR_CFG_CAP_HZ)
		req_hz = SPI_BAUDR_CFG_CAP_HZ;
	clk_div = ((u64)max_freq + req_hz - 1) / req_hz;
	clk_div = (clk_div + 1) & ~1ULL;
	if (clk_div > SPI_DW_BAUDR_DIV_MAX)
		clk_div = SPI_DW_BAUDR_DIV_MAX;
	if (clk_div < SPI_DW_BAUDR_DIV_MIN)
		clk_div = SPI_DW_BAUDR_DIV_MIN;
	return (u32)(max_freq / clk_div);
}

static unsigned long parse_ulong(const char *arg, const char *name,
				 unsigned long min, unsigned long max)
{
	char *end;
	unsigned long value;

	errno = 0;
	value = strtoul(arg, &end, 0);
	if (errno || end == arg || *end || value < min || value > max) {
		fprintf(stderr, "invalid %s: %s\n", name, arg);
		exit(EXIT_FAILURE);
	}
	return value;
}

/* Copy src into dst, converting \xHH escapes; returns length or -1 */
static ssize_t unescape(u8 *dst, const char *src, size_t dst_cap)
{
	size_t out = 0;

	while (*src) {
		if (out >= dst_cap)
			return -1;
		if (src[0] == '\\' && src[1] == 'x') {
			char hex[3] = { src[2], src[2] ? src[3] : 0, 0 };

			if (!isxdigit((unsigned char)hex[0]) ||
			    !isxdigit((unsigned char)hex[1]))
				return -1;
			dst[out++] = (u8)strtoul(hex, NULL, 16);
			src += 4;
		} else {
			dst[out++] = (u8)*src++;
		}
	}
	return (ssize_t)out;
}

static void spi_print_hex_lines(FILE *fp, const u8 *buf, size_t nbytes)
{
	size_t i;

	for (i = 0; i < nbytes; i++) {
		fprintf(fp, "%02X", buf[i]);
		if ((i + 1) % SPI_HEX_BYTES_PER_LINE == 0 || i + 1 == nbytes)
			fputc('\n', fp);
		else
			fputc(' ', fp);
	}
}

static void spi_print_words_chunk(FILE *fp, const u8 *buf, size_t len, u8 bpw)
{
	unsigned int nb = spi_dw_n_bytes(bpw);
	size_t nwords = len / nb;
	unsigned int width = (bpw + 3U) / 4U;
	size_t i;

	if (width < 2)
		width = 2;
	if (width > 8)
		width = 8;
	for (i = 0; i < nwords; i++) {
		fprintf(fp, "%0*X", width,
			spi_unpack_word(buf + i * nb, nb, bpw));
		if ((i + 1) % 16 == 0 || i + 1 == nwords)
			fputc('\n', fp);
		else
			fputc(' ', fp);
	}
}

static void spi_log_edge_or_full(FILE *fp, const u8 *buf, size_t len, u8 bpw,
				 int as_words)
{
	unsigned int nb = spi_dw_n_bytes(bpw);
	size_t n = as_words ? len / nb : len;
	size_t edge = n < SPI_LOG_EDGE ? n : SPI_LOG_EDGE;
	size_t elen = as_words ? edge * nb : edge;

	if (spi_dump_full_data) {
		fprintf(fp, as_words ? "Complete %zu words:\n" :
			"Complete %zu Bytes:\n", n);
		if (as_words)
			spi_print_words_chunk(fp, buf, len, bpw);
		else
			spi_print_hex_lines(fp, buf, len);
		return;
	}
	fprintf(fp, as_words ? "First %zu words:\n" : "First %zu Bytes:\n", edge);
	if (as_words)
		spi_print_words_chunk(fp, buf, elen, bpw);
	else
		spi_print_hex_lines(fp, buf, elen);
	if (n > edge) {
		fprintf(fp, as_words ? "\nLast %zu words:\n" :
			"Last %zu Bytes:\n", edge);
		if (as_words)
			spi_print_words_chunk(fp, buf + len - elen, elen, bpw);
		else
			spi_print_hex_lines(fp, buf + len - elen, elen);
	}
}

static void spi_log_data_direction(FILE *fp, const char *title, const u8 *buf,
				   size_t len, u8 bpw, const char *inactive)
{
	unsigned int nb = spi_dw_n_bytes(bpw);

	fprintf(fp, "---------------- %s ----------------\n", title);
	if (!buf) {
		fprintf(fp, "%s\n", inactive);
		return;
	}
	if (bpw != 8)
		fprintf(fp,
			"SPI words (%u-bit, %zu words; LE u%u, matches Saleae word):\n",
			bpw, len / nb, nb * 8U);
	spi_log_edge_or_full(fp, buf, len, bpw, bpw != 8);
}

static void spi_log_tx_rx_edge(FILE *fp, const char *role, const u8 *tx,
			       const u8 *rx, size_t len)
{
	fprintf(fp, "====================================================\n");
	fprintf(fp, "                 %s DATA LOGS\n", role);
	fprintf(fp, "====================================================\n");
	fprintf(fp, "Device: %s\n", device);
	spi_log_data_direction(fp, "TRANSMIT DATA (TX)", tx, len, bits,
			       "(TX disabled: tx_buf=0)");
	spi_log_data_direction(fp, "RECEIVE DATA (RX)", rx, len, bits,
			       "(RX disabled: rx_buf=0)");
	fprintf(fp, "====================================================\n");
}

/* peer_slave_pat: master -B uses 1 (expect slave TX pattern in RX) */
static int spi_print_dma_summary(FILE *fp, const char *role_banner, size_t len,
				 int xfer_ret, u32 ssi_hz,
				 const struct timespec *t0,
				 const struct timespec *t1, const u8 *rx,
				 int crc_verify, int peer_slave_pat)
{
	u32 req_hz = speed_hz_req ? speed_hz_req : speed;
	u32 eff_hz, nbits = bits ? bits : SPI_BITS_PER_WORD_DEFAULT;
	unsigned long long bus_ns;
	long long wall_ns;
	int full_ok = 1;
	u32 peer_crc = 0, rx_crc = 0;

	if (!req_hz)
		req_hz = 1;
	eff_hz = dw_spi_effective_hz(ssi_hz, req_hz);
	bus_ns = ((unsigned long long)len * nbits * 1000000000ULL) / eff_hz;
	wall_ns = spi_diff_ns(t0, t1);

	fprintf(fp, "========== SPI %s SUMMARY ==========\n", role_banner);
	fprintf(fp, "Device       : %s\n", device);
	fprintf(fp, "Transfer     : %zu Bytes, %s\n", len,
		spi_transfer_dir_label(transfer_dir));
	fprintf(fp, "Clock-mode   : %d\n", spi_mode_num & 3);
	fprintf(fp, "BPW          : %u-bit\n", nbits);
	fprintf(fp, "Frequency    : %.3f MHz\n", eff_hz / 1e6);
	fprintf(fp, "\nBus Time     : %.3f ms\n", bus_ns / 1e6);
	fprintf(fp, "IOCTL Time   : %.3f ms\n", wall_ns / 1e6);
	fprintf(fp, "Overhead     : %.3f ms\n",
		(wall_ns - (long long)bus_ns) / 1e6);
	fprintf(fp, "===========================================\n\n");

	fprintf(fp, "TRANSFER STATUS:\n");
	if (xfer_ret < (int)len) {
		fprintf(fp, "%-23s: NOT CHECKED\n", "Peer TX CRC32");
		fprintf(fp, "%-23s: NOT CHECKED\n", "RX buffer CRC32");
		fprintf(fp, "%-23s: %s\n", "Full buffer verification",
			"FAIL (incomplete transfer)");
		fprintf(fp, "%-23s: %s\n", "Final Result", "FAIL");
		fprintf(fp, "===================================================\n");
		return 1;
	}
	/*
	 * -T 1 (TX only) and master without -B: always print NOT CHECKED
	 * (no RX buffer or no peer CRC check requested).
	 */
	if (transfer_dir != SPI_TRANSFER_TX && crc_verify && rx) {
		full_ok = spi_verify_peer_rx(rx, len, nbits, peer_slave_pat,
					     SPI_XFER_LEN, &peer_crc, &rx_crc);
		fprintf(fp, "%-23s: 0x%08X\n", "Peer TX CRC32", peer_crc);
		fprintf(fp, "%-23s: 0x%08X\n", "RX buffer CRC32", rx_crc);
		fprintf(fp, "%-23s: %s\n", "Full buffer verification",
			full_ok ? "PASS (CRC32 match)"
				: "FAIL (CRC32 mismatch)");
	} else {
		fprintf(fp, "%-23s: NOT CHECKED\n", "Peer TX CRC32");
		fprintf(fp, "%-23s: NOT CHECKED\n", "RX buffer CRC32");
		fprintf(fp, "%-23s: NOT CHECKED\n", "Full buffer verification");
		full_ok = 1;
	}
	fprintf(fp, "%-23s: %s\n", "Final Result",
		(xfer_ret >= (int)len && full_ok) ? "PASS" : "FAIL");
	fprintf(fp, "===================================================\n");
	return (xfer_ret >= (int)len && full_ok) ? 0 : 1;
}

static int spi_master_wait_file(const char *path, int timeout_ms)
{
	struct timespec t0, now;

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (;;) {
		if (access(path, F_OK) == 0)
			return 0;
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (spi_elapsed_ms(&t0, &now) >= timeout_ms) {
			fprintf(stderr,
				"timed out waiting for %s (%d ms). Start the slave first.\n",
				path, timeout_ms);
			return -1;
		}
		usleep(SPI_SLAVE_FILE_POLL_US);
	}
}

/* First 4 RX bytes vs expect; TRANSFER STATUS line. Returns 1 on FAIL. */
static int spi_print_prefix_4b_status(FILE *fp, const u8 *rx, size_t len,
				      const u8 *expect, const char *label)
{
	int ok = len >= SPI_RX_PREFIX_VERIFY_LEN &&
		 !memcmp(rx, expect, SPI_RX_PREFIX_VERIFY_LEN);

	fprintf(fp, "TRANSFER STATUS:\n");
	fprintf(fp, "%-23s: [%02X %02X %02X %02X] %s\n", label,
		expect[0], expect[1], expect[2], expect[3],
		ok ? "PASS" : "FAIL");
	if (!ok && len >= SPI_RX_PREFIX_VERIFY_LEN)
		fprintf(fp, "%-23s: [%02X %02X %02X %02X]\n", "RX first 4 bytes",
			rx[0], rx[1], rx[2], rx[3]);
	fprintf(fp, "===================================================\n");
	return ok ? 0 : 1;
}

static u32 spi_ssi_clk_hz(void)
{
	static int warned;

	if (!warned && strcmp(device, SPI_DEFAULT_DEVICE) != 0) {
		warned = 1;
		fprintf(stderr,
			"warning: BAUDR report assumes SPI master SSI %u Hz; device is %s\n",
			SPI3_SSI_CLK_HZ, device);
	}
	return SPI3_SSI_CLK_HZ;
}

static int spi_master_check_slave_speed(u32 master_xfer_hz)
{
	FILE *f = fopen(SPI_SLAVE_SPEED_FILE, "r");
	unsigned int slave_eff = 0;
	u32 ssi_hz = spi_ssi_clk_hz();
	u32 master_eff = dw_spi_effective_hz(ssi_hz, master_xfer_hz);

	if (!f) {
		fprintf(stderr,
			"ERROR: %s missing; slave must publish its effective SCLK (use the same -s on both sides)\n",
			SPI_SLAVE_SPEED_FILE);
		return -1;
	}
	if (fscanf(f, "%u", &slave_eff) != 1) {
		fprintf(stderr, "ERROR: invalid %s (expected effective Hz)\n",
			SPI_SLAVE_SPEED_FILE);
		fclose(f);
		return -1;
	}
	fclose(f);
	if (slave_eff != master_eff) {
		fprintf(stderr,
			"ERROR: slave effective SCLK %u Hz != master effective %u Hz\n"
			"       (master xfer %u Hz, ssi %u). Use same -s on both; default -s %u -> %u Hz wire.\n",
			slave_eff, master_eff, master_xfer_hz, ssi_hz,
			SPI_LOOPBACK_SPEED_HZ,
			dw_spi_effective_hz(ssi_hz, SPI_LOOPBACK_SPEED_HZ));
		return -1;
	}
	return 0;
}

static void spi_post_rx_checks(FILE *fp, const u8 *tx, const u8 *rx, size_t len,
			       int tx_en, int *status)
{
	if (spi_spidev_slave_loopback || !rx ||
	    transfer_dir == SPI_TRANSFER_TX)
		return;
	if (spi_external_rx_check) {
		if (spi_print_prefix_4b_status(fp, rx, len,
					       spi_external_rx_prefix,
					       "Data check (4-byte)"))
			*status = EXIT_FAILURE;
		return;
	}
	if (!(mode & SPI_LOOP))
		return;
	if (input_tx && tx_en) {
		if (spi_print_prefix_4b_status(fp, rx, len, tx,
					       "Data check (loopback)"))
			*status = EXIT_FAILURE;
	} else {
		printf("LOOPBACK: RX check skipped (needs -p and TX enabled)\n");
	}
}

static int transfer(int fd, const u8 *tx, u8 *rx, size_t len)
{
	int ret, status = EXIT_SUCCESS;
	struct timespec t0, t1;
	struct spi_ioc_transfer tr = {
		.tx_buf = transfer_dir != SPI_TRANSFER_RX ?
			  (__u64)(uintptr_t)tx : 0,
		.rx_buf = transfer_dir != SPI_TRANSFER_TX ?
			  (__u64)(uintptr_t)rx : 0,
		.len = len,
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	if (spi_spidev_slave_loopback) {
		unlink(SPI_SLAVE_TIMING_FILE);
		if (spi_master_wait_file(SPI_SLAVE_READY_FILE,
					 SPI_SLAVE_READY_TIMEOUT_MS))
			return EXIT_FAILURE;
		if (spi_master_check_slave_speed(speed) < 0)
			return EXIT_FAILURE;
	}
	if (tr.rx_buf)
		memset(rx, 0, len);
	clock_gettime(CLOCK_MONOTONIC, &t0);
	do {
		ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	} while (ret < 0 && errno == EINTR);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	if (ret < 0) {
		perror("SPI_IOC_MESSAGE");
		if (errno == EMSGSIZE)
			fprintf(stderr,
				"%s: hint: exceeds spidev bufsiz (default 4096); load spidev with a larger bufsiz\n",
				__func__);
		return EXIT_FAILURE;
	}
	if ((size_t)ret != len) {
		fprintf(stderr, "%s: partial SPI message: %d of %zu bytes\n",
			__func__, ret, len);
		spi_print_dma_summary(stdout, "MASTER", len, ret,
				      spi_ssi_clk_hz(), &t0, &t1, rx, 0, 1);
		fflush(stdout);
		return EXIT_FAILURE;
	}
	if (spi_print_dma_summary(stdout, "MASTER", len, ret,
				  spi_ssi_clk_hz(), &t0, &t1, rx,
				  spi_spidev_slave_loopback &&
				  transfer_dir != SPI_TRANSFER_TX && tr.rx_buf,
				  1))
		status = EXIT_FAILURE;
	spi_log_tx_rx_edge(stdout, "MASTER", tr.tx_buf ? tx : NULL,
			   tr.rx_buf ? rx : NULL, len);
	spi_post_rx_checks(stdout, tx, rx, len, !!tr.tx_buf, &status);
	fflush(stdout);
	if (spi_spidev_slave_loopback &&
	    spi_master_wait_file(SPI_SLAVE_TIMING_FILE,
				 SPI_SLAVE_TIMING_TIMEOUT_MS) == 0)
		unlink(SPI_SLAVE_TIMING_FILE);
	return status;
}

static void spi_usage(FILE *fp, const char *prog)
{
	fprintf(fp, "Usage: %s [options]\n", prog);
	fprintf(fp,
		"  -h       help  -B       spidev_test_Slave_* CRC loopback\n"
		"  -D dev   (default %s)  -s Hz  -m mode0-3 (def %d)  -d usec delay\n"
		"  -b bpw4-32 (def %u)  -l SPI_LOOP (needs -T0; -p checks 1st 4 RX)\n"
		"  -p TX escapes  -S bytes (def %d, max %d)  -T 0/1/2  -F full hex dump\n"
		"  -e [4B]  external: 1st 4 RX (def AA BB CC 11); stamps 1st 4 TX if no -p\n",
		SPI_DEFAULT_DEVICE, SPI_DEFAULT_MODE_NUM,
		SPI_BITS_PER_WORD_DEFAULT, SPI_XFER_DEFAULT, SPI_XFER_LEN);
}

static void parse_opts(int argc, char *argv[])
{
	int c, external_e_got_data;

	external_e_got_data = 0;
	while ((c = getopt(argc, argv, "D:s:d:b:lp:S:m:T:FBe::h")) != -1) {
		switch (c) {
		case 'D':
			device = optarg;
			break;
		case 's':
			speed = (u32)parse_ulong(optarg, "speed (Hz)", 1,
						0xffffffffUL);
			speed_hz_req = speed;
			break;
		case 'd':
			delay = (u16)parse_ulong(optarg, "delay (usec)", 0, 65535);
			break;
		case 'b':
			bits = (u8)parse_ulong(optarg, "bits per word",
					       SPI_BITS_PER_WORD_MIN,
					       SPI_BITS_PER_WORD_MAX);
			break;
		case 'l':
			mode |= SPI_LOOP;
			break;
		case 'p':
			input_tx = optarg;
			break;
		case 'S':
			transfer_size = parse_ulong(optarg, "transfer size", 0,
						    SPI_XFER_LEN);
			break;
		case 'm':
			spi_mode_num = (int)parse_ulong(optarg, "SPI mode", 0, 3);
			break;
		case 'T':
			transfer_dir = (enum spi_transfer_dir)parse_ulong(optarg,
				"transfer direction", 0, 2);
			break;
		case 'F':
			spi_dump_full_data = 1;
			break;
		case 'B':
			spi_spidev_slave_loopback = 1;
			break;
		case 'e':
			spi_external_rx_check = 1;
			if (optarg &&
			    unescape(spi_external_rx_prefix, optarg,
				     sizeof(spi_external_rx_prefix)) !=
			    SPI_RX_PREFIX_VERIFY_LEN) {
				fprintf(stderr,
					"error: -e needs 4 bytes (e.g. \"\\xaa\\xbb\\xcc\\x11\")\n");
				exit(EXIT_FAILURE);
			}
			if (optarg)
				external_e_got_data = 1;
			break;
		case 'h':
			spi_usage(stdout, argv[0]);
			exit(EXIT_SUCCESS);
		default:
			spi_usage(stderr, argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	if (spi_external_rx_check && !external_e_got_data && optind < argc &&
	    argv[optind][0] != '-') {
		if (unescape(spi_external_rx_prefix, argv[optind],
			     sizeof(spi_external_rx_prefix)) !=
		    SPI_RX_PREFIX_VERIFY_LEN) {
			fprintf(stderr,
				"error: -e needs 4 bytes (e.g. \"\\xaa\\xbb\\xcc\\x11\")\n");
			exit(EXIT_FAILURE);
		}
		external_e_got_data = 1;
		optind++;
	}
	if (optind < argc) {
		fprintf(stderr, "unexpected argument: %s\n", argv[optind]);
		exit(EXIT_FAILURE);
	}
	if (spi_spidev_slave_loopback && (mode & SPI_LOOP)) {
		fprintf(stderr, "error: -B cannot be combined with -l\n");
		exit(EXIT_FAILURE);
	}
	if (input_tx && transfer_dir == SPI_TRANSFER_RX) {
		fprintf(stderr, "error: -p cannot be used with -T 2 (RX only)\n");
		exit(EXIT_FAILURE);
	}
	if ((mode & SPI_LOOP) && transfer_dir != SPI_TRANSFER_TXRX) {
		fprintf(stderr, "error: -l requires -T 0 (TX+RX)\n");
		exit(EXIT_FAILURE);
	}
	mode = (mode & ~(SPI_CPOL | SPI_CPHA)) |
	       (spi_mode_num & 1 ? SPI_CPHA : 0) |
	       (spi_mode_num & 2 ? SPI_CPOL : 0);
}

int main(int argc, char *argv[])
{
	static const struct {
		unsigned long req;
		void *arg;
		const char *err;
	} cfg[] = {
		{ SPI_IOC_WR_MODE32, &mode, "can't set spi mode" },
		{ SPI_IOC_RD_MODE32, &mode, "can't get spi mode" },
		{ SPI_IOC_WR_BITS_PER_WORD, &bits, "can't set bits per word" },
		{ SPI_IOC_RD_BITS_PER_WORD, &bits, "can't get bits per word" },
		{ SPI_IOC_WR_MAX_SPEED_HZ, &speed, "can't set max speed hz" },
		{ SPI_IOC_RD_MAX_SPEED_HZ, &speed, "can't get max speed hz" },
	};
	int fd = -1, ret = EXIT_FAILURE;
	size_t xfer_len, i;
	u32 mode_req;
	u8 *tx = NULL, *rx = NULL;

	parse_opts(argc, argv);
	if (!speed_hz_req)
		speed_hz_req = speed;
	xfer_len = transfer_size ? transfer_size : SPI_XFER_DEFAULT;
	tx = malloc(xfer_len);
	rx = malloc(xfer_len);
	if (!tx || !rx) {
		fprintf(stderr, "can't allocate transfer buffers\n");
		goto out;
	}
	if (transfer_dir != SPI_TRANSFER_RX) {
		ssize_t n;

		if (!input_tx) {
			spi_build_pattern(tx, xfer_len, bits, 0);
		} else {
			n = unescape(tx, input_tx, xfer_len);
			if (n < 0) {
				fprintf(stderr, "invalid -p TX data\n");
				goto out;
			}
			if ((size_t)n < xfer_len)
				memset(tx + n, 0, xfer_len - n);
		}
		if (spi_external_rx_check && !input_tx)
			memcpy(tx, spi_external_rx_prefix,
			       SPI_RX_PREFIX_VERIFY_LEN);
	}

	fd = open(device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "cannot open \"%s\": ", device);
		perror("");
		goto out;
	}
	mode_req = mode;
	for (i = 0; i < ARRAY_SIZE(cfg); i++) {
		if (ioctl(fd, cfg[i].req, cfg[i].arg) == -1) {
			perror(cfg[i].err);
			goto out;
		}
	}
	if (mode != mode_req)
		fprintf(stderr, "warning: SPI mode: requested 0x%x, read back 0x%x\n",
			mode_req, mode);
	if (speed != speed_hz_req)
		fprintf(stderr, "warning: SPI max speed: requested %u Hz, driver %u Hz\n",
			speed_hz_req, speed);
	if (!bits)
		bits = SPI_BITS_PER_WORD_DEFAULT;
	if (xfer_len % spi_dw_n_bytes(bits)) {
		fprintf(stderr, "error: -S %zu not multiple of %u-byte word\n",
			xfer_len, spi_dw_n_bytes(bits));
		goto out;
	}
	ret = transfer(fd, tx, rx, xfer_len);
out:
	free(rx);
	free(tx);
	if (fd >= 0)
		close(fd);
	return ret;
}
