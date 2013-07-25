/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief basic firmware main file
 */

#include <arm_io.h>
#include <arm_heap.h>
#include <arm_mmu.h>
#include <arm_irq.h>
#include <arm_timer.h>
#include <arm_string.h>
#include <arm_stdio.h>
#include <arm_board.h>
#include <libfdt/libfdt.h>
#include <libfdt/fdt_support.h>
#include <dhry.h>

static int memory_size = 0x0;

/* Works in supervisor mode */
void arm_init(void)
{
	arm_heap_init();

	arm_irq_disable();

	arm_irq_setup();

	arm_stdio_init();

	arm_timer_init(10000);

	memory_size = arm_board_ram_size();

	arm_timer_enable();

	arm_irq_enable();
}

void arm_cmd_help(int argc, char **argv)
{
	arm_puts("help        - List commands and their usage\n");
	arm_puts("\n");
	arm_puts("hi          - Say hi to basic firmware\n");
	arm_puts("\n");
	arm_puts("hello       - Say hello to basic firmware\n");
	arm_puts("\n");
	arm_puts("wfi_test    - Run wait for irq instruction test for basic firmware\n");
	arm_puts("            Usage: wfi_test [<msecs>]\n");
	arm_puts("            <msecs>  = delay in milliseconds to wait for\n");
	arm_puts("\n");
	arm_puts("mmu_setup   - Setup MMU for basic firmware\n");
	arm_puts("\n");
	arm_puts("mmu_state   - MMU is enabled/disabled for basic firmware\n");
	arm_puts("\n");
	arm_puts("mmu_test    - Run MMU test suite for basic firmware\n");
	arm_puts("\n");
	arm_puts("mmu_cleanup - Cleanup MMU for basic firmware\n");
	arm_puts("\n");
	arm_puts("timer       - Display timer information\n");
	arm_puts("\n");
	arm_puts("dhrystone   - Dhrystone 2.1 benchmark\n");
	arm_puts("            Usage: dhrystone [<iterations>]\n");
	arm_puts("\n");
	arm_puts("hexdump     - Dump memory contents in hex format\n");
	arm_puts("            Usage: hexdump <addr> <count>\n");
	arm_puts("            <addr>  = memory address in hex\n");
	arm_puts("            <count> = byte count in hex\n");
	arm_puts("\n");
	arm_puts("copy        - Copy to target memory from source memory\n");
	arm_puts("            Usage: copy <dest> <src> <count>\n");
	arm_puts("            <dest>  = destination address in hex\n");
	arm_puts("            <src>   = source address in hex\n");
	arm_puts("            <count> = byte count in hex\n");
	arm_puts("\n");
	arm_puts("start_linux - Start linux kernel (atags mechanism)\n");
	arm_puts("            Usage: start_linux <kernel_addr> <initrd_addr> <initrd_size>\n");
	arm_puts("            <kernel_addr>  = kernel load address\n");
	arm_puts("            <initrd_addr>  = initrd load address\n");
	arm_puts("            <initrd_size>  = initrd size\n");
	arm_puts("\n");
#ifdef BOARD_FDT_SUPPORT
	arm_puts("start_linux_fdt - Start linux kernel (device-tree mechanism)\n");
	arm_puts("            Usage: start_linux_fdt <kernel_addr> <initrd_addr> <initrd_size> <fdt_addr>\n");
	arm_puts("            <kernel_addr>  = kernel load address\n");
	arm_puts("            <initrd_addr>  = initrd load address\n");
	arm_puts("            <initrd_size>  = initrd size\n");
	arm_puts("            <fdt_size>     = fdt blob size\n");
	arm_puts("\n");
#endif
	arm_puts("linux_cmdline - Show/Update linux command line\n");
	arm_puts("            Usage: linux_cmdline <new_linux_cmdline> \n");
	arm_puts("            <new_linux_cmdline>  = linux command line\n");
	arm_puts("\n");
	arm_puts("linux_memory_size - Show/Update linux memory size\n");
	arm_puts("            Usage: linux_memory_size <memory_size> \n");
	arm_puts("            <memory_size>  = memory size in hex\n");
	arm_puts("\n");
	arm_puts("autoexec    - autoexec command list from flash\n");
	arm_puts("            Usage: autoexec\n");
	arm_puts("\n");
	arm_puts("go          - Jump to a given address\n");
	arm_puts("            Usage: go <addr>\n");
	arm_puts("            <addr>  = jump address in hex\n");
	arm_puts("\n");
	arm_puts("reset       - Reset the system\n");
	arm_puts("\n");
}

void arm_cmd_hi(int argc, char **argv)
{
	if (argc != 1) {
		arm_puts ("hi: no parameters required\n");
		return;
	}

	arm_puts("hello\n");
}

void arm_cmd_hello(int argc, char **argv)
{
	if (argc != 1) {
		arm_puts ("hello: no parameters required\n");
		return;
	}

	arm_puts("hi\n");
}

void arm_cmd_wfi_test(int argc, char **argv)
{
	u64 tstamp;
	char time[256];
	int delay = 1000;

	if (argc > 2) {
		arm_puts ("wfi_test: could provide only <delay>\n");
		return;
	} else if (argc == 2) {
		delay = arm_str2int(argv[1]);
	}

	arm_puts("Executing WFI instruction\n");
	arm_timer_disable();
	arm_timer_change_period(delay*1000);
	arm_timer_enable();
	tstamp = arm_timer_timestamp();
	arm_irq_wfi();
	tstamp = arm_timer_timestamp() - tstamp;
	arm_timer_disable();
	arm_timer_change_period(10000);
	arm_timer_enable();
	arm_puts("Resumed from WFI instruction\n");
	arm_puts("Time spent in WFI: ");
	arm_ulonglong2str(time, tstamp);
	arm_puts(time);
	arm_puts(" nsecs\n");
}

void arm_cmd_mmu_setup(int argc, char **argv)
{
	if (argc != 1) {
		arm_puts ("mmu_setup: no parameters required\n");
		return;
	}

	arm_mmu_setup();
}

void arm_cmd_mmu_state(int argc, char **argv)
{
	if (argc != 1) {
		arm_puts ("mmu_state: no parameters required\n");
		return;
	}

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

	if (argc != 1) {
		arm_puts ("mmu_test: no parameters required\n");
		return;
	}

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
	if (argc != 1) {
		arm_puts ("mmu_cleanup: no parameters required\n");
		return;
	}

	arm_mmu_cleanup();
}

void arm_cmd_timer(int argc, char **argv)
{
	char str[32];
	u64 irq_count, irq_delay, tstamp;

	if (argc != 1) {
		arm_puts ("timer: no parameters required\n");
		return;
	}

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
	if (argc > 2) {
		arm_puts ("dhrystone: could provide only <iter_number>\n");
		return;
	} else if (argc == 2) {
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
	u32 i, count, len;
	if (argc != 3) {
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
	u64 tstamp;
	char time[256];
	u8 *dest, *src;
	u32 i, count;
	if (argc != 4) {
		arm_puts ("copy: must provide <dest>, <src>, and <count>\n");
		return;
	}
	dest = (u8 *)arm_hexstr2uint(argv[1]);
	src = (u8 *)arm_hexstr2uint(argv[2]);
	count = arm_hexstr2uint(argv[3]);
	arm_timer_disable();
	tstamp = arm_timer_timestamp();
	for (i = 0; i < count; i++) {
		dest[i] = src[i];
	}
	tstamp = arm_timer_timestamp() - tstamp;
	arm_timer_enable();
	arm_ulonglong2str(time, tstamp);
	arm_puts("copy took ");
	arm_puts(time);
	arm_puts(" ns for ");
	arm_puts(argv[3]);
	arm_puts(" bytes\n");
}

static char  cmdline[1024];

typedef void (* linux_entry_t) (u32 zero, u32 machine_type, u32 kernel_args, u32 zero2);

void arm_cmd_start_linux(int argc, char **argv)
{
	char cfg_str[32];
	u32 * kernel_args = (u32 *)(arm_board_ram_start() + 0x1000);
	u32 cmdline_size, p;
	u32 kernel_addr, initrd_addr, initrd_size;

	if (argc != 4) {
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
	kernel_args[p++] = memory_size;
	kernel_args[p++] = arm_board_ram_start();
	/* ATAG_INITRD2 */
	kernel_args[p++] = 4;
	kernel_args[p++] = 0x54420005;
	kernel_args[p++] = initrd_addr;
	kernel_args[p++] = initrd_size;

	/* Pass memory size via kernel command line */
	arm_sprintf(cfg_str, " mem=%dM", (memory_size >> 20));
	arm_strcat(cmdline, cfg_str);

	cmdline_size = arm_strlen(cmdline);
	if (cmdline_size) {
		/* ATAG_CMDLINE */
		arm_strcpy((char *)&kernel_args[p + 2], cmdline);
		cmdline_size = (cmdline_size >> 2) + 1;
		kernel_args[p++] = cmdline_size + 2;
		kernel_args[p++] = 0x54410009;
		p += cmdline_size;
	}

	/* ATAG_END */
	kernel_args[p++] = 0;
	kernel_args[p++] = 0;

	/* Disable interrupts and timer */
	arm_timer_disable();
	arm_irq_disable();

	/* Jump to Linux Kernel
	 * r0 -> zero
	 * r1 -> board machine type
	 * r2 -> kernel args address 
	 */
	((linux_entry_t)kernel_addr)(0x0, arm_board_linux_machine_type(), (u32)kernel_args, 0);

	/* We should never reach here */
	while (1);

	return;
}

#ifdef BOARD_FDT_SUPPORT
void arm_cmd_start_linux_fdt(int argc, char **argv)
{
	u32 kernel_addr, fdt_addr;
	u32 initrd_addr, initrd_size;
	char cfg_str[32];

	if (argc != 5) {
		arm_puts ("start_linux: must provide <kernel_addr>, <initrd_addr>, <initrd_size> and <fdt_addr>\n");
		return;
	}

	/* Parse the arguments from command line */
	kernel_addr = arm_hexstr2uint(argv[1]);
	fdt_addr = arm_hexstr2uint(argv[4]);
	initrd_addr = arm_hexstr2uint(argv[2]);
	initrd_size = arm_hexstr2uint(argv[3]);

	/* Pass memory size via kernel command line */
	arm_sprintf(cfg_str, " mem=%dM", (memory_size >> 20));
	arm_strcat(cmdline, cfg_str);

	/* Fillup/fixup the fdt blob with following:
	 * 		- initrd start, end
	 * 		- kernel cmd line
	 * 		- number of cpus   */
	fdt_chosen((void *)fdt_addr, 1, cmdline);
	fdt_initrd((void *)fdt_addr, initrd_addr, initrd_addr + initrd_size, 1);

	/* Disable interrupts and timer */
	arm_timer_disable();
	arm_irq_disable();
	arm_mmu_cleanup();

	/* Jump to Linux Kernel
	 * r0 -> dtb address
	 */
	arm_puts("Jumping into linux ...\n");
	((linux_entry_t)kernel_addr)(0, -1, fdt_addr, 0);

	/* We should never reach here */
	while (1);

	return;
}
#endif

void arm_cmd_linux_cmdline(int argc, char **argv)
{
	if (argc >= 2) {
		int cnt = 1;
		cmdline[0] = 0;

		while (cnt < argc) {
			arm_strcat(cmdline, argv[cnt]);
			arm_strcat(cmdline, " ");
			cnt++;
		}
	}

	arm_puts ("linux_cmdline = \"");
	arm_puts(cmdline);
	arm_puts ("\"\n");

	return;
}

void arm_cmd_linux_memory_size(int argc, char **argv)
{
	char str[32];

	if (argc == 2) {
		memory_size = (u32)arm_hexstr2uint(argv[1]);
	}

	arm_puts ("linux_memory_size = 0x");
	arm_uint2hexstr(str, memory_size);
	arm_puts(str);
	arm_puts (" Bytes\n");

	return;
}

void arm_exec(char *line);

void arm_cmd_autoexec(int argc, char **argv)
{
	static int lock = 0;
	int len;
	/* commands to execute are stored in NOR flash */
	char *ptr = (char *)(arm_board_flash_addr() + 0xFF000);
	char buffer[4096];

	if (argc != 1) {
		arm_puts ("autoexec: no parameters required\n");
		return;
	}

	/* autoexec is not recursive */
	if (lock) {
		arm_puts("ignoring autoexec calling autoexec\n");
		return;
	}

	lock = 1;

	if ((len = arm_strlen(ptr))) {
		int pos = 0;

		/* copy commands from NOR flash */
		arm_strcpy(buffer, ptr);

		/* now we process them */
		while (pos < len) {
			ptr = &buffer[pos];

			/* We need to separate the commands */
			while ((buffer[pos] != '\r') &&
				(buffer[pos] != '\n') &&
				(buffer[pos] != 0)) {
				pos++;
			}
			buffer[pos] = '\0';
			pos++;

			/* print the command */
			arm_puts("autoexec(");
			arm_puts(ptr);
			arm_puts(")\n");
			/* execute it */
			arm_exec(ptr);
		}

	}

	lock = 0;

	return;
}

void arm_cmd_go(int argc, char **argv)
{
	char str[32];
	void (* jump)(void);

	if (argc != 2) {
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
	if (argc != 1) {
		arm_puts ("reset: no parameters required\n");
		return;
	}

	arm_puts("System reset ...\n\n");

	arm_board_reset();

	while (1);
}

#define ARM_MAX_ARG_SIZE	32

void arm_exec(char *line)
{
	int argc = 0, pos = 0, cnt = 0;
	char *argv[ARM_MAX_ARG_SIZE];

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

	if (argc) {
		if (arm_strcmp(argv[0], "help") == 0) {
			arm_cmd_help(argc, argv);
		} else if (arm_strcmp(argv[0], "hi") == 0) {
			arm_cmd_hi(argc, argv);
		} else if (arm_strcmp(argv[0], "hello") == 0) {
			arm_cmd_hello(argc, argv);
		} else if (arm_strcmp(argv[0], "wfi_test") == 0) {
			arm_cmd_wfi_test(argc, argv);
		} else if (arm_strcmp(argv[0], "mmu_setup") == 0) {
			arm_cmd_mmu_setup(argc, argv);
		} else if (arm_strcmp(argv[0], "mmu_state") == 0) {
			arm_cmd_mmu_state(argc, argv);
		} else if (arm_strcmp(argv[0], "mmu_test") == 0) {
			arm_cmd_mmu_test(argc, argv);
		} else if (arm_strcmp(argv[0], "mmu_cleanup") == 0) {
			arm_cmd_mmu_cleanup(argc, argv);
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
#ifdef BOARD_FDT_SUPPORT
		} else if (arm_strcmp(argv[0], "start_linux_fdt") == 0) {
			arm_cmd_start_linux_fdt(argc, argv);
#endif
		} else if (arm_strcmp(argv[0], "linux_cmdline") == 0) {
			arm_cmd_linux_cmdline(argc, argv);
		} else if (arm_strcmp(argv[0], "linux_memory_size") == 0) {
			arm_cmd_linux_memory_size(argc, argv);
		} else if (arm_strcmp(argv[0], "autoexec") == 0) {
			arm_cmd_autoexec(argc, argv);
		} else if (arm_strcmp(argv[0], "go") == 0) {
			arm_cmd_go(argc, argv);
		} else if (arm_strcmp(argv[0], "reset") == 0) {
			arm_cmd_reset(argc, argv);
		} else {
			arm_puts("Unknown command\n");
		}
	}
}

#define ARM_MAX_CMD_STR_SIZE	256

/* Works in user mode */
void arm_main(void)
{
	char line[ARM_MAX_CMD_STR_SIZE];

	/* Setup board specific linux default cmdline */
	arm_board_linux_default_cmdline(cmdline, sizeof(cmdline));

	arm_puts(arm_board_name());
	arm_puts(" Basic Firmware\n\n");

	arm_board_init();

	while(1) {
		arm_puts("basic# ");

		arm_gets(line, ARM_MAX_CMD_STR_SIZE, '\n');

		arm_exec(line);
	}
}
