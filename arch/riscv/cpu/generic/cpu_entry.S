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
 * @file cpu_entry.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief entry points (booting, reset, exceptions) for RISC-V
 */

#include <riscv_asm.h>
#include <riscv_csr.h>

	/* 
	 * _start: Primary CPU startup code
	 * _start_secondary: Secondary CPU startup code
	 * _start_secondary_nopen: Secondary CPU startup code without holding pen
	 *
	 * Note: Xvisor could be loaded any where in memory by boot loaders.
	 * The _start ensures that Xvisor exectues from intended
	 * base address provided at compile time.
	 */
	.section .entry, "ax", %progbits
	.globl _start
	.globl _start_secondary
	.globl _start_secondary_nopen
_start:
	/* Mask all interrupts */
	csrw sie, zero

	/*
	 * Disable FPU to detect illegal usage of
	 * floating point in kernel space
	 */
	li t0, SR_FS
	csrc sstatus, t0

#ifdef CONFIG_SMP
	/* Pick one hart to run the main boot sequence */
	la a3, _start_lottery
	li a2, 1
	amoadd.w a3, a2, (a3)
	bnez a3, _start_secondary
#endif

	/* Save hart ID and DTB base */
	la	a6, _boot_reg0
	REG_S	a0, (a6)
	la	a6, _boot_reg1
	REG_S	a1, (a6)

	/* Save load addresses
	 * a2 -> load start
	 * a3 -> load end
	 * a4 -> execution start
	 * a5 -> execution end
	 */
	la	a2, _start
	la	a6, __exec_start
	REG_L	a4, (a6)
	la	a6, __exec_end
	REG_L	a5, (a6)
	sub	a6, a5, a4
	add	a3, a2, a6
	la	a6, _load_start
	REG_S	a2, (a6)
	la	a6, _load_end
	REG_S	a3, (a6)

	/* Zero-out bss section */
	la	a6, __bss_start
	REG_L	a0, (a6)
	sub	a0, a0, a4
	add	a0, a0, a2
	la	a6, __bss_end
	REG_L	a1, (a6)
	sub	a1, a1, a4
	add	a1, a1, a2
_bss_zero:
	REG_S	zero, (a0)
	add	a0, a0, __SIZEOF_POINTER__
	blt	a0, a1, _bss_zero

	/* Setup temporary stack */
	la	a6, __hvc_stack_end
	REG_L	a0, (a6)
	sub	a0, a0, a4
	add	sp, a0, a2

	/* Check and copy FDT */
	la	a6, _boot_reg1
	REG_L	a0, (a6)
	la	a6, __builtin_dtb
	REG_L	a1, (a6)
	sub	a1, a1, a4
	add	a1, a1, a2
	call	_copy_fdt

	/* Setup initial page table */
	la	a6, _load_start
	REG_L	a0, (a6)
	la	a6, _load_end
	REG_L	a1, (a6)
	la	a6, __exec_start
	REG_L	a2, (a6)
	la	a6, __exec_end
	REG_L	a3, (a6)
	call	_setup_initial_pgtbl

	j	_start_secondary_nopen

#ifdef CONFIG_SMP
	.align	3
_start_lottery:
	.dword	0
	.align	3
__start_secondary_smp_id:
	.dword	start_secondary_smp_id
	.align	3
__start_secondary_pen_release:
	.dword	.
	.dword	start_secondary_pen_release

	/*
	 * Secondary CPU startup code 
	 */
_start_secondary:
	/*
	 * TODO: This provides a "holding pen" for platforms to hold all secondary
	 * cores are held until we're ready for them to initialise.
	 */

	/* TODO: Calculate load address of secondary_holding_pen_release */

	j	_start_secondary
#endif

	/*
	 * Note: From this point primary CPU startup is same as secondary CPU
	 */
_start_secondary_nopen:
	/* Set trap vector to spin forever to help debug */
	la	a6, _start_hang
	csrw	stvec, a6

	/* Enable MMU */
	la	a6, _load_start
	REG_L	a2, (a6)
	la	a6, __exec_start
	REG_L	a4, (a6)
	la	a6, __pgtbl_root
	REG_L	a0, (a6)
	sub	a0, a0, a4
	add	a0, a0, a2
	srl	a0, a0, 12 /* Shift right by page size */
	li	a1, SATP_MODE
	or	a0, a0, a1
	sfence.vma
	csrw	sptbr, a0

	/* Jump to final execution address */
	la	a6, __cpu_init
	REG_L	a0, (a6)
	jalr	a0

_start_hang:
	wfi
	j	_start_hang

	.align 3
__pgtbl_root:
	.dword def_pgtbl
__builtin_dtb:
	.dword dt_blob_start
__exec_start:
	.dword _code_start
__exec_end:
	.dword _code_end
__bss_start:
	.dword _bss_start
__bss_end:
	.dword _bss_end
__cpu_init:
	.dword _cpu_init
__boot_reg0:
	.dword _boot_reg0

	/*
	 * Boot register 0 passed by bootloader
	 */
	.globl _boot_reg0
_boot_reg0:
	.dword 0x0

	/*
	 * Boot register 1 passed by bootloader
	 */
	.globl _boot_reg1
_boot_reg1:
	.dword 0x0

	/* 
	 * Load start address storage
	 */
	.globl _load_start
_load_start:
	.dword 0x0

	/* 
	 * Load end address storage
	 */
	.globl _load_end
_load_end:
	.dword 0x0

	/* 
	 * Exception stacks.
	 */
__hvc_stack_end:
	.dword _hvc_stack_end

	.align 3
	.globl _cpu_init
_cpu_init:
	/* Re-setup exception handler */
	la	a6, _start_hang
	csrw	stvec, a6

#ifdef CONFIG_SMP
	/* Setup SMP ID for current processor */
	la	a6, __start_secondary_smp_id
	REG_L	a0, (a6)
	REG_L	a0, (a0)
	call	proc_setup_smp_id
#endif

	/* Setup Hypervisor Stack */
	la	a6, __hvc_stack_end
	REG_L	a5, (a6)
#ifdef CONFIG_SMP
	li	a4, CONFIG_IRQ_STACK_SIZE
	call	arch_smp_id
	mul	a4, a4, a0
#else
	li	a4, 0
#endif
	sub	sp, a5, a4

	/* Jump to C code */
	call	cpu_init

	/* Hang !!! */
	j	_start_hang

	ret

	/* Exception handling */
	.align 3
	.global _handle_exception
_handle_exception:
	/* TODO: Save arch_regs_t */
	li	a0, 0 /* TODO: a0 points to arch_regs_t on stack */
	call	do_handle_exception
	/* TODO: Restore arch_regs_t */
	sret
