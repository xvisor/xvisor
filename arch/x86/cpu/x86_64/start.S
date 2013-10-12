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
#include <cpu_mmu.h>
#include <multiboot.h>
#include <arch_config.h>

.section .mboot,"aw",@progbits
/*
 * Don't worry about this. This is a multi-boot signature for grub
 * to let it know that this binary is actually a kernel.
 */
.align 4
mboot:
	.extern _code_start, _code_end    /* These externs are coming from the linker script */

	.int MULTIBOOT_HEADER_MAGIC
	.int MULTIBOOT_HEADER_FLAGS
	.int MULTIBOOT_CHECKSUM

	.int mboot
	.int _code_start
        .int _code_end
        .int 0
	.int _start_32

.code32 /* 32-bit code */
.section .bootstrap.text, "ax"
.globl _start_32
.extern _bss_bootstrap_end

_start_32:
        cli /* no interrupts please. */
	leal	_bootstrap_stack_start, %esp	/* setup the stack (bss segment) */

	movl    %ebx, %esi
	leal    _mboot_info, %edi
	movl    $MAX_BOOTINFO_WORDS, %ecx /* 4k worth data */
_mboot_copy_loop:
	movl    (%esi), %eax
	movl    %eax, (%edi)
	addl    $4, %esi
	addl    $4, %edi
	subl    $1, %ecx
	cmp     $0, %ecx
	jne     _mboot_copy_loop

	movl    16(%ebx), %esi /* command line */
	leal    _cmd_line, %edi
	movl    $MAX_CMD_LINE_WORDS, %ecx
_cmdline_copy_loop:
	movl    (%esi), %eax
	movl    %eax, (%edi)
	addl    $4, %esi
	addl    $4, %edi
	subl    $1, %ecx
	cmp     $0, %ecx
	jne     _cmdline_copy_loop

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

_no_longmode:
        jmp _no_longmode

/*
 * If not loaded by multiboot compliant
 * bootloader.
 */
_pe_not_enabled:
        jmp _pe_not_enabled

        .type create_page_table_entry, @function
create_page_table_entry:
        pushl   %ebp
        movl    %esp, %ebp
        subl    $20, %esp
        movl    8(%ebp), %eax
        movl    %eax, -4(%ebp)  /* VA Start */
        movl    12(%ebp), %eax
        movl    %eax, -8(%ebp)  /* PA Start */
        movl    16(%ebp), %eax
        movl    %eax, -12(%ebp) /* NR_PAGES to MAP */

__fill_IMP_table:
        /* Populate pgti */
        movl    -4(%ebp), %ebx
        shrl    $PGTI_SHIFT, %ebx
        andl    $(~PGTREE_MASK), %ebx /* ebx now has pgti index. */
	movl	%ebx, %eax
        movl    -8(%ebp), %ecx
        andl    $PAGE_MASK, %ecx
        or      $0x3, %ecx /* page table entry present and its r/w */
        movl    %ecx, __pgti(,%eax,8)
        xor     %ecx, %ecx
        movl    %ecx, __pgti+4(,%eax,8)

        /* Populate pgdi */
        movl    -4(%ebp), %ebx
        shrl    $PGDI_SHIFT, %ebx
        andl    $(~PGTREE_MASK), %ebx /* ebx now has pgdi index. */
        movl    $__pgti, %ecx
        movl    $8, %edx
        mull    %edx
        addl    %ecx, %eax
        andl    $PAGE_MASK, %eax
        or      $0x3, %eax
        movl    %eax, __pgdi(,%ebx,8)
        xorl    %eax, %eax

        /* populate pgdp */
        movl    -4(%ebp), %eax
        shrl    $PGDP_SHIFT, %eax
        andl    $(~PGTREE_MASK), %eax /* eax now has pgdp index. */
        movl    $__pgdi, %ecx
        andl    $PAGE_MASK, %ecx
        or      $0x3, %ecx
        movl    %ecx, __pgdp(,%eax,8)
        xorl    %ecx, %ecx

        /* populate PML4 */
        andl    $0, %eax
        movl    $__pgdp, %ecx
        andl    $PAGE_MASK, %ecx
        or      $0x3, %ecx
        movl    %ecx, __pml4(,%eax,8)
        xorl    %ecx, %ecx

        addl     $4096, -4(%ebp)
        addl     $4096, -8(%ebp)
        subl     $1, -12(%ebp)
        cmp      $0, -12(%ebp)
        ja      __fill_IMP_table
        leave
        ret

.extern _code_end
        .type __do_IMP, @function
__do_IMP:
        pushl   %ebp
        movl    %esp, %ebp
        movl    $_code_end, %eax
        subl    $_code_start, %eax
        xor     %edx, %edx
        movl    $PAGE_SIZE, %ecx
        divl    %ecx
        pushl   %eax
        pushl   $CPU_TEXT_LMA
        pushl   $CPU_TEXT_LMA /* identical map */
        call    create_page_table_entry
        leave
        ret

	.type __map_vga_base, @function
__map_vga_base:
	pushl   %ebp
	movl    %esp, %ebp
	pushl   $4 /* vga buffer is 16K */
	pushl   $0xb8000 /* vga base */
	pushl   $0xb8000 /* identical map */
	call    create_page_table_entry
	leave
	ret

#if 0
        .type __test_pagetables, @function
__test_pagetables:
        pushl   %ebp
        movl    %esp, %ebp
        pushl   $2 /* vga buffer is 16K */
        pushl   $0x300000 /* vga base */
        pushl   $0x300000 /* identical map */
        call    create_page_table_entry
        leave
        ret
#endif

        .type	bootstrap32, @function
bootstrap32:
        pushl   %ebp
        movl    %esp, %ebp

#if 0
        call    __test_pagetables
#endif
        /*
         * Create identity map page table entries
         * because we will execute a few
         * more 32-bit bootstrap instructions
         * before jumping to 64-code.
         */
        call    __do_IMP

	call    __map_vga_base

        /* Enable PAE */
        movl    %cr4, %eax
        or      $(1 << 5), %eax
        movl    %eax, %cr4

        /* Load the page table in cr3 */
        movl    $__pml4, %eax
        movl    %eax, %cr3

        /* Set the IA32-e mode */
        movl    $0xc0000080, %ecx
        rdmsr
        or      $0x100, %eax
        wrmsr

        /* Enable Paging */
        movl    %cr0, %eax
        orl    $(1 << 31 | 1 << 0), %eax
        movl    %eax, %cr0

        /* Load the new 64-bit GDT */
        movl   $.GDT_POINTER, %eax
        lgdt   (%eax)
        movl   $0x10, %eax
        movl   %eax, %ds
        movl   %eax, %es
        movl   %eax, %fs
        movl   %eax, %ss
        movl   %eax, %gs

        /*
         * Do a far call to 64-bit trampoline
         * Bootstrapping is almost done!
         */
        ljmp   $0x8, $(__64_bit_entry_trampoline)

/*
 * 1 page for PML4, 1 page for PGPT, 1 page
 * for PGTI, 4 pages for Page tables.
 */
/*
 * !!!!! ATTENTION !!!!!
 *
 * Contrary to the original design, Xvisor uses these
 * same set of page tables rather than starting afresh.
 * So essentially when ever the VAPOOL changes, this
 * has to be changed accordingly. - Himanshu 13/10/2013
 */
.section .bootstrap.bss,"aw",@progbits
        .globl __pml4
        .align 32
	.type	__pml4, @object
	.size	__pml4, 4096
__pml4:
	.long	0
	.long	0
	.zero	4088

	.align 32
        .globl __pgdp
	.type	__pgdp, @object
	.size	__pgdp, 8192 /* two pages */
__pgdp:
	.long	0
	.long	0
	.zero	8184

        .globl __pgdi
	.align 32
	.type	__pgdi, @object
	.size	__pgdi, 8192 /* two pages */
__pgdi:
	.long	0
	.long	0
	.zero	8184

        .globl __pgti
	.align 32
	.type	__pgti, @object
	.size	__pgti, 65536 /* sixteen pages */
__pgti:
	.zero	65536 /* sixteen pages */

.align 4
	.type   _mboot_info, @object
        .size   _mboot_info, MAX_BOOTINFO_SIZE
_mboot_info:
	.zero MAX_BOOTINFO_SIZE

.align 4
	.type   _cmd_line, @object
	.size   _cmd_line, MAX_CMD_LINE
_cmd_line:
	.zero   MAX_CMD_LINE

.align 4
_gdt64:
        .NULL:
        .quad 0 /* Null segment */

        .xvisor_code_64:
        /* Segment limit: Shouldn't matter in long mode but still.. */
        .word 0xFFFF
        /* Segment base: Again shouln't matter */
        .word 0
        .long 0xBF9A00 /* AVL:L:Execute/Access */

        .xvisor_data_64:
        .word 0xFFFF
        .word 0
        .long 0x9F9200

	.type __xvisor_tss_64_desc, @object
	.globl __xvisor_tss_64_desc
	.size __xvisor_tss_64_desc, 16 /* 16 bytes */
	__xvisor_tss_64_desc: /* TSS descriptor */
	.zero 16 /* To be filled later */

        .GDT_POINTER:
        .word .-_gdt64 -1
        .long _gdt64

	/*
	 * The calling convention of the System V AMD64 ABI
	 * is followed on Solaris, GNU/Linux, FreeBSD, and
	 * other non-Microsoft operating systems. The first
	 * six integer or pointer arguments are passed in
	 * registers RDI, RSI, RDX, RCX, R8, and R9.
	 */
.code64
.section .bootstrap.text
__64_bit_entry_trampoline:
        movabsq $_stack_start, %rsp
        movq    $_mboot_info, %rdi
        movq    $_cmd_line, %rsi

        /*
         * Hell yes. We are done bootstrapping!
         * Time for hypervisor initialization now.
         *
         * This was all *dirty* and I hope I got it
         * right. Well atleast I tried my best! :)
         */
        call cpu_init

        /*
         * Any case: Shouldn't be here. Kill yourself!!
         */
        hlt
