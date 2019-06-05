/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file arch_entry.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief Entry point code for basic firmware
 */

#include <arch_asm.h>

	/*
	 * Basic firmware could be loaded any where in memory.
	 * The _start function ensures that it exectues from intended
	 * base address provided at compile time.
	 */
	.align	3
	.section .entry, "ax", %progbits
	.globl _start
_start:
	/* Wait if hartid != 0 */
	blt	zero, a0, _secondary_wait

	/* Relocate if required */
	la	a4, exec_start
	REG_L	a4, 0(a4)
	la	a5, _fw_start
	beq	a4, a5, _skip_relocate
	la	a6, _fw_end
_relocate:
	REG_L	a7, 0(a5)
	REG_S	a7, 0(a4)
	add	a4, a4, __SIZEOF_POINTER__
	add	a5, a5, __SIZEOF_POINTER__
	blt	a5, a6, _relocate
	la	a4, exec_start
	REG_L	a4, 0(a4)
	jr	a4
_skip_relocate:

	/* Zero-out BSS */
	la	a4, _bss_start
	la	a5, _bss_end
_bss_zero:
	REG_S	zero, (a4)
	add	a4, a4, __SIZEOF_POINTER__
	blt	a4, a5, _bss_zero

	/* Save boot arg0 & arg1 */
	la	a4, boot_arg0
	REG_S	a0, 0(a4)
	la	a4, boot_arg1
	REG_S	a1, 0(a4)

	/* Disable and clear all interrupts */
	csrw	sie, zero
	csrw	sip, zero

	/* Setup SSCRATCH */
	la	tp, _fw_end
	li	t0, 0x2000
	add	tp, tp, t0
	li	t1, PT_SCRATCH_SIZE
	sub	tp, tp, t1
	csrw	sscratch, tp

	/* Setup SP */
	la	tp, _fw_end
	li	t0, 0x4000
	add	sp, tp, t0

	/* Setup trap handler */
	la	tp, _trap_handler
	csrw	stvec, tp

	/* clear tp */
	li	tp, 0

	/* Jump to C code */
	call	basic_init
	call	basic_main

	/* We don't expect to reach here hence just hang */
	j	_start_hang

	.align 3
	.section .entry, "ax", %progbits
	.globl _secondary_wait
_secondary_wait:
	la	t1, _jump_linux_addr
	REG_L	t1, 0(t1)
	REG_S	zero, 0(t1)

_secondary_loop:
	REG_L	t4, 0(t1)
	nop
	nop
	nop
	nop
	beqz	t4, _secondary_loop

	/* Disable and clear all interrupts */
	csrw	sie, zero
	csrw	sip, zero

	/* Clear sp & tp */
	li	sp, 0
	li	tp, 0

	/* Jump to kernel address directly as a0 & a1 are already set */
	jr	t4

	.align 3
	.section .entry, "ax", %progbits
	.globl _start_hang
_start_hang:
	wfi
	j	_start_hang

	.align	3
	.section .entry, "ax", %progbits
exec_start:
	RISCV_PTR	_fw_start
exec_end:
	RISCV_PTR	_fw_end

	.align	3
	.globl boot_arg0
	.section .entry, "ax", %progbits
boot_arg0:
	RISCV_PTR	0

	.align	3
	.globl _jump_linux_addr
	.section .data
_jump_linux_addr:
	RISCV_PTR	jump_linux_addr

	.align	3
	.globl jump_linux_addr
	.section .data
jump_linux_addr:
	RISCV_PTR	0

	.align	3
	.globl boot_arg1
	.section .entry, "ax", %progbits
boot_arg1:
	RISCV_PTR	0

	.align 3
	.section .entry, "ax", %progbits
	.globl _trap_handler
_trap_handler:
	/* Swap SP and SSCRATCH */
	csrrw	sp, sscratch, sp

	/* Setup exception stack */
	add	sp, sp, -(PT_REGS_SIZE)

	/* Save RA, T0, T1, and T2 */
	REG_S	ra, PT_REGS_OFFSET(ra)(sp)
	REG_S	t0, PT_REGS_OFFSET(t0)(sp)
	REG_S	t1, PT_REGS_OFFSET(t1)(sp)
	REG_S	t2, PT_REGS_OFFSET(t2)(sp)

	/* Save original SP and restore SSCRATCH */
	add	t0, sp, PT_REGS_SIZE
	csrrw	t0, sscratch, t0
	REG_S	t0, PT_REGS_OFFSET(sp)(sp)

	/* Save SEPC and SSTATUS CSRs */
	csrr	t0, sepc
	csrr	t1, sstatus

	/*
	 * Note: Fast path trap handling can be done here
	 * using SP, RA, T0, T1, and T2 registers where
	 * T0 <- SEPC
	 * T1 <- SSTATUS
	 */

	/* Save SEPC and SSTATUS CSRs */
	REG_S	t0, PT_REGS_OFFSET(sepc)(sp)
	REG_S	t1, PT_REGS_OFFSET(sstatus)(sp)

	/* Save all general regisers except SP, RA, T0, T1, and T2 */
	REG_S	zero, PT_REGS_OFFSET(zero)(sp)
	REG_S	gp, PT_REGS_OFFSET(gp)(sp)
	REG_S	tp, PT_REGS_OFFSET(tp)(sp)
	REG_S	s0, PT_REGS_OFFSET(s0)(sp)
	REG_S	s1, PT_REGS_OFFSET(s1)(sp)
	REG_S	a0, PT_REGS_OFFSET(a0)(sp)
	REG_S	a1, PT_REGS_OFFSET(a1)(sp)
	REG_S	a2, PT_REGS_OFFSET(a2)(sp)
	REG_S	a3, PT_REGS_OFFSET(a3)(sp)
	REG_S	a4, PT_REGS_OFFSET(a4)(sp)
	REG_S	a5, PT_REGS_OFFSET(a5)(sp)
	REG_S	a6, PT_REGS_OFFSET(a6)(sp)
	REG_S	a7, PT_REGS_OFFSET(a7)(sp)
	REG_S	s2, PT_REGS_OFFSET(s2)(sp)
	REG_S	s3, PT_REGS_OFFSET(s3)(sp)
	REG_S	s4, PT_REGS_OFFSET(s4)(sp)
	REG_S	s5, PT_REGS_OFFSET(s5)(sp)
	REG_S	s6, PT_REGS_OFFSET(s6)(sp)
	REG_S	s7, PT_REGS_OFFSET(s7)(sp)
	REG_S	s8, PT_REGS_OFFSET(s8)(sp)
	REG_S	s9, PT_REGS_OFFSET(s9)(sp)
	REG_S	s10, PT_REGS_OFFSET(s10)(sp)
	REG_S	s11, PT_REGS_OFFSET(s11)(sp)
	REG_S	t3, PT_REGS_OFFSET(t3)(sp)
	REG_S	t4, PT_REGS_OFFSET(t4)(sp)
	REG_S	t5, PT_REGS_OFFSET(t5)(sp)
	REG_S	t6, PT_REGS_OFFSET(t6)(sp)

	/* Call C routine */
	add	a0, sp, zero
	call	do_exec

	/* Restore all general regisers except SP, RA, T0, T1, T2, and T3 */
	REG_L	gp, PT_REGS_OFFSET(gp)(sp)
	REG_L	tp, PT_REGS_OFFSET(tp)(sp)
	REG_L	s0, PT_REGS_OFFSET(s0)(sp)
	REG_L	s1, PT_REGS_OFFSET(s1)(sp)
	REG_L	a0, PT_REGS_OFFSET(a0)(sp)
	REG_L	a1, PT_REGS_OFFSET(a1)(sp)
	REG_L	a2, PT_REGS_OFFSET(a2)(sp)
	REG_L	a3, PT_REGS_OFFSET(a3)(sp)
	REG_L	a4, PT_REGS_OFFSET(a4)(sp)
	REG_L	a5, PT_REGS_OFFSET(a5)(sp)
	REG_L	a6, PT_REGS_OFFSET(a6)(sp)
	REG_L	a7, PT_REGS_OFFSET(a7)(sp)
	REG_L	s2, PT_REGS_OFFSET(s2)(sp)
	REG_L	s3, PT_REGS_OFFSET(s3)(sp)
	REG_L	s4, PT_REGS_OFFSET(s4)(sp)
	REG_L	s5, PT_REGS_OFFSET(s5)(sp)
	REG_L	s6, PT_REGS_OFFSET(s6)(sp)
	REG_L	s7, PT_REGS_OFFSET(s7)(sp)
	REG_L	s8, PT_REGS_OFFSET(s8)(sp)
	REG_L	s9, PT_REGS_OFFSET(s9)(sp)
	REG_L	s10, PT_REGS_OFFSET(s10)(sp)
	REG_L	s11, PT_REGS_OFFSET(s11)(sp)
	REG_L	t3, PT_REGS_OFFSET(t3)(sp)
	REG_L	t4, PT_REGS_OFFSET(t4)(sp)
	REG_L	t5, PT_REGS_OFFSET(t5)(sp)
	REG_L	t6, PT_REGS_OFFSET(t6)(sp)

	/* Load T0 and T1 with SEPC and SSTATUS */
	REG_L	t0, PT_REGS_OFFSET(sepc)(sp)
	REG_L	t1, PT_REGS_OFFSET(sstatus)(sp)

	/*
	 * Note: Jump here after fast trap handling
	 * using SP, RA, T0, T1, and T2
	 * T0 <- SEPC
	 * T1 <- SSTATUS
	 */

	/* Restore SEPC and SSTATUS CSRs */
	csrw	sepc, t0
	csrw	sstatus, t1

	/* Restore RA, T0, T1, and T2 */
	REG_L	ra, PT_REGS_OFFSET(ra)(sp)
	REG_L	t0, PT_REGS_OFFSET(t0)(sp)
	REG_L	t1, PT_REGS_OFFSET(t1)(sp)
	REG_L	t2, PT_REGS_OFFSET(t2)(sp)

	/* Restore SP */
	REG_L	sp, PT_REGS_OFFSET(sp)(sp)

	sret
