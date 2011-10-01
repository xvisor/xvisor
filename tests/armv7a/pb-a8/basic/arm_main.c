/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file arm_main.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM test code main file
 */

#include <arm_io.h>
#include <arm_heap.h>
#include <arm_mmu.h>
#include <arm_irq.h>
#include <arm_timer.h>
#include <arm_string.h>
#include <arm_stdio.h>
#include "dhry.h"

/* Works in supervisor mode */
void arm_init(void)
{
	arm_heap_init();

	arm_irq_setup();

	arm_irq_enable();

	arm_stdio_init();

	arm_timer_init(1000, 1);

	arm_timer_enable();
}

void arm_cmd_help(int argc, char **argv)
{
	arm_puts("help        - List commands and their usage\n");
	arm_puts("\n");
	arm_puts("hi          - Say hi to ARM test code\n");
	arm_puts("\n");
	arm_puts("hello       - Say hello to ARM test code\n");
	arm_puts("\n");
	arm_puts("mmu_setup   - Setup MMU for ARM test code\n");
	arm_puts("\n");
	arm_puts("mmu_test    - Test MMU for ARM test code\n");
	arm_puts("\n");
	arm_puts("mmu_cleanup - Cleanup MMU for ARM test code\n");
	arm_puts("\n");
	arm_puts("sysctl      - Display sysctl registers\n");
	arm_puts("\n");
	arm_puts("timer       - Display timer information\n");
	arm_puts("\n");
	arm_puts("dhrystone   - Dhrystone 2.1 benchmark\n");
	arm_puts("              Usage: dhrystone [<iterations>]\n");
	arm_puts("\n");
	arm_puts("hexdump     - Dump memory contents in hex format\n");
	arm_puts("              Usage: hexdump <addr> <count>\n");
	arm_puts("              <addr>  = memory address in hex\n");
	arm_puts("              <count> = byte count in hex\n");
	arm_puts("\n");
	arm_puts("copy        - Copy to target memory from source memory\n");
	arm_puts("              Usage: copy <dest> <src> <count>\n");
	arm_puts("              <dest>  = destination address in hex\n");
	arm_puts("              <src>   = source address in hex\n");
	arm_puts("              <count> = byte count in hex\n");
	arm_puts("\n");
	arm_puts("go          - Jump to a given address\n");
	arm_puts("              Usage: go <addr>\n");
	arm_puts("                <addr>  = jump address in hex\n");
	arm_puts("\n");
	arm_puts("reset       - Reset the system\n");
	arm_puts("\n");
}

void arm_cmd_hi(int argc, char **argv)
{
	arm_puts("hello\n");
}

void arm_cmd_hello(int argc, char **argv)
{
	arm_puts("hi\n");
}

void arm_cmd_mmu_setup(int argc, char **argv)
{
	arm_mmu_setup();
}

void arm_cmd_mmu_test(int argc, char **argv)
{
	arm_mmu_test();
}

void arm_cmd_mmu_cleanup(int argc, char **argv)
{
	arm_mmu_cleanup();
}

void arm_cmd_sysctl(int argc, char **argv)
{
	char str[32];
	u32 sys_100hz, sys_24mhz;
	sys_100hz = arm_readl((void *)(REALVIEW_SYS_BASE + 
					REALVIEW_SYS_100HZ_OFFSET));
	sys_24mhz = arm_readl((void *)(REALVIEW_SYS_BASE + 
					REALVIEW_SYS_24MHz_OFFSET));
	arm_puts("Sysctl Registers ...\n");
	arm_puts("  SYS_100Hz: 0x");
	arm_uint2hexstr(str, sys_100hz);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  SYS_24MHz: 0x");
	arm_uint2hexstr(str, sys_24mhz);
	arm_puts(str);
	arm_puts("\n");
}

void arm_cmd_timer(int argc, char **argv)
{
	char str[32];
	u64 irq_count, tstamp, ratio;
	irq_count = arm_timer_irqcount();
	tstamp = arm_timer_timestamp();
	ratio = tstamp / irq_count;
	arm_puts("Timer Information ...\n");
	arm_puts("  Timer IRQ:  0x");
	arm_ulonglong2hexstr(str, irq_count);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Time Stamp: 0x");
	arm_ulonglong2hexstr(str, tstamp);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Time Ratio: 0x");
	arm_ulonglong2hexstr(str, ratio);
	arm_puts(str);
	arm_puts("\n");
}

void arm_cmd_dhrystone(int argc, char **argv)
{
	char str[32];
	int iters = 1000000;
	if (argc > 1) {
		iters = arm_str2int(argv[1]);
	} else {
		arm_puts ("dhrystone: number of iterations not provided\n");
		arm_puts ("dhrystone: using default ");
		arm_int2str (str, iters);
		arm_puts (str);
		arm_puts (" iterations\n");
	}
	arm_timer_disable();
	dhry_main(iters);
	arm_timer_enable();
}

void arm_cmd_hexdump(int argc, char **argv)
{
	char str[32];
	u32 *addr;
	u32 i, count;
	if (argc < 3) {
		arm_puts ("hexdump: must provide <addr> and <count>\n");
		return;
	}
	addr = (u32 *)arm_hexstr2uint(argv[1]);
	count = arm_hexstr2uint(argv[2]);
	for (i = 0; i < (count / 4); i++) {
		if (i % 4 == 0) {
			arm_uint2hexstr(str, (u32)&addr[i]);
			arm_puts(str);
			arm_puts(": ");
		}
		arm_uint2hexstr(str, addr[i]);
		arm_puts(str);
		if (i % 4 == 3) {
			arm_puts("\n");
		} else {
			arm_puts(" ");
		}
	}
	arm_puts("\n");
}

void arm_cmd_copy(int argc, char **argv)
{
	u8 *dest, *src;
	u32 i, count;
	if (argc < 4) {
		arm_puts ("copy: must provide <dest>, <src>, and <count>\n");
		return;
	}
	dest = (u8 *)arm_hexstr2uint(argv[1]);
	src = (u8 *)arm_hexstr2uint(argv[2]);
	count = arm_hexstr2uint(argv[3]);
	for (i = 0; i < count; i++) {
		dest[i] = src[i];
	}
}

void arm_cmd_go(int argc, char **argv)
{
	char str[32];
	void (* jump)(void);
	if (argc < 2) {
		arm_puts ("go: must provide destination address\n");
		return;
	}
	arm_timer_disable();
	jump = (void (*)(void))arm_hexstr2uint(argv[1]);
	arm_uint2hexstr(str, (u32)jump);
	arm_puts("Jumping to location 0x");
	arm_puts(str);
	arm_puts(" ...\n");
	jump ();
	arm_timer_enable();
}

void arm_cmd_reset(int argc, char **argv)
{
	arm_puts("System reset ...\n\n");

	/* Unlock Lockable reigsters */
	arm_writel(REALVIEW_SYS_LOCKVAL, 
		   (void *)(REALVIEW_SYS_BASE + REALVIEW_SYS_LOCK_OFFSET));
#if 0
	arm_writel(REALVIEW_SYS_CTRL_RESET_CONFIGCLR, 
		   (void *)(pba8_csr_base + REALVIEW_SYS_RESETCTL_OFFSET));
#else
	arm_writel(0x100, 
		   (void *)(REALVIEW_SYS_BASE + REALVIEW_SYS_RESETCTL_OFFSET));
#endif

	while (1);
}

#define ARM_MAX_CMD_STR_SIZE	256
#define ARM_MAX_ARG_SIZE	32

/* Works in user mode */
void arm_main(void)
{
	int argc, pos, cnt;
	char line[ARM_MAX_CMD_STR_SIZE];
	char *argv[ARM_MAX_ARG_SIZE];

	arm_puts("ARM Realview PB-A8 Test Code\n\n");

	while(1) {
		arm_puts("arm-test# ");

		argc = 0;
		cnt = 0;
		pos = 0;
		arm_gets(line, ARM_MAX_CMD_STR_SIZE, '\n');
		while (line[pos] && (argc < ARM_MAX_ARG_SIZE)) {
			if ((line[pos] == '\r') ||
			    (line[pos] == '\n')) {
				line[pos] = '\0';
				break;
			}
			if (line[pos] == ' ') {
				if (cnt > 0) {
					line[pos] = '\0';
					cnt = 0;
				}
			} else {
				if (cnt == 0) {
					argv[argc] = &line[pos];
					argc++;
				}
				cnt++;
			}
			pos++;
		}
		if (!argc) {
			continue;
		}

		if (arm_strcmp(argv[0], "help") == 0) {
			arm_cmd_help(argc, argv);
		} else if (arm_strcmp(argv[0], "hi") == 0) {
			arm_cmd_hi(argc, argv);
		} else if (arm_strcmp(argv[0], "hello") == 0) {
			arm_cmd_hello(argc, argv);
		} else if (arm_strcmp(argv[0], "mmu_setup") == 0) {
			arm_cmd_mmu_setup(argc, argv);
		} else if (arm_strcmp(argv[0], "mmu_test") == 0) {
			arm_cmd_mmu_test(argc, argv);
		} else if (arm_strcmp(argv[0], "mmu_cleanup") == 0) {
			arm_cmd_mmu_cleanup(argc, argv);
		} else if (arm_strcmp(argv[0], "sysctl") == 0) {
			arm_cmd_sysctl(argc, argv);
		} else if (arm_strcmp(argv[0], "timer") == 0) {
			arm_cmd_timer(argc, argv);
		} else if (arm_strcmp(argv[0], "dhrystone") == 0) {
			arm_cmd_dhrystone(argc, argv);
		} else if (arm_strcmp(argv[0], "hexdump") == 0) {
			arm_cmd_hexdump(argc, argv);
		} else if (arm_strcmp(argv[0], "copy") == 0) {
			arm_cmd_copy(argc, argv);
		} else if (arm_strcmp(argv[0], "go") == 0) {
			arm_cmd_go(argc, argv);
		} else if (arm_strcmp(argv[0], "reset") == 0) {
			arm_cmd_reset(argc, argv);
		}
	}
}
