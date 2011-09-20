/**
 * Copyright (c) 2011 Anup Patel.
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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief various entry points (booting, reset, exceptions) to xvisor
 */

#include <cpu_asm_macro.h>

	/* 
	 * Xvisor could be loaded any where in memory by boot loaders.
	 * The _start function ensures that Xvisor exectues from intended
	 * base address provided at compile time.
	 */
	.section .entry, "ax", %progbits
	.globl _start
_start:
	add	r4, pc, #-0x8
	ldr	r6, __code_start
	ldr	r7, __code_end
	sub	r2, r7, r6
	add	r5, r4, r2
	/*
	 * r4 -> load start
	 * r5 -> load end
	 * r6 -> execution start
	 * r7 -> execution end
	 * r2 -> code size
	 * We need to ensure that when we jump to reset code, we are executing
	 * from intended execution address. If necessary do relocation from
	 * load memory to execution memory.
	 */
	cmp	r4, r6
	beq	_start_done
	cmp	r5, r6
	blt	_start_relocate
	cmp	r7, r4
	blt	_start_relocate
_start_hang:
	b	.
_start_relocate:
	mov	r0, r4
	mov	r1, r6
	bl	_copy
_start_done:
	ldr	pc, __reset
__code_start:
	.word _code_start
__code_end:
	.word _code_end

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
	mov	r3, #0
_copy_loop:
	cmp	r3, r2
	beq	_copy_done
	cmp	r3, #16
	bge	_copy_chunk
_copy_word:
	ldmia	r0!, {r8}
	stmia	r1!, {r8}
	add	r3, r3, #4
	b	_copy_loop
_copy_chunk:
	ldmia	r0!, {r8 - r11}
	stmia	r1!, {r8 - r11}
	add	r3, r3, #16
	b	_copy_loop
_copy_done:
	mov	r0, r3
	bx	lr

	/* 
	 * Exception vector start.
	 */
	.section .entry, "ax", %progbits
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
	.word _undefined_instruction
__software_interrupt:
	.word _software_interrupt
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
__svc_stack_end:
	.word _svc_stack_end
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
	SET_CURRENT_FLAGS (CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED), r8
	/* Set Supervisor Mode Stack */
	SET_CURRENT_MODE CPSR_MODE_SUPERVISOR
	SET_CURRENT_STACK __svc_stack_end
	/* Set Undefined Mode Stack */
	SET_CURRENT_MODE CPSR_MODE_UNDEFINED
	SET_CURRENT_STACK __und_stack_end
	/* Set Abort Mode Stack */
	SET_CURRENT_MODE CPSR_MODE_ABORT
	SET_CURRENT_STACK __abt_stack_end
	/* Set IRQ Mode Stack */
	SET_CURRENT_MODE CPSR_MODE_IRQ
	SET_CURRENT_STACK __irq_stack_end
	/* Set FIQ Mode Stack */
	SET_CURRENT_MODE CPSR_MODE_FIQ
	SET_CURRENT_STACK __fiq_stack_end
	/* Set to Supervisor Mode */
	SET_CURRENT_MODE CPSR_MODE_SUPERVISOR
	/* Call CPU init function */
	b	cpu_init
	/* We should never reach here */
	b	.

	/* 
	 * Undefined instruction exception handler.
	 */
START_EXCEPTION_HANDLER _undefined_instruction, 4
	PUSH_USER_REGS
	PUSH_BANKED_REGS _undefined_instruction_bankpush_skip
	CALL_EXCEPTION_CFUNC do_undefined_instruction
	PULL_BANKED_REGS _undefined_instruction_bankpull_skip
	PULL_USER_REGS
END_EXCEPTION_HANDLER

	/* 
	 * Software interrupt exception handler.
	 */
START_EXCEPTION_HANDLER _software_interrupt, 4
	PUSH_USER_REGS
	PUSH_BANKED_REGS _software_interrupt_bankpush_skip
	CALL_EXCEPTION_CFUNC do_software_interrupt
	PULL_BANKED_REGS _software_interrupt_bankpull_skip
	PULL_USER_REGS
END_EXCEPTION_HANDLER

	/* 
	 * Prefetch abort exception handler.
	 */
START_EXCEPTION_HANDLER _prefetch_abort, 4
	PUSH_USER_REGS
	PUSH_BANKED_REGS _prefetch_abort_bankpush_skip
	CALL_EXCEPTION_CFUNC do_prefetch_abort
	PULL_BANKED_REGS _prefetch_abort_bankpull_skip
	PULL_USER_REGS
END_EXCEPTION_HANDLER

	/* 
	 * Data abort exception handler.
	 */
START_EXCEPTION_HANDLER _data_abort, 8
	PUSH_USER_REGS
	PUSH_BANKED_REGS _data_abort_bankpush_skip
	CALL_EXCEPTION_CFUNC do_data_abort
	PULL_BANKED_REGS _data_abort_bankpull_skip
	PULL_USER_REGS
END_EXCEPTION_HANDLER

	/* 
	 * Not used exception handler.
	 */
START_EXCEPTION_HANDLER _not_used, 4
	PUSH_USER_REGS
	PUSH_BANKED_REGS _not_used_bankpush_skip
	CALL_EXCEPTION_CFUNC do_not_used
	PULL_BANKED_REGS _not_used_bankpull_skip
	PULL_USER_REGS
END_EXCEPTION_HANDLER

	/* 
	 * IRQ exception handler.
	 */
START_EXCEPTION_HANDLER _irq, 4
	PUSH_USER_REGS
	PUSH_BANKED_REGS _irq_bankpush_skip
	CALL_EXCEPTION_CFUNC do_irq
	PULL_BANKED_REGS _irq_bankpull_skip
	PULL_USER_REGS
END_EXCEPTION_HANDLER

	/* 
	 * FIQ exception handler.
	 */
START_EXCEPTION_HANDLER _fiq, 4
	PUSH_USER_REGS
	PUSH_BANKED_REGS _fiq_bankpush_skip
	CALL_EXCEPTION_CFUNC do_fiq
	PULL_BANKED_REGS _fiq_bankpull_skip
	PULL_USER_REGS
END_EXCEPTION_HANDLER

