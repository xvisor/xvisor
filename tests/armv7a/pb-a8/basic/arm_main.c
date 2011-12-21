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
	u32 sys_100hz;

	arm_heap_init();

	arm_irq_setup();

	arm_irq_enable();

	arm_stdio_init();

	sys_100hz = arm_readl((void *)(REALVIEW_SYS_BASE + 
					REALVIEW_SYS_100HZ_OFFSET));
	arm_timer_init(10000, sys_100hz, 1);

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
	arm_puts("mmu_state   - MMU is enabled/disabled for ARM test code\n");
	arm_puts("\n");
	arm_puts("mmu_test    - Run MMU test suite for ARM test code\n");
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
	arm_puts("start_linux - Start linux kernel\n");
	arm_puts("              Usage: start_linux <kernel_addr> <initrd_addr> <initrd_size>\n");
	arm_puts("                <kernel_addr>  = kernel load address\n");
	arm_puts("                <initrd_addr>  = initrd load address\n");
	arm_puts("                <initrd_size>  = initrd size\n");
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

void arm_cmd_mmu_state(int argc, char **argv)
{
	if (arm_mmu_is_enabled()) {
		arm_puts("MMU Enabled\n");
	} else {
		arm_puts("MMU Disabled\n");
	}
}

void arm_cmd_mmu_test(int argc, char **argv)
{
	char str[32];
	u32 total = 0x0, pass = 0x0, fail = 0x0;
	arm_puts("MMU Section Test Suite ...\n");
	total = 0x0;
	pass = 0x0;
	fail = 0x0;
	arm_mmu_section_test(&total, &pass, &fail);
	arm_puts("  Total: ");
	arm_int2str(str, total);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Pass : ");
	arm_int2str(str, pass);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Fail : ");
	arm_int2str(str, fail);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("MMU Page Test Suite ...\n");
	total = 0x0;
	pass = 0x0;
	fail = 0x0;
	arm_mmu_page_test(&total, &pass, &fail);
	arm_puts("  Total: ");
	arm_int2str(str, total);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Pass : ");
	arm_int2str(str, pass);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Fail : ");
	arm_int2str(str, fail);
	arm_puts(str);
	arm_puts("\n");
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
	u64 irq_count, irq_delay, tstamp;
	irq_count = arm_timer_irqcount();
	irq_delay = arm_timer_irqdelay();
	tstamp = arm_timer_timestamp();
	arm_puts("Timer Information ...\n");
	arm_puts("  IRQ Count:  0x");
	arm_ulonglong2hexstr(str, irq_count);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  IRQ Delay:  0x");
	arm_ulonglong2hexstr(str, irq_delay);
	arm_puts(str);
	arm_puts("\n");
	arm_puts("  Time Stamp: 0x");
	arm_ulonglong2hexstr(str, tstamp);
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
	dhry_main(iters);
}

void arm_cmd_hexdump(int argc, char **argv)
{
	char str[32];
	u32 *addr;
	u32 i, count, len;
	if (argc < 3) {
		arm_puts ("hexdump: must provide <addr> and <count>\n");
		return;
	}
	addr = (u32 *)arm_hexstr2uint(argv[1]);
	count = arm_hexstr2uint(argv[2]);
	for (i = 0; i < (count / 4); i++) {
		if (i % 4 == 0) {
			arm_uint2hexstr(str, (u32)&addr[i]);
			len = arm_strlen(str);
			while (len < 8) {
				arm_puts("0");
				len++;
			}
			arm_puts(str);
			arm_puts(": ");
		}
		arm_uint2hexstr(str, addr[i]);
		len = arm_strlen(str);
		while (len < 8) {
			arm_puts("0");
			len++;
		}
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

#define RAM_START	0x70000000
#define RAM_SIZE	0x06000000

typedef void (* linux_entry_t) (u32 zero, u32 machine_type, u32 kernel_args);

void arm_cmd_start_linux(int argc, char **argv)
{
	char  * cmdline = "root=/dev/ram rw ramdisk_size=0x1000000 earlyprintk console=ttyAMA0 mem=96M" ;
	u32 * kernel_args = (u32 *)(RAM_START + 0x100);
	u32 cmdline_size, p;
	u32 kernel_addr, initrd_addr, initrd_size;

	if (argc < 4) {
		arm_puts ("start_linux: must provide <kernel_addr>, <initrd_addr>, and <initrd_size>\n");
		return;
	}

	/* Parse the arguments from command line */
	kernel_addr = arm_hexstr2uint(argv[1]);
	initrd_addr = arm_hexstr2uint(argv[2]);
	initrd_size = arm_hexstr2uint(argv[3]);

	/* Setup kernel args */
	for (p = 0; p < 128; p++) {
		kernel_args[p] = 0x0;
	}
	p = 0;
	/* ATAG_CORE */
	kernel_args[p++] = 5;
	kernel_args[p++] = 0x54410001;
	kernel_args[p++] = 1;
	kernel_args[p++] = 0x1000;
	kernel_args[p++] = 0;
	/* ATAG_MEM */
	kernel_args[p++] = 4;
	kernel_args[p++] = 0x54410002;
	kernel_args[p++] = RAM_SIZE;
	kernel_args[p++] = RAM_START;
	/* ATAG_INITRD2 */
	kernel_args[p++] = 4;
        kernel_args[p++] = 0x54420005;
        kernel_args[p++] = initrd_addr;
        kernel_args[p++] = initrd_size;
	/* ATAG_CMDLINE */
	cmdline_size = arm_strlen(cmdline);
	arm_strcpy((char *)&kernel_args[p + 2], cmdline);
	cmdline_size = (cmdline_size >> 2) + 1;
	kernel_args[p++] = cmdline_size + 2;
	kernel_args[p++] = 0x54410009;
	p += cmdline_size;
	/* ATAG_END */
	kernel_args[p++] = 0;
	kernel_args[p++] = 0;

	/* Disable interrupts and timer */
	arm_timer_disable();
	arm_irq_disable();

	/* Jump to Linux Kernel
	 * r0 -> zero
	 * r1 -> machine type (0x769)
	 * r2 -> kernel args address 
	 */
	((linux_entry_t)kernel_addr)(0x0, 0x769, (u32)kernel_args);

	/* We should never reach here */
	while (1);

	return;
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

#if 0 /* QEMU checks bit 8 which is wrong */
	arm_writel(0x100, 
		   (void *)(REALVIEW_SYS_BASE + REALVIEW_SYS_RESETCTL_OFFSET));
#else
	arm_writel(0x0, 
		   (void *)(REALVIEW_SYS_BASE+ REALVIEW_SYS_RESETCTL_OFFSET));
	arm_writel(REALVIEW_SYS_CTRL_RESET_PLLRESET, 
		   (void *)(REALVIEW_SYS_BASE+ REALVIEW_SYS_RESETCTL_OFFSET));
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

	arm_puts("ARM Realview PB-A8 Basic Test\n\n");

	/* Unlock Lockable reigsters */
	arm_writel(REALVIEW_SYS_LOCKVAL, 
		   (void *)(REALVIEW_SYS_BASE + REALVIEW_SYS_LOCK_OFFSET));

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
		} else if (arm_strcmp(argv[0], "mmu_state") == 0) {
			arm_cmd_mmu_state(argc, argv);
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
		} else if (arm_strcmp(argv[0], "start_linux") == 0) {
			arm_cmd_start_linux(argc, argv);
		} else if (arm_strcmp(argv[0], "go") == 0) {
			arm_cmd_go(argc, argv);
		} else if (arm_strcmp(argv[0], "reset") == 0) {
			arm_cmd_reset(argc, argv);
		}
	}
}
