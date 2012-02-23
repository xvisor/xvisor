/*
 * File: start.S
 * E-mail: hschauhan@nulltrace.org
 *
 * Copyright (c) Himanshu Chauhan 2012-13.
 * All rights reserved.
 *
 * This is released under GNU GPLv2.
 *
 */
.code32 /* 32-bit code */
.section .text.bootstrap
.globl _start_64, _pg_dir

/*
 * I think this is really the best way to reclaim the
 * area of initial code. For all other non-recalled code
 * we will put them in a seperate .init section and then
 * reclaim it later.
 */
_pg_dir:
_start_64:
	leal	__xvisor_stack + 8192, %esp	/* setup the stack (bss segment) */

        /* O.K. We have CPUID. Check if we can enter
         * long mode. */
        movl $0x80000000, %eax
        cpuid
        cmp  $0x80000001, %eax
        jb _no_longmode

        movl $0x80000001, %eax
        cpuid
        movl $(0x20000000), %ecx
        test %ecx, %edx
        jz _no_longmode

        /*
         * O.K. CPU Supports Long mode. Need to set up
         * Long mode page tables and move to long mode.
         */
        movl  %cr0, %eax
        movl $0x1, %ecx
        test %ecx, %eax
        jz   _pe_not_enabled

        /*
         * Need to do a far jump to long code, before that
         * Setup!!!
         */
1:      jmp	1b /* we shouldn't work here */

/*
 * Don't worry about this. This is a multi-boot signature for grub
 * to let it know that this binary is actually a kernel.
 */
.align 4
mboot:
	.equ MULTIBOOT_PAGE_ALIGN, 1<< 0
	.equ MULTIBOOT_MEMORY_INFO, 1 << 1
	.equ MULTIBOOT_AOUT_KLUDGE, 1 << 16
	.equ MULTIBOOT_HEADER_MAGIC, 0x1BADB002
	.equ MULTIBOOT_HEADER_FLAGS, MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_AOUT_KLUDGE
	.equ MULTIBOOT_CHECKSUM, -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
	.extern _text_bootstrap_start, _bss_bootstrap_start, _code_bootstrap_end    /* These externs are coming from the linker script */

	.int MULTIBOOT_HEADER_MAGIC
	.int MULTIBOOT_HEADER_FLAGS
	.int MULTIBOOT_CHECKSUM

	.int mboot
	.int _text_bootstrap_start
	.int _bss_bootstrap_start
	.int _code_bootstrap_end
	.int _start_64

_no_longmode:
        jmp _no_longmode

/*
 * If not loaded by multiboot compliant
 * bootloader.
 */
_pe_not_enabled:
        jmp _pe_not_enabled

.section .bss.bootstrap
.globl __xvisor_stack
.lcomm __xvisor_stack, 8192 /* 8K of stack */
