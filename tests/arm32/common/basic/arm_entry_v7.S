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
 * @file arm_entry_v7.S
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Entry point code for ARM test code
 */

#include <arm_asm_macro.h>

	/* 
	 * ARM Basic test could be loaded any where in memory by boot loaders.
	 * The _start function ensures that it exectues from intended
	 * base address provided at compile time.
	 */
	.section .expvect, "ax", %progbits
	.globl _start
_start:
	add	r0, pc, #-0x8
	cpsid	if
	ldr	r2, __code_start
	ldr	r3, __code_end
	sub	r4, r3, r2
	add	r1, r0, r4
	/*
	 * r0 -> load start
	 * r1 -> load end
	 * r2 -> execution start
	 * r3 -> execution end
	 * r4 -> code size
	 * We need to ensure that when we jump to reset code, we are executing
	 * from intended execution address. If necessary do relocation from
	 * load memory to execution memory.
	 */
	/* Core-1 should spin and core-0 should go ahead */
	mrc	p15, 0, r5, c0, c0, 5
	ands	r5, r5, #0x3
	bne	_secondary_loop

	cmp	r0, r2
	beq	_start_done
	cmp	r1, r2
	blt	_start_relocate
	cmp	r3, r0
	blt	_start_relocate

_secondary_loop:
	/* Enable the GIC CPU interface for this core */
	ldr	r0, _gic_cpu_addr
	mov	r1, #1
	str	r1, [r0]
	mov	r1, #0xFF
	str	r1, [r0, #4]
	ldr	r0, _sys_flags_addr
1:	
	/* Wait for interrupt before checking SYS_FLAGS */
	wfi	
	ldr	r1, [r0]
	teq	r1, #0
	/* Repeat if SYS_FLAGS == 0 */
	beq	1b
	/* Jump to the address stored the the SYS_FLAGS register */
	bx	r1

_gic_cpu_addr:
	.word	0x1e000100
_sys_flags_addr:
	.word 	0x10000030

_start_hang:
	b	.
_start_relocate:
	mov	r5, #0
_start_loop:
	cmp	r5, r4
	bge	_start_done
	ldmia	r0!, {r7 - r10}
	stmia	r2!, {r7 - r10}
	add	r5, r5, #16
	b	_start_loop
_start_done:
	ldr	pc, __reset
__code_start:
	.word _code_start
__code_end:
	.word _code_end

	.section .expvect, "ax", %progbits
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
__usr_stack_end:
	.word _usr_stack_end

	.globl _reset
_reset:
	/* Clear a register for temporary usage */
	mov	r8, #0
	/* Disable IRQ & FIQ */
	cpsid if
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
	/* Set System Mode Stack */
	SET_CURRENT_MODE CPSR_MODE_SYSTEM
	SET_CURRENT_STACK __usr_stack_end
	/* Set to Supervisor Mode */
	SET_CURRENT_MODE CPSR_MODE_SUPERVISOR
	/* Call init function */
	bl	arm_init
	/* Call main function */
	bl	arm_main
	/* We should never reach here */
	b	.
	
	.globl _switch_to_user_mode
_switch_to_user_mode:
	sub	r0, sp
	mov	r1, lr
	SET_CURRENT_MODE CPSR_MODE_USER
	mov	sp, r0
	mov	lr, r1
	bx	lr

START_EXCEPTION_HANDLER _undefined_instruction, 4
	PUSH_USER_REGS
	CALL_EXCEPTION_CFUNC do_undefined_instruction
	PULL_USER_REGS
END_EXCEPTION_HANDLER

START_EXCEPTION_HANDLER _software_interrupt, 4
	PUSH_USER_REGS
	CALL_EXCEPTION_CFUNC do_software_interrupt
	PULL_USER_REGS
END_EXCEPTION_HANDLER

START_EXCEPTION_HANDLER _prefetch_abort, 4
	PUSH_USER_REGS
	CALL_EXCEPTION_CFUNC do_prefetch_abort
	PULL_USER_REGS
END_EXCEPTION_HANDLER

START_EXCEPTION_HANDLER _data_abort, 8
	PUSH_USER_REGS
	CALL_EXCEPTION_CFUNC do_data_abort
	PULL_USER_REGS
END_EXCEPTION_HANDLER

START_EXCEPTION_HANDLER _not_used, 4
	PUSH_USER_REGS
	CALL_EXCEPTION_CFUNC do_not_used
	PULL_USER_REGS
END_EXCEPTION_HANDLER

START_EXCEPTION_HANDLER _irq, 4
	PUSH_USER_REGS
	CALL_EXCEPTION_CFUNC do_irq
	PULL_USER_REGS
END_EXCEPTION_HANDLER

START_EXCEPTION_HANDLER _fiq, 4
	PUSH_FIQUSER_REGS
	CALL_EXCEPTION_CFUNC do_fiq
	PULL_USER_REGS
END_EXCEPTION_HANDLER

