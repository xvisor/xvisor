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
.section .bootstrap.text, "ax"
.globl _start_64
.extern _bss_bootstrap_end

/*
 * I think this is really the best way to reclaim the
 * area of initial code. For all other non-recalled code
 * we will put them in a seperate .init section and then
 * reclaim it later.
 */
_start_64:
	leal	_bss_bootstrap_end, %esp	/* setup the stack (bss segment) */

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

        call bootstrap32
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

.type   early_print, @function
early_print:
        pushl   %ebp
        movl    %esp, %ebp
        subl    $16, %esp
        movl    $0xb8000, -8(%ebp)
        movl    $0, -4(%ebp)
        jmp     _ccd
_ctex:
        movl    8(%ebp), %eax
        movzbl  (%eax), %eax
        movl    %eax, %edx
        movl    -8(%ebp), %eax
        movb    %dl, (%eax)
        addl    $1, -8(%ebp)
        movl    -8(%ebp), %eax
        movb    $7, (%eax)
        addl    $1, -8(%ebp)
        addl    $1, 8(%ebp)
        addl    $1, -4(%ebp)
_ccd:
        cmpl    $24, -4(%ebp)
        jle     _ctex
        leave
        ret

.type	bootstrap32, @function
bootstrap32:
        pushl   %ebp
        movl    %esp, %ebp
        subl    $4, %esp
        movl    $_load_banner, (%esp)
        call    early_print
30:
	jmp	30b

.section .bootstrap_readonly,"aw",@progbits
.align 4
_load_banner:
	.string	"Bootstrapping X-Visor..."
