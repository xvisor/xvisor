/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file arm_entry.S
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Entry point code for basic firmware
 */

#include <arm_asm_macro.h>
#include <gic_config.h>
#include <arm_plat.h>

	/* 
	 * Basic firmware could be loaded any where in memory.
	 * The _start function ensures that it exectues from intended
	 * base address provided at compile time.
	 */
	.section .text, "ax", %progbits
	.globl _start
_start:
	/*
	 * x4 -> load start
	 * x5 -> load end
	 * x6 -> execution start
	 * x7 -> execution end
	 * x8 -> code size
	 */
	adr	x4, .		/* x4 <- pc */
	/* Disable IRQ & FIQ */
	msr	daifset, #3
	ldr	x6, __reloc_region_start
	ldr	x7, __reloc_region_end
	sub	x8, x7, x6
	add	x5, x4, x8

	/* If not-running at EL1 then hang */
	mrs	x0, CurrentEL
	cmp	x0, #PSR_MODE_EL1t
	ccmp	x0, #PSR_MODE_EL1h, #0x4, ne
	b.ne	_start_hang

	/* Set GIC priority mask bit [7] = 1 */
	ldr	x0, __gic_cpu_base		/* CPU GIC base */
	mov	x1, #0x1
	str	w1, [x0]			/* GIC CPU CTRL */
	mov	x1, #0xFF
	str	w1, [x0, #0x4]			/* GIC CPU PMR */

	/* Skip secondary spin for primary core */
	mrs	x0, mpidr_el1
	ands	x0, x0, #15			/* CPU number */
	cmp	x0, #0
	b.eq	primary_cpu_boot

	/* Secondary spin loop */
	ldr	x0, __jump_addr
	str	xzr, [x0]
secondary_cpu_spin:
	ldr	x1, [x0]
	cbnz	x1, secondary_cpu_jump
	wfe
	b	secondary_cpu_spin
secondary_cpu_jump:
	br	x1

	.align 3
__gic_cpu_base:
	.dword	GIC_CPU_BASE

	.align 3
	.global __jump_addr
__jump_addr:
	.dword ARM_PLAT_SPIN_ADDR

	/* Primary core boot-up sequence */
primary_cpu_boot:
	/* Check if relocation required */
	cmp	x4, x6
	b.eq	_relocate_end

	/*
	 * We need to clear the A bit in sctlr_el1
	 * for the relocate routine to work properly
	 */
	mrs    	x3, sctlr_el1
	ldr	x2, __sctlr_mmu_clear
	and	x3, x3, x2
	/* We also enable I/D caching */
	ldr	x2, __sctlr_mmu_val
	orr	x3, x3, x2
	msr     sctlr_el1, x3
	dsb	sy
	isb

	mov	x0, x6	/* destination */
	mov	x1, x4  /* source */
	mov	x2, x8  /* byte count */
	/* TODO: Handle overlaps */
_relocate:
	subs	x2, x2, #8
	b.mi	2f
1:	ldr	x3, [x1], #8
	subs	x2, x2, #8
	str	x3, [x0], #8
	b.pl	1b
2:	adds	x2, x2, #4
	b.mi	3f
	ldr	w3, [x1], #4
	sub	x2, x2, #4
	str	w3, [x0], #4
3:	adds	x2, x2, #2
	b.mi	4f
	ldrh	w3, [x1], #2
	sub	x2, x2, #2
	strh	w3, [x0], #2
4:	adds	x2, x2, #1
	b.mi	_relocate_end
	ldrb	w3, [x1]
	strb	w3, [x0]
_relocate_end:

	ldr	x0, __relocated
	br	x0

_relocated:

	/* Zero out bss & heap */
	ldr	x0, __zero_region_start
	ldr	x1, __zero_region_end
1:	subs	x2, x1, x0
	b.le	2f
	str	xzr, [x0]
	add	x0, x0, 8
	b	1b
2:
	/* Set exception vectors */
	adr	x0, vectors
	msr	vbar_el1, x0
	/* Set EL1 stack-pointer */
	ldr	x0, __el1_stack
	mov	sp, x0
	/* Set EL0 stack-pointer */
	ldr	x0, __el0_stack
	msr	sp_el0, x0

	bl	arm_init
	bl	arm_main

_start_hang:
	b	.

	.align 3
__relocated:
	.dword	_relocated

	.globl _switch_to_user_mode
_switch_to_user_mode:
	msr	tpidr_el1, x0
	mov	x0, #(PSR_FIQ_DISABLED | PSR_IRQ_DISABLED |\
		      PSR_ASYNC_ABORT_DISABLED | PSR_MODE_EL0t)
	msr	spsr_el1, x0
	mrs	x0, tpidr_el1
	msr	elr_el1, lr
	eret

	.align 3
__sctlr_mmu_clear:
	.dword ~(SCTLR_A_MASK)
__sctlr_mmu_val:
	.dword (SCTLR_C_MASK | SCTLR_I_MASK)
__reloc_region_start:
	.dword _reloc_region_start
__reloc_region_end:
	.dword _reloc_region_end
__zero_region_start:
	.dword _zero_region_start
__zero_region_end:
	.dword _zero_region_end

	/* 
	 * Stacks.
	 */
__el0_stack:
	.dword _usr_stack_end
__el1_stack:
	.dword _svc_stack_end


	.align	11
ENTRY(vectors)
	ventry	svc_sync_invalid	/* Synchronous EL1t */
	ventry	svc_irq_invalid		/* IRQ EL1t */
	ventry	svc_fiq_invalid		/* FIQ EL1t */
	ventry	svc_error_invalid	/* Error EL1t */

	ventry	svc_sync		/* Synchronous EL1h */
	ventry	svc_irq			/* IRQ EL1h */
	ventry	svc_fiq_invalid		/* FIQ EL1h */
	ventry	svc_error_invalid	/* Error EL1h */

	ventry	usr_sync_a64		/* Synchronous 64-bit EL0 */
	ventry	usr_irq_a64		/* IRQ 64-bit EL0 */
	ventry	usr_fiq_a64		/* FIQ 64-bit EL0 */
	ventry	usr_error_a64		/* Error 64-bit EL0 */

	ventry	usr_sync_a32		/* Synchronous 32-bit EL0 */
	ventry	usr_irq_a32		/* IRQ 32-bit EL0 */
	ventry	usr_fiq_a32		/* FIQ 32-bit EL0 */
	ventry	usr_error_a32		/* Error 32-bit EL0 */
END(vectors)

EXCEPTION_HANDLER svc_sync_invalid
	PUSH_REGS
	mov	x1, EXC_SVC_SYNC_SP0
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER svc_irq_invalid
	PUSH_REGS
	mov	x1, EXC_SVC_IRQ_SP0
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER svc_fiq_invalid
	PUSH_REGS
	mov	x1, EXC_SVC_FIQ_SP0
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER svc_error_invalid
	PUSH_REGS
	mov	x1, EXC_SVC_SERROR_SP0
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER svc_sync
	PUSH_REGS
	mov	x1, EXC_SVC_SYNC_SPx
	CALL_EXCEPTION_CFUNC do_sync
	PULL_REGS

EXCEPTION_HANDLER svc_irq
	PUSH_REGS
	mov	x1, EXC_SVC_IRQ_SPx
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS

EXCEPTION_HANDLER svc_fiq
	PUSH_REGS
	mov	x1, EXC_SVC_FIQ_SPx
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER svc_error
	PUSH_REGS
	mov	x1, EXC_SVC_SERROR_SPx
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER usr_sync_a64
	PUSH_REGS
	mov	x1, EXC_USR_SYNC_A64
	CALL_EXCEPTION_CFUNC do_sync
	PULL_REGS

EXCEPTION_HANDLER usr_irq_a64
	PUSH_REGS
	mov	x1, EXC_USR_IRQ_A64
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS

EXCEPTION_HANDLER usr_fiq_a64
	PUSH_REGS
	mov	x1, EXC_USR_FIQ_A64
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER usr_error_a64
	PUSH_REGS
	mov	x1, EXC_USR_SERROR_A64
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER usr_sync_a32
	PUSH_REGS
	mov	x1, EXC_USR_SYNC_A32
	CALL_EXCEPTION_CFUNC do_sync
	PULL_REGS

EXCEPTION_HANDLER usr_irq_a32
	PUSH_REGS
	mov	x1, EXC_USR_IRQ_A32
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS

EXCEPTION_HANDLER usr_fiq_a32
	PUSH_REGS
	mov	x1, EXC_USR_FIQ_A32
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS

EXCEPTION_HANDLER usr_error_a32
	PUSH_REGS
	mov	x1, EXC_USR_SERROR_A32
	CALL_EXCEPTION_CFUNC do_bad_mode
	PULL_REGS


