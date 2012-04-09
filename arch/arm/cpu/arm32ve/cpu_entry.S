/**
 * Copyright (c) 2012 Anup Patel.
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
 * @brief entry points (booting, reset, exceptions) for ARMv7a family
 */

#include <cpu_defines.h>

	/* 
	 * Xvisor could be loaded any where in memory by boot loaders.
	 * The _start ensures that Xvisor exectues from intended
	 * base address provided at compile time.
	 */
	.section .entry, "ax", %progbits
	.globl _start
_start:
	/*
	 * r4 -> load start
	 * r5 -> load end
	 * r6 -> execution start
	 * r7 -> execution end
	 * r2 -> code size
	 */
	add	r4, pc, #-0x8
	ldr	r6, __exec_start
	ldr	r7, __exec_end
	sub	r2, r7, r6
	add	r5, r4, r2
	ldr	r0, __load_start
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r4, [r0]
	ldr	r0, __load_end
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r5, [r0]
	/*
	 * Hang if execution start is not 4 KB aligned
	 */
	mov	r0, r6
	bfc	r0, #0, #12
	cmp	r0, r6
	blt	_start_hang
	/*
	 * Hang if execution end is not 4 KB aligned
	 */
	mov	r0, r7
	bfc	r0, #0, #12
	cmp	r0, r7
	blt	_start_hang
	/*
	 * Relocate code if load start is not 4 KB aligned
	 */
	mov	r0, r4
	bfc	r0, #0, #12
	cmp	r0, r4
	beq	_start_mmu_init
_start_relocate:
	/*
	 * Relocate copy function at end after load end address
	 */
	ldr	r0, __copy_start
	ldr	r1, __copy_end
	sub	r2, r1, r0
	sub	r0, r0, r6
	add	r0, r0, r4
	mov	r1, r5
	bl	_copy
	/*
	 * Use newly relocated copy function to relocate entire code
	 */
	mov	r0, r4
	mov	r1, r5
	sub	r2, r5, r4
	mov	r1, r4
	bfc	r1, #0, #12
	bl	_start_nextpc1
_start_nextpc1:
	add	lr, lr, #16
	sub	lr, lr, r4
	add	lr, lr, r1
	bx	r5
	/*
	 * Update load start and load end
	 * r4 -> new load start
	 * r5 -> new load end
	 */
	mov	r0, r4
	bfc	r0, #0, #12
	sub	r1, r4, r0
	sub	r4, r4, r1
	sub	r5, r5, r1
	ldr	r0, __load_start
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r4, [r0]
	ldr	r0, __load_end
	sub	r0, r0, r6
	add	r0, r0, r4
	str	r5, [r0]
_start_mmu_init:
	/*
	 * Enable hypervisor mode
	 */
	bl	_enable_hvc
	/*
	 * Setup initial TTBL using C code and a temporary stack
	 */
	ldr	sp, __hvc_stack_end
	sub	sp, sp, r6
	add	sp, sp, r4
	mov	r0, r4
	mov	r1, r5
	mov	r2, r6
	mov	r3, r7
	bl	_setup_initial_ttbl
	/*
	 * Setup Hypervisor System Control Register
	 */
	ldr	r1, __hsctlr_mmu_val
	ldr	r2, __hsctlr_mmu_clear
	mrc     p15, 4, r3, c1, c0, 0
	and	r3, r3, r2
	orr	r3, r3, r1
	mcr     p15, 4, r3, c1, c0, 0
	/*
	 * Jump to reset code
	 */
	ldr	pc, __reset
_start_hang:
	b	.
__hsctlr_mmu_clear:
	.word ~(HSCTLR_A_MASK)
__hsctlr_mmu_val:
	.word (HSCTLR_M_MASK | HSCTLR_C_MASK | HSCTLR_I_MASK)
__exec_start:
	.word _code_start
__exec_end:
	.word _code_end
__load_start:
	.word _load_start
__load_end:
	.word _load_end
__copy_start:
	.word _copy
__copy_end:
	.word _copy_end

	/* 
	 * Load start address storage
	 */
	.globl _load_start
_load_start:
	.word 0x0

	/* 
	 * Load end address storage
	 */
	.globl _load_end
_load_end:
	.word 0x0

	/* 
	 * Copy data from source to destination
	 * Arguments:
	 *  r0 -> source address
	 *  r1 -> destination address
	 *  r2 -> byte count
	 * Return:
	 *  r0 -> bytes copied
	 */
	.section .entry, "ax", %progbits
	.globl _copy
_copy:
	mov	r3, r2
_copy_loop:
	cmp	r3, #0
	beq	_copy_done
	cmp	r3, #16
	bge	_copy_chunk
_copy_word:
	ldmia	r0!, {r8}
	stmia	r1!, {r8}
	sub	r3, r3, #4
	b	_copy_loop
_copy_chunk:
	ldmia	r0!, {r8 - r11}
	stmia	r1!, {r8 - r11}
	sub	r3, r3, #16
	b	_copy_loop
_copy_done:
	mov	r0, r2
	bx	lr
_copy_end:

	/* 
	 * Enable hypervisor mode.
	 * Arguments:
	 *  r4 -> load start
	 *  r5 -> load end
	 *  r6 -> execution start
	 *  r7 -> execution end
	 * Note: This function should be called before enabling MMU
	 */
	.section .entry, "ax", %progbits
	.globl _enable_hvc
_enable_hvc:
	/* Save lr and jump to hvc_start */
	mov	r12, lr
	b	_enable_hvc_start
	.balign	256
_enable_hvc_tmon_table:
	b	.
	b	.
	b	_enable_hvc_tmon
	b	.
	b	.
	b	.
	b	.
	b	.
__enable_hvc_tmon_table:
	.word _enable_hvc_tmon_table
	.balign	256
_enable_hvc_thyp_table:
	b	.
	b	.
	b	.
	b	.
	b	.
	b	_enable_hvc_thyp
	b	.
	b	.
__enable_hvc_thyp_table:
	.word _enable_hvc_thyp_table
_enable_hvc_tmon:
	/* Get secure configuration register */
	mrc	p15, 0, r0, c1, c1, 0	
	bic	r0, r0, #0x07f
	mov	r1, #0x10
	lsl	r1, r1, #4
	orr	r1, r1, #0x1
	orr	r0, r0, r1
	mcr	p15, 0, r0, c1, c1, 0
	/* Get temp hypervisor vector base address */
	ldr	r0, __enable_hvc_thyp_table
	sub	r0, r0, r6
	add	r0, r0, r4
	mcr	p15, 4, r0, c12, c0, 0
	movs	pc, lr
_enable_hvc_thyp:
	/* Yeppie! in hypervisor mode */
	mrs	r0, cpsr_all
	msr	spsr_hyp, r0
	msr	elr_hyp, r12
	eret
_enable_hvc_start:
	/* Get temp monitor vector base address */
	ldr	r0, __enable_hvc_tmon_table
	sub	r0, r0, r6
	add	r0, r0, r4
	/* Set temp monitor vector base address */
	mcr	p15, 0, r0, c12, c0, 1
	/* Call temp monitor mode code */
	smc	#0
	/* Call temp hypervisor mode code */
	hvc	#0
	/* Hang! Not supposed to reach here */
	b	.

	/* 
	 * Exception vector start.
	 */
	.section .entry, "ax", %progbits
	.balign	256
	.globl _start_vect
_start_vect:
	ldr	pc, __reset
	ldr	pc, __undefined_instruction
	ldr	pc, __software_interrupt
	ldr	pc, __prefetch_abort
	ldr	pc, __data_abort
	ldr	pc, __hypervisor_trap
	ldr	pc, __irq
	ldr	pc, __fiq
__reset:
	.word _reset
__undefined_instruction:
	.word _undef_inst
__software_interrupt:
	.word _soft_irq
__prefetch_abort:
	.word _prefetch_abort
__data_abort:
	.word _data_abort
__hypervisor_trap:
	.word _hyp_trap
__irq:
	.word _irq
__fiq:
	.word _fiq
	.global _end_vect
_end_vect:
	b	.

	/* 
	 * Exception stacks.
	 */
__hvc_stack_end:
	.word _hvc_stack_end

#define HCR_INITIAL_VAL (HCR_AMO_MASK | HCR_IMO_MASK | HCR_FMO_MASK)
__hcr_initial_set:
	.word (HCR_INITIAL_VAL)
__hcr_initial_clear:
	.word ~(0x0)

	/* 
	 * Reset exception handler.
	 * Reset hardware state before starting Xvisor.
	 */
	.globl _reset
_reset:
	/* Clear lr */
	mov 	lr, #0
	/* Clear a register for temporary usage */
	mov	r0, #0
	mov	r1, #0
	mov	r2, #0
	/* Disable IRQ & FIQ */
	mrs	r0, cpsr_all
	orr	r0, r0, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED)
	msr	cpsr_cxsf, r0
	/* Set Hypervisor Mode Stack */
	ldr	sp, __hvc_stack_end
	/* Initial Hypervisor Configuration */
	ldr	r1, __hcr_initial_set
	ldr	r2, __hcr_initial_clear
	mrc     p15, 4, r0, c1, c1, 0
	and	r0, r0, r2
	orr	r0, r0, r1
	mcr     p15, 4, r0, c1, c1, 0
	/* Call CPU init function */
	b	cpu_init
	/* We should never reach here */
	b	.

	/* 
	 * Helper Macros for Exception Handlers
	 */
.macro EXCEPTION_HANDLER irqname
	.align 5
\irqname:
.endm

/* Push registers on stack */
.macro PUSH_REGS
	str	lr, [sp, #-4]
	str	sp, [sp, #-8]
	sub	sp, sp, #(4*15)
	stm	sp, {r0-r12}
	mov	r0, r0
	sub	sp, sp, #(4*2)
	mrs	r0, spsr_hyp
	str	r0, [sp, #0]
	mrs	r0, elr_hyp
	str	r0, [sp, #4]
.endm

/* Call C function to handle exception */
.macro CALL_EXCEPTION_CFUNC cfunc
	mov	r0, sp
	bl	\cfunc
.endm

/* Pull registers from stack */
.macro PULL_REGS
	ldr	r0, [sp], #4
	msr	spsr_hyp, r0
	ldr	r0, [sp], #4
	msr	elr_hyp, r0
	ldm	sp, {r0-r14}
	mov	r0, r0
	eret
.endm

	/* 
	 * Undefined instruction exception handler.
	 */
EXCEPTION_HANDLER _undef_inst
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_undef_inst
	PULL_REGS

	/* 
	 * Software interrupt exception handler.
	 */
EXCEPTION_HANDLER _soft_irq
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_soft_irq
	PULL_REGS

	/* 
	 * Prefetch abort exception handler.
	 */
EXCEPTION_HANDLER _prefetch_abort
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_prefetch_abort
	PULL_REGS

	/* 
	 * Data abort exception handler.
	 */
EXCEPTION_HANDLER _data_abort
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_data_abort
	PULL_REGS

	/* 
	 * Hypervisor trap exception handler.
	 */
EXCEPTION_HANDLER _hyp_trap
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_hyp_trap
	PULL_REGS

	/* 
	 * IRQ exception handler.
	 */
EXCEPTION_HANDLER _irq
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS

	/* 
	 * FIQ exception handler.
	 */
EXCEPTION_HANDLER _fiq
	PUSH_REGS
	CALL_EXCEPTION_CFUNC do_fiq
	PULL_REGS

