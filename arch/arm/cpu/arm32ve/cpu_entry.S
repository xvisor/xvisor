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
	mov	r1, #0x13
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
	mov	lr, r12
	bx	lr
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
	ldr	pc, __not_used
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
__not_used:
	.word _not_used
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
__und_stack_end:
	.word _und_stack_end
__abt_stack_end:
	.word _abt_stack_end
__irq_stack_end:
	.word _irq_stack_end
__fiq_stack_end:
	.word _fiq_stack_end

	/* 
	 * Reset exception handler.
	 * Reset hardware state before starting Xvisor.
	 */
	.globl _reset
_reset:
	/* Clear a register for temporary usage */
	mov	r8, #0
	/* Disable IRQ & FIQ */
	mrs	r8, cpsr_all
	orr	r8, r8, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED)
	msr	cpsr_cxsf, r8
	/* Set Hypervisor Mode Stack */
	ldr	sp, __hvc_stack_end
	/* Call CPU init function */
	b	cpu_init
	/* We should never reach here */
	b	.

	/* 
	 * Helper Macros for Exception Handlers
	 */
.macro EXCEPTION_HANDLER irqname, lroffset
	.align 5
\irqname:
	sub	lr, lr, #\lroffset
.endm

/* If came from priviledged mode then push banked registers */
.macro PUSH_REGS skip_lable
	str     lr, [sp, #-4]!;		/* Push the return address */
	sub	sp, sp, #(4*2);		/* Adjust the stack pointer */
	stm	sp, {r13-r14}^;		/* Push user mode registers */
	stmdb	sp!, {r0-r12};		/* Push user mode registers */
	mrs     r4, spsr_all;		/* Put SPSR on stack & NOP for prev. */
	str     r4, [sp, #-4]!
	and	r0, r4, #CPSR_MODE_MASK
	cmp	r0, #CPSR_MODE_USER
	beq	\skip_lable
	add	r1, sp, #(4*14)
	mrs	r5, cpsr_all
	orr	r4, r4, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED)
	msr	cpsr_cxsf, r4
	stm	r1, {r13-r14}
	msr	cpsr_cxsf, r5		/* Restore CPSR & NOP for prev. */
\skip_lable:
.endm

/* Call C function to handle exception */
.macro CALL_EXCEPTION_CFUNC cfunc
	mov	r0, sp
	bl	\cfunc
.endm

/* If going back to priviledged mode then pull banked registers */
.macro PULL_REGS skip_lable skip_lable1
	ldr     r4, [sp, #0]
	and	r0, r4, #CPSR_MODE_MASK
	cmp	r0, #CPSR_MODE_USER
	beq	\skip_lable
	mrs	r1, cpsr_all
	and 	r1, r1, #CPSR_MODE_MASK
	cmp	r0, r1
	beq	\skip_lable1
	add	r1, sp, #(4*14)
	mrs	r5, cpsr_all
	orr	r4, r4, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED)
	msr	cpsr_cxsf, r4
	ldm	r1, {r13-r14}
	msr	cpsr_cxsf, r5		/* Restore CPSR & NOP for prev. inst */
\skip_lable:
	ldr     r0, [sp], #0x0004;	/* Get SPSR from stack */
	msr     spsr_cxsf, r0;
	ldmia   sp, {r0-r14}^;		/* Restore registers (user) */
	add     sp, sp, #(4*15);	/* Adjust stack & NOP for prev. */
	ldr     lr, [sp], #0x0004	/* Pull return address */
	movs	pc, lr
\skip_lable1:
	ldr     r0, [sp], #0x0004;	/* Get SPSR from stack */
	msr     spsr_cxsf, r0;
	ldm	sp, {r0-r15}^;		/* Restore all registers & CPSR */
	mov     r0, r0;			/* NOP for prev. */
.endm

	/* 
	 * Undefined instruction exception handler.
	 */
EXCEPTION_HANDLER _undef_inst, 4
	PUSH_REGS _undef_inst_push
	CALL_EXCEPTION_CFUNC do_undef_inst
	PULL_REGS _undef_inst_pull _undef_inst_pull1

	/* 
	 * Software interrupt exception handler.
	 */
EXCEPTION_HANDLER _soft_irq, 4
	PUSH_REGS _soft_irq_push
	CALL_EXCEPTION_CFUNC do_soft_irq
	PULL_REGS _soft_irq_pull _soft_irq_pull1

	/* 
	 * Prefetch abort exception handler.
	 */
EXCEPTION_HANDLER _prefetch_abort, 4
	PUSH_REGS _prefetch_abort_push
	CALL_EXCEPTION_CFUNC do_prefetch_abort
	PULL_REGS _prefetch_abort_pull _prefetch_abort_pull1

	/* 
	 * Data abort exception handler.
	 */
EXCEPTION_HANDLER _data_abort, 8
	PUSH_REGS _data_abort_push
	CALL_EXCEPTION_CFUNC do_data_abort
	PULL_REGS _data_abort_pull _data_abort_pull1

	/* 
	 * Not used exception handler.
	 */
EXCEPTION_HANDLER _not_used, 4
	PUSH_REGS _not_used_push
	CALL_EXCEPTION_CFUNC do_not_used
	PULL_REGS _not_used_pull _not_used_pull1

	/* 
	 * IRQ exception handler.
	 */
EXCEPTION_HANDLER _irq, 4
	PUSH_REGS _irq_push
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS _irq_pull _irq_pull1

	/* 
	 * FIQ exception handler.
	 */
EXCEPTION_HANDLER _fiq, 4
	PUSH_REGS _fiq_push
	CALL_EXCEPTION_CFUNC do_fiq
	PULL_REGS _fiq_pull _fiq_pull1
