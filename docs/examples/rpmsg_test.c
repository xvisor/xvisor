/**
 * Copyright (c) 2018 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file rpmsg_test.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for Rpmsg user-space test
 *
 * This code has been largely adapted from meta-openamp sources:
 * recipes-openamp/rpmsg-examples/rpmsg-echo-test/echo_test.c
 *
 * The meta-openamp sources are located at:
 * https://github.com/OpenAMP/meta-openamp
 *
 * The original source is licensed under BSD 3-clause.
 */

/*
 * Test application to checks data integraty and bandwidth
 * of inter processor communication from linux userspace
 * to a remote software context. The application sends chunks
 * of data to the remote processor. The remote side echoes
 * the data back to application which then validates the data
 * returned by remote processor.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/rpmsg.h>

struct rpmsg_test_payload {
	unsigned long num;
	unsigned long size;
	unsigned long flags;
	char data[];
};

static int charfd = -1, fd = -1, err_cnt;

struct rpmsg_test_payload *i_payload;
struct rpmsg_test_payload *r_payload;

#define RPMSG_HEADER_LEN			16

#define RPMSG_TEST_HEADER_LEN			(3 * sizeof(unsigned long))
#define RPMSG_TEST_FLAG_ECHO			(1 << 0)

#define RPMSG_TEST_MAX_BUFF_SIZE(rmax)		((rmax) - RPMSG_HEADER_LEN)
#define RPMSG_TEST_PAYLOAD_MIN_SIZE(rmax)	1
#define RPMSG_TEST_PAYLOAD_MAX_SIZE(rmax)	\
	(RPMSG_TEST_MAX_BUFF_SIZE(rmax) - RPMSG_TEST_HEADER_LEN)
#define RPMSG_TEST_NUM_PAYLOADS(rmax)			\
	(RPMSG_TEST_PAYLOAD_MAX_SIZE(rmax) / RPMSG_TEST_PAYLOAD_MIN_SIZE(min))

unsigned long long rpmsg_timestamp_usecs(void)
{
	struct timeval  tv;
	struct timezone tz;
 
	gettimeofday(&tv, &tz);
	return (((unsigned long long)tv.tv_sec) * ((unsigned long long)1000000))
		+ ((unsigned long long)tv.tv_usec);
}

int rpmsg_create_ept(int rpfd, struct rpmsg_endpoint_info *eptinfo)
{
	int ret;

	ret = ioctl(rpfd, RPMSG_CREATE_EPT_IOCTL, eptinfo);
	if (ret)
		perror("Failed to create endpoint.\n");
	return ret;
}

char *get_rpmsg_ept_dev_name(char *rpmsg_char_name, char *ept_name,
			     char *ept_dev_name)
{
	char sys_rpmsg_ept_name_path[64];
	char svc_name[64];
	char *sys_rpmsg_path = "/sys/class/rpmsg";
	FILE *fp;
	int i;
	int ept_name_len;

	for (i = 0; i < 128; i++) {
		sprintf(sys_rpmsg_ept_name_path, "%s/%s/rpmsg%d/name",
			sys_rpmsg_path, rpmsg_char_name, i);
		fp = fopen(sys_rpmsg_ept_name_path, "r");
		if (!fp) {
			printf("ERROR: failed to open %s\n",
				sys_rpmsg_ept_name_path);
			break;
		}
		fgets(svc_name, sizeof(svc_name), fp);
		fclose(fp);
		ept_name_len = strlen(ept_name);
		if (ept_name_len > sizeof(svc_name))
			ept_name_len = sizeof(svc_name);
		if (!strncmp(svc_name, ept_name, ept_name_len)) {
			sprintf(ept_dev_name, "rpmsg%d", i);
			return ept_dev_name;
		}
	}

	printf("Not able to create RPMsg endpoint file for %s:%s.\n",
		rpmsg_char_name, ept_name);
	return NULL;
}

void usage(const char *app)
{
	printf("Usage: %s -d <rpmsg_device_path> [<options>]\n", app);
	printf("Common options:\n");
	printf("\t-d <rpmsg_device_path>         - Rpmsg device path (Mandatory)\n");
	printf("\t-h                             - Display this help (Optional)\n");
	printf("\t-r <rpmsg_max_kernel_buf_size> - Rpmsg max kernel buffer size (Optional)\n");
	printf("\t-s                             - Server mode (Optional)\n");
	printf("\t-v                             - Verbose (Optional)\n");
	printf("Client options:\n");
	printf("\t-f <fixed_payload_size>        - Use specified fixed payload size (Optional)\n");
	printf("\t-i                             - Check data integrity (Optional)\n");
	printf("\t-l <local_address>             - Use specified local address (Optional)\n");
	printf("\t-n <number_of_rounds>          - Number of rounds (Optional)\n");
	printf("\t-p <payloads_per_round>        - Number of payloads per round (Optional)\n");
	printf("\t-u                             - Unidirectional transfer (Optional)\n");
}

int main(int argc, char *argv[])
{
	int ret, i, j, tpos, rpos, tbytes, rbytes;
	int size, bytes_rcvd, bytes_sent;
	int rmax = 512;
	int fsize = RPMSG_TEST_PAYLOAD_MIN_SIZE(rmax);
	int fpayload = 0;
	int is_server = 0;
	int check_integrity = 0;
	int is_verbose = 0;
	int is_unidir = 0;
	int local = 0;
	err_cnt = 0;
	int opt;
	char *rpmsg_dev="/dev/rpmsg_ctrl0";
	int ntimes = 1;
	int pcount = RPMSG_TEST_NUM_PAYLOADS(rmax);
	char *rpmsg_char_name;
	unsigned long long tbw, rbw, usecs;
	struct rpmsg_endpoint_info eptinfo;

	while ((opt = getopt(argc, argv, "d:f:hil:n:p:r:suv")) != -1) {
		switch (opt) {
		case 'd':
			rpmsg_dev = optarg;
			break;
		case 'f':
			fsize = atoi(optarg);
			fpayload = 1;
			break;
		case 'i':
			check_integrity = 1;
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
			break;
		case 'l':
			local = atoi(optarg);
			break;
		case 'n':
			ntimes = atoi(optarg);
			if (ntimes < 1)
				ntimes = 1;
			break;
		case 'p':
			pcount = atoi(optarg);
			if (pcount < 1)
				pcount = 1;
			break;
		case 'r':
			rmax = atoi(optarg);
			if (rmax < (RPMSG_HEADER_LEN + RPMSG_TEST_HEADER_LEN + 1))
				rmax = (RPMSG_HEADER_LEN + RPMSG_TEST_HEADER_LEN + 1);
			if (4096 < rmax)
				rmax = 4096;
			break;
		case 's':
			is_server = 1;
			break;
		case 'u':
			is_unidir = 1;
			break;
		case 'v':
			is_verbose = 1;
			break;
		default:
			printf("ERROR: unsupported option: -%c\n", opt);
			usage(argv[0]);
			exit(1);
			break;
		}
	}


	if (fpayload) {
		if (fsize < RPMSG_TEST_PAYLOAD_MIN_SIZE(rmax))
			fsize = RPMSG_TEST_PAYLOAD_MIN_SIZE(rmax);
		else if (RPMSG_TEST_PAYLOAD_MAX_SIZE(rmax) < fsize)
			fsize = RPMSG_TEST_PAYLOAD_MAX_SIZE(rmax);
	} else {
		if (RPMSG_TEST_NUM_PAYLOADS(rmax) < pcount)
			pcount = RPMSG_TEST_NUM_PAYLOADS(rmax);
	}

	if (is_server) {
		printf("Rpmsg test started\n");
		printf("Rpmsg test server mode\n");
	} else {
		printf("Rpmsg test started for %d rounds and "
			"%d payloads in each round\n", ntimes, pcount);
		printf("Rpmsg test client mode\n");
		printf("Rpmsg test %s payload\n",
			(fpayload) ? "fixed" : "dynamic");
		if (is_unidir) {
			printf("Rpmsg test unidirectional transfer\n");
		} else {
			printf("Rpmsg test bidirectional transfer\n");
		}
	}
	printf("Rpmsg test max kernel buffer size %d bytes\n", rmax);
	printf("Rpmsg test max payload size %d bytes (%d bytes)\n",
		(int)RPMSG_TEST_PAYLOAD_MAX_SIZE(rmax),
		(int)RPMSG_TEST_MAX_BUFF_SIZE(rmax));
	printf("Rpmsg test open dev %s!\n", rpmsg_dev);

	fd = open(rpmsg_dev, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		perror("ERROR: Failed to open rpmsg device.");
		return -1;
	}

	rpmsg_char_name = strstr(rpmsg_dev, "rpmsg_ctrl");
	if (rpmsg_char_name != NULL) {
		char ept_dev_name[16];
		char ept_dev_path[32];

		strcpy(eptinfo.name, "rpmsg-test-channel");
		eptinfo.src = (unsigned int)local;
		eptinfo.dst = 0xffffffff;
		ret = rpmsg_create_ept(fd, &eptinfo);
		if (ret) {
			printf("ERROR: failed to create RPMsg endpoint.\n");
			return -1;
		}
		charfd = fd;

		if (!get_rpmsg_ept_dev_name(rpmsg_char_name, eptinfo.name,
					   ept_dev_name))
			return -1;
		sprintf(ept_dev_path, "/dev/%s", ept_dev_name);
		fd = open(ept_dev_path, O_RDWR | O_NONBLOCK);
		if (fd < 0) {
			perror("ERROR: failed to open rpmsg device.");
			close(charfd);
			return -1;
		}
	}

	i_payload = malloc(RPMSG_TEST_MAX_BUFF_SIZE(rmax));
	r_payload = malloc(RPMSG_TEST_MAX_BUFF_SIZE(rmax));

	if (i_payload == 0 || r_payload == 0) {
		printf("ERROR: failed to allocate memory for payload.\n");
		return -1;
	}

	while (is_server) {
		r_payload->num = 0;
		bytes_rcvd = read(fd, r_payload,
				  RPMSG_TEST_MAX_BUFF_SIZE(rmax));
		while (bytes_rcvd <= 0) {
			usleep(100);
			bytes_rcvd = read(fd, r_payload,
					  RPMSG_TEST_MAX_BUFF_SIZE(rmax));
		}

		if (is_verbose) {
			printf("Rpmsg test Rx #%ld payload %ld total %d\n",
				r_payload->num, r_payload->size, bytes_rcvd);
		}

		if (r_payload->flags & RPMSG_TEST_FLAG_ECHO) {
			size = RPMSG_TEST_HEADER_LEN + r_payload->size;
		} else {
			size = sizeof(r_payload->num);
		}

		bytes_sent = write(fd, r_payload, size);
		while (bytes_sent <= 0) {
			bytes_sent = write(fd, r_payload, size);
		}

		if (is_verbose) {
			printf("Rpmsg test Tx #%ld payload %d total %d\n",
				r_payload->num, size, bytes_sent);
		}
	}

	for (i = 0; i < ntimes; i++) {
		printf("Rpmsg test round %d started\n", i);

		memset(&(i_payload->data[0]), 0xA5,
			RPMSG_TEST_PAYLOAD_MAX_SIZE(rmax));

		usecs = rpmsg_timestamp_usecs();

		err_cnt = 0;
		size = (fpayload) ? fsize :
			RPMSG_TEST_PAYLOAD_MIN_SIZE(rmax);
		tpos = rpos = 0;
		tbytes = rbytes = 0;
		while ((tpos < pcount) || (rpos < pcount)) {
			if (tpos == pcount)
				goto skip_tx;

			i_payload->num = tpos;
			i_payload->size = size;
			i_payload->flags = (is_unidir) ? 0x0 :
						RPMSG_TEST_FLAG_ECHO;

			bytes_sent = write(fd, i_payload,
					   RPMSG_TEST_HEADER_LEN + size);
			if (bytes_sent <= 0) {
				if (is_verbose) {
					printf("ERROR: Tx #%ld failed "
						"(%d error) ..... retrying\n",
						i_payload->num, bytes_sent);
				}
				goto skip_tx;
			}

			if (is_verbose) {
				printf("Rpmsg test Tx #%ld payload %d total"
					" %d\n", i_payload->num, size,
					bytes_sent);
			}

			tpos++;
			tbytes += bytes_sent;
			if (!fpayload)
				size++;

skip_tx:
			if (rpos == pcount)
				continue;

			r_payload->num = 0;
			bytes_rcvd = read(fd, r_payload,
					  RPMSG_TEST_MAX_BUFF_SIZE(rmax));
			if (bytes_rcvd <= 0) {
				if (tpos == pcount)
					usleep(100);
				continue;
			}

			if (is_verbose) {
				printf("Rpmsg test Rx #%ld payload %ld total"
					" %d\n", r_payload->num,
					(is_unidir) ? 0 : r_payload->size,
					bytes_rcvd);
			}

			if (check_integrity && !is_unidir) {
				for (j = 0; j < r_payload->size; j++) {
					if (r_payload->data[j] == 0xA5)
						continue;
					printf("ERROR: data corruption");
					printf(" at index %d\n", j);
					err_cnt++;
					break;
				}
			}

			rpos++;
			rbytes += bytes_rcvd;
		}

		usecs = rpmsg_timestamp_usecs() - usecs;

		tbw = tbytes * 8;
		tbw = tbw * 1000000;
		tbw = tbw / usecs;
		rbw = rbytes * 8;
		rbw = rbw * 1000000;
		rbw = rbw / usecs;

		printf("Rpmsg test round %d time taken %lld usecs\n",
			i, usecs);
		printf("Rpmsg test round %d Tx %d payloads Rx %d payloads "
			"%d errors\n", i, tpos, rpos, err_cnt);
		printf("Rpmsg test round %d Tx %d bytes Rx %d bytes\n",
			i, tbytes, rbytes);
		printf("Rpmsg test round %d Tx %lld bps Rx %lld bps\n",
			i, tbw, rbw);
		printf("Rpmsg test round %d finished\n", i);
	}

	free(i_payload);
	free(r_payload);

	close(fd);
	if (charfd >= 0)
		close(charfd);

	return 0;
}
