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
	csrw CSR_SIE, zero

	/*
	 * Disable FPU to detect illegal usage of
	 * floating point in kernel space
	 */
	li t0, SSTATUS_FS
	csrc CSR_SSTATUS, t0

#ifdef CONFIG_SMP
	/* Pick one HART to run the main boot sequence */
	la a3, _start_lottery
	li a2, 1
	amoadd.w a3, a2, (a3)
	bnez a3, _start_secondary
#endif

	/* Save HART ID and DTB base */
	la	a6, _bootcpu_reg0
	REG_S	a0, (a6)
	la	a6, _bootcpu_reg1
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
	la	a6, _bootcpu_reg1
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
	RISCV_PTR	0
	.align	3
__start_secondary_smp_id:
	RISCV_PTR	start_secondary_smp_id
	.align	3
__start_secondary_pen_release:
	RISCV_PTR	start_secondary_pen_release

	/*
	 * Secondary CPU startup code
	 * a0 = HART ID
	 */
_start_secondary:
	/*
	 * This provides a "holding pen" for platforms to hold all secondary
	 * cores are held until we're ready for them to initialise.
	 */

	/* Calculate load address of start_secondary_pen_release */
	la	a2, _start
	la	a6, __exec_start
	REG_L	a4, (a6)
	la	a6, __start_secondary_pen_release
	REG_L	a6, (a6)
	sub	a6, a6, a4
	add	a6, a6, a2

_start_secondary_wait:
	/* FIXME: We should use WFI to save some energy here. */
	REG_L	a7, (a6)
	nop
	nop
	nop
	nop
	bne	a7, a0, _start_secondary_wait
#endif

	/*
	 * Note: From this point primary CPU startup is same as secondary CPU
	 */
_start_secondary_nopen:
	/* Set trap vector to spin forever to help debug */
	la	a6, _start_hang
	csrw	CSR_STVEC, a6

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
	csrw	CSR_SATP, a0

	/* Jump to final execution address */
	la	a6, __cpu_init
	REG_L	a0, (a6)
	jalr	a0

_start_hang:
	wfi
	j	_start_hang

	.align 3
__pgtbl_root:
	RISCV_PTR def_pgtbl
__builtin_dtb:
	RISCV_PTR dt_blob_start
__exec_start:
	RISCV_PTR _code_start
__exec_end:
	RISCV_PTR _code_end
__bss_start:
	RISCV_PTR _bss_start
__bss_end:
	RISCV_PTR _bss_end
__cpu_init:
	RISCV_PTR _cpu_init

	/*
	 * Boot register 0 passed by bootloader
	 */
	.globl _bootcpu_reg0
_bootcpu_reg0:
	RISCV_PTR 0x0

	/*
	 * Boot register 1 passed by bootloader
	 */
	.globl _boot_reg1
_bootcpu_reg1:
	RISCV_PTR 0x0

	/*
	 * Load start address storage
	 */
	.globl _load_start
_load_start:
	RISCV_PTR 0x0

	/*
	 * Load end address storage
	 */
	.globl _load_end
_load_end:
	RISCV_PTR 0x0

	/*
	 * Exception stacks.
	 */
__hvc_stack_end:
	RISCV_PTR _hvc_stack_end

	.align 3
	.globl _cpu_init
_cpu_init:
	/* Re-setup exception handler */
	la	a6, _start_hang
	csrw	CSR_STVEC, a6

	/* Setup scratch space */
	la	a6, __hvc_stack_end
	REG_L	a5, (a6)
#ifdef CONFIG_SMP
	li	a4, CONFIG_IRQ_STACK_SIZE
	la	a6, __start_secondary_smp_id
	REG_L	a0, (a6)
	REG_L	a0, (a0)
	mul	a4, a4, a0
#else
	li	a4, 0
#endif
	sub	a5, a5, a4
	li	a3, RISCV_SCRATCH_SIZE
	sub	a5, a5, a3
	csrw	CSR_SSCRATCH, a5

	/* Setup Hypervisor Exception Stack */
	csrr	tp, CSR_SSCRATCH
	REG_S	tp, RISCV_SCRATCH_EXCE_STACK_OFFSET(tp)
	add	sp, tp, zero

#ifdef CONFIG_SMP
	/* Setup SMP ID for current processor */
	la	a6, __start_secondary_smp_id
	REG_L	a0, (a6)
	REG_L	a0, (a0)
	call	proc_setup_smp_id
#endif

	/* Jump to C code */
	call	cpu_init

	/* Hang !!! */
	j	_start_hang

	ret

.macro SAVE_ALL
	/* Swap TP and SSCRATCH */
	csrrw	tp, CSR_SSCRATCH, tp

	/* Save S0 in scratch space */
	REG_S	s0, RISCV_SCRATCH_TMP0_OFFSET(tp)

	/* Save SP in S0 */
	add	s0, sp, zero

	/* Setup exception stack */
	REG_L	sp, RISCV_SCRATCH_EXCE_STACK_OFFSET(tp)
	addi	sp, sp, -(RISCV_ARCH_REGS_SIZE)

	/* Save original SP (in S0) */
	REG_S	s0, RISCV_ARCH_REGS_OFFSET(sp)(sp)

	/* Save original S0 */
	REG_L	s0, RISCV_SCRATCH_TMP0_OFFSET(tp)
	REG_S	s0, RISCV_ARCH_REGS_OFFSET(s0)(sp)

	/* Swap TP and SSCRATCH */
	csrrw	tp, CSR_SSCRATCH, tp

	/* Save all general regisers except SP & S0 */
	REG_S	zero, RISCV_ARCH_REGS_OFFSET(zero)(sp)
	REG_S	ra, RISCV_ARCH_REGS_OFFSET(ra)(sp)
	REG_S	gp, RISCV_ARCH_REGS_OFFSET(gp)(sp)
	REG_S	tp, RISCV_ARCH_REGS_OFFSET(tp)(sp)
	REG_S	t0, RISCV_ARCH_REGS_OFFSET(t0)(sp)
	REG_S	t1, RISCV_ARCH_REGS_OFFSET(t1)(sp)
	REG_S	t2, RISCV_ARCH_REGS_OFFSET(t2)(sp)
	REG_S	s1, RISCV_ARCH_REGS_OFFSET(s1)(sp)
	REG_S	a0, RISCV_ARCH_REGS_OFFSET(a0)(sp)
	REG_S	a1, RISCV_ARCH_REGS_OFFSET(a1)(sp)
	REG_S	a2, RISCV_ARCH_REGS_OFFSET(a2)(sp)
	REG_S	a3, RISCV_ARCH_REGS_OFFSET(a3)(sp)
	REG_S	a4, RISCV_ARCH_REGS_OFFSET(a4)(sp)
	REG_S	a5, RISCV_ARCH_REGS_OFFSET(a5)(sp)
	REG_S	a6, RISCV_ARCH_REGS_OFFSET(a6)(sp)
	REG_S	a7, RISCV_ARCH_REGS_OFFSET(a7)(sp)
	REG_S	s2, RISCV_ARCH_REGS_OFFSET(s2)(sp)
	REG_S	s3, RISCV_ARCH_REGS_OFFSET(s3)(sp)
	REG_S	s4, RISCV_ARCH_REGS_OFFSET(s4)(sp)
	REG_S	s5, RISCV_ARCH_REGS_OFFSET(s5)(sp)
	REG_S	s6, RISCV_ARCH_REGS_OFFSET(s6)(sp)
	REG_S	s7, RISCV_ARCH_REGS_OFFSET(s7)(sp)
	REG_S	s8, RISCV_ARCH_REGS_OFFSET(s8)(sp)
	REG_S	s9, RISCV_ARCH_REGS_OFFSET(s9)(sp)
	REG_S	s10, RISCV_ARCH_REGS_OFFSET(s10)(sp)
	REG_S	s11, RISCV_ARCH_REGS_OFFSET(s11)(sp)
	REG_S	t3, RISCV_ARCH_REGS_OFFSET(t3)(sp)
	REG_S	t4, RISCV_ARCH_REGS_OFFSET(t4)(sp)
	REG_S	t5, RISCV_ARCH_REGS_OFFSET(t5)(sp)
	REG_S	t6, RISCV_ARCH_REGS_OFFSET(t6)(sp)

	/* Save SEPC and SSTATUS CSRs */
	csrr	s0, CSR_SEPC
	REG_S	s0, RISCV_ARCH_REGS_OFFSET(sepc)(sp)
	csrr	s1, CSR_SSTATUS
	REG_S	s1, RISCV_ARCH_REGS_OFFSET(sstatus)(sp)
.endm

.macro SAVE_HSTATUS
	csrr	s2, CSR_HSTATUS
	REG_S	s2, RISCV_ARCH_REGS_OFFSET(hstatus)(sp)
.endm

.macro HANDLE_EXCEPTION
	/* Save exception SP */
	add	s3, sp, RISCV_ARCH_REGS_SIZE
	REG_S	s3, RISCV_ARCH_REGS_OFFSET(sp_exec)(sp)

	/* Call C routine */
	add	a0, sp, zero
	call	do_handle_exception
.endm

.macro RESTORE_HSTATUS
	/* Restore HSSTATUS */
	REG_L	s4, RISCV_ARCH_REGS_OFFSET(hstatus)(sp)
	csrw	CSR_HSTATUS, s4
.endm

.macro RESTORE_ALL
	/* Restore exception SP in scratch space */
	REG_L	s0, RISCV_ARCH_REGS_OFFSET(sp_exec)(sp)
	csrr	s1, CSR_SSCRATCH
	REG_S	s0, RISCV_SCRATCH_EXCE_STACK_OFFSET(s1)

	/* Restore SEPC, SSTATUS */
	REG_L	s2, RISCV_ARCH_REGS_OFFSET(sepc)(sp)
	csrw	CSR_SEPC, s2
	REG_L	s3, RISCV_ARCH_REGS_OFFSET(sstatus)(sp)
	csrw	CSR_SSTATUS, s3

	/* Restore all general regisers except SP */
	REG_L	ra, RISCV_ARCH_REGS_OFFSET(ra)(sp)
	REG_L	gp, RISCV_ARCH_REGS_OFFSET(gp)(sp)
	REG_L	tp, RISCV_ARCH_REGS_OFFSET(tp)(sp)
	REG_L	t0, RISCV_ARCH_REGS_OFFSET(t0)(sp)
	REG_L	t1, RISCV_ARCH_REGS_OFFSET(t1)(sp)
	REG_L	t2, RISCV_ARCH_REGS_OFFSET(t2)(sp)
	REG_L	s0, RISCV_ARCH_REGS_OFFSET(s0)(sp)
	REG_L	s1, RISCV_ARCH_REGS_OFFSET(s1)(sp)
	REG_L	a0, RISCV_ARCH_REGS_OFFSET(a0)(sp)
	REG_L	a1, RISCV_ARCH_REGS_OFFSET(a1)(sp)
	REG_L	a2, RISCV_ARCH_REGS_OFFSET(a2)(sp)
	REG_L	a3, RISCV_ARCH_REGS_OFFSET(a3)(sp)
	REG_L	a4, RISCV_ARCH_REGS_OFFSET(a4)(sp)
	REG_L	a5, RISCV_ARCH_REGS_OFFSET(a5)(sp)
	REG_L	a6, RISCV_ARCH_REGS_OFFSET(a6)(sp)
	REG_L	a7, RISCV_ARCH_REGS_OFFSET(a7)(sp)
	REG_L	s2, RISCV_ARCH_REGS_OFFSET(s2)(sp)
	REG_L	s3, RISCV_ARCH_REGS_OFFSET(s3)(sp)
	REG_L	s4, RISCV_ARCH_REGS_OFFSET(s4)(sp)
	REG_L	s5, RISCV_ARCH_REGS_OFFSET(s5)(sp)
	REG_L	s6, RISCV_ARCH_REGS_OFFSET(s6)(sp)
	REG_L	s7, RISCV_ARCH_REGS_OFFSET(s7)(sp)
	REG_L	s8, RISCV_ARCH_REGS_OFFSET(s8)(sp)
	REG_L	s9, RISCV_ARCH_REGS_OFFSET(s9)(sp)
	REG_L	s10, RISCV_ARCH_REGS_OFFSET(s10)(sp)
	REG_L	s11, RISCV_ARCH_REGS_OFFSET(s11)(sp)
	REG_L	t3, RISCV_ARCH_REGS_OFFSET(t3)(sp)
	REG_L	t4, RISCV_ARCH_REGS_OFFSET(t4)(sp)
	REG_L	t5, RISCV_ARCH_REGS_OFFSET(t5)(sp)
	REG_L	t6, RISCV_ARCH_REGS_OFFSET(t6)(sp)

	/* Restore SP */
	REG_L	sp, RISCV_ARCH_REGS_OFFSET(sp)(sp)
.endm

	/* Exception handling */
	.align 3
	.global _handle_exception
_handle_exception:
	SAVE_ALL

	HANDLE_EXCEPTION

	RESTORE_ALL

	sret

	.align 3
	.global _handle_hyp_exception
_handle_hyp_exception:
	SAVE_ALL

	SAVE_HSTATUS

	HANDLE_EXCEPTION

	RESTORE_HSTATUS

	RESTORE_ALL

	sret
