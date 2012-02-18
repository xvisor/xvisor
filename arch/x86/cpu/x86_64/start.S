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

.section .text
.globl _start_64, _pg_dir

/*
 * I think this is really the best way to reclaim the
 * area of initial code. For all other non-recalled code
 * we will put them in a seperate .init section and then
 * reclaim it later.
 */
_pg_dir:
_start_64:
	movl	%esp, __xvisor_stack + 8192	/* setup the stack (bss segment) */
	jmp	1f

1:
	.extern cpu_init
	call cpu_init		/* Call the C- code now */
	jmp	1b /* we shouldn't work here */

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
	.extern _text_start, _bss_start, _code_end    /* These externs are coming from the linker script */

	.int MULTIBOOT_HEADER_MAGIC
	.int MULTIBOOT_HEADER_FLAGS
	.int MULTIBOOT_CHECKSUM

	.int mboot
	.int _text_start
	.int _bss_start
	.int _code_end
	.int _start_64

.section .bss
.globl __xvisor_stack
.lcomm __xvisor_stack, 8192 /* 8K of stack */
