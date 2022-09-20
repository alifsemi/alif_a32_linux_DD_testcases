#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/if_alg.h>
#include <string.h>
#include <errno.h>

#define CRC_TYPE_CRC32             0
#define CRC_TYPE_CRC32C            1
#define CRC_TYPE_CRC16             2
#define CRC_TYPE_CRC16_CCITT       3
#define CRC_TYPE_CRC8              4
/* CRC32_ALIF_LINUX and CRC32_LINUX should both give the same result */
#define CRC_TYPE_CRC32_ALIF_LINUX  5
#define CRC_TYPE_CRC32_LINUX       6
#define CRC_TYPE_CRC32C_LINUX      7

char output[5] = {'\0'};
int output_len = 4;

int input_len;
char *input;

int crc_type = 0;

void parse_input(int argc, char *argv[])
{
	int len;
	int i = 1;

	if(argc < 3) {
		printf("Usage:%s <crc_type> <input_string>\n", argv[0]);
		printf("valid crc types: crc32, crc32c, crc16, crc16-ccitt, crc8 crc32-alif-linux crc32-linux crc32c-linux\n");
		exit(0);
	}

	if(strcmp(argv[i], "crc32") == 0) {
		crc_type = CRC_TYPE_CRC32;
	} else if(strcmp(argv[i], "crc32c") == 0) {
		crc_type = CRC_TYPE_CRC32C;
	} else if (strcmp(argv[i], "crc16") == 0) {
		crc_type = CRC_TYPE_CRC16;
	} else if (strcmp(argv[i], "crc16-ccitt") == 0) {
		crc_type = CRC_TYPE_CRC16_CCITT;
	} else if (strcmp(argv[i], "crc8") == 0) {
		crc_type = CRC_TYPE_CRC8;
	} else if (strcmp(argv[i], "crc32-alif-linux") == 0) {
		/* Alif implmentation of CRC32 that confirms to the linux sw implementation */
		crc_type = CRC_TYPE_CRC32_ALIF_LINUX;
	} else if (strcmp(argv[i], "crc32-linux") == 0) {
		/* Linux sw implmentation of CRC */
		crc_type = CRC_TYPE_CRC32_LINUX;
	} else if (strcmp(argv[i], "crc32c-linux") == 0) {
		/* Linux sw implmentation of CRC */
		crc_type = CRC_TYPE_CRC32C_LINUX;
	} else {
		printf("Usage:%s <crc_type> <input_string>\n", argv[0]);
		printf("valid crc types: crc32, crc32c, crc16, crc16-ccitt crc8 crc32-alif-linux crc32-linux crc32c-linux\n");
		exit(0);
	}

	i++;

	len = strlen(argv[i]);
	input = calloc(len+1, sizeof(char));
	memcpy(input, argv[i], len);
	input_len = len;
}

int main(int argc, char *argv[])
{
	struct sockaddr_alg sa;
	int fd1;
	int fd2;
	int i;
	int rc;
	int n;
	int len;

	parse_input(argc, argv);

	memset(&sa, 0, sizeof(sa));

	sa.salg_family = AF_ALG;
	strcpy(sa.salg_type, "hash");


	if(crc_type == CRC_TYPE_CRC32) {
		strcpy(sa.salg_name , "alif-crc");
	} else if(crc_type == CRC_TYPE_CRC32C) {
		strcpy(sa.salg_name, "alif-crcc");
	} else if(crc_type == CRC_TYPE_CRC16) {
		strcpy(sa.salg_name, "alif-crc16");
	} else if(crc_type == CRC_TYPE_CRC16_CCITT) {
		strcpy(sa.salg_name, "alif-crc16-ccitt");
	} else if(crc_type == CRC_TYPE_CRC8) {
		strcpy(sa.salg_name, "alif-crc8");
	} else if(crc_type == CRC_TYPE_CRC32_ALIF_LINUX) {
		strcpy(sa.salg_name, "alif-crc-linux");
	} else if(crc_type == CRC_TYPE_CRC32_LINUX) {
		strcpy(sa.salg_name, "crc32");
	} else if(crc_type == CRC_TYPE_CRC32C_LINUX) {
		strcpy(sa.salg_name, "crc32c");
	}

	fd1 = socket(AF_ALG, SOCK_SEQPACKET, 0);

	if(fd1 < 0) {
		printf("socket error\n");
		return 0;
	}

	rc = bind(fd1, (struct sockaddr*) &sa, sizeof(sa));

	if(rc < 0) {
		printf("bind error %s\n", strerror(errno));
		return 0;
	}

	fd2 = accept(fd1, NULL, 0);

	if(fd2 < 0) {
		printf("accept error\n");
		return 0;
	}

	n = write(fd2, input, input_len);

	if(n <= 0) {
		printf("write error\n");
		return 0;
	}

	n = read(fd2, output, output_len);

	if(n <= 0) {
		printf("read error\n");
		return 0;
	}

	printf("input: %s\n", input);

	if(crc_type == CRC_TYPE_CRC32) {
		printf("crc type: crc32\n");
	} else if(crc_type == CRC_TYPE_CRC32C){
		printf("crc type: crc32c\n");
	} else if(crc_type == CRC_TYPE_CRC16) {
		printf("crc type: crc16\n");
	} else if(crc_type == CRC_TYPE_CRC16_CCITT) {
		printf("crc type: crc16-ccitt\n");
	} else if(crc_type == CRC_TYPE_CRC32_ALIF_LINUX) {
		printf("crc type: crc32-alif-linux: Alif crc32 that confirms to linux sw implementaion\n");
	} else if(crc_type == CRC_TYPE_CRC32_LINUX) {
		printf("crc type: crc32-linux: crc32 implementation from linux sw\n");
	} else if(crc_type == CRC_TYPE_CRC32C_LINUX) {
		printf("crc type: crc32-linux: crc32c implementation from linux sw\n");
	} else {
		printf("crc_type: crc8\n");
	}

	printf("Result is: %x %x %x %x\n", output[3], output[2], output[1], output[0]);

	return 0;
}
