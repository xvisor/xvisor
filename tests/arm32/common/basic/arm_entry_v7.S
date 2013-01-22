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
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Entry point code for ARM test code
 */

#include <arm_asm_macro.h>
#include <gic_config.h>
#include <arm_plat.h>

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

	/* Core-1 should spin and core-0 should go ahead */
	mrc	p15, 0, r5, c0, c0, 5
	ands	r5, r5, #0x3
	bne	_secondary_loop

	ldr	r1, __code_start
	sub	r6, r0, r1		/* r1 -> Load Start - Exec Start */
	/*
	 * r6 -> offset between binary execution and load addresses
	 * We need to ensure that when we jump to reset code, we are executing
	 * from intended execution address. If necessary do relocation from
	 * load memory to execution memory.
	 */
	ldr	r1, __reloc_region_start	/* r1 -> execution address of reloc_region_start */
	ldr	r2, __reloc_region_end
	sub	r2, r2, r1		/* r2 -> reloc_region size */
	add	r0, r1, r6		/* r0 -> load address of reloc_region start */
	bl	_reloc_region

	/*
	 * Manually zero out the zero region (bss + heap)
	 */
	ldr	r1, __zero_region_start
	ldr	r2, __zero_region_end
	mov	r7, #0x0
	mov	r8, #0x0
	mov	r9, #0x0
	mov	r10, #0x0
_zeroing_loop:
	cmp	r1, r2
	bge	_zeroing_done
	stmia	r1!, {r7 - r10}
	b	_zeroing_loop
_zeroing_done:

_jump_to_exec:
	ldr	pc, __reset

__code_start:
	.word _code_start
__reloc_region_start:
	.word _reloc_region_start
__reloc_region_end:
	.word _reloc_region_end
__zero_region_start:
	.word _zero_region_start
__zero_region_end:
	.word _zero_region_end
__heap_start:
	.word _heap_start
__heap_end:
	.word _heap_end

	/* 
	 * Copies data from source to destination taking care of even
	 * overlapping regions
	 * Arguments:
	 *  r0 -> source address
	 *  r1 -> destination address
	 *  r2 -> byte count
	 * Unmodified gprs: r4, r5, r6, r11, r12
	 */
_reloc_region:
	mov	r3, #0
	cmp	r0, r1
	beq	_reloc_done
	blt	_rev_copy
_fwd_loop:
	cmp	r3, r2
	bge	_reloc_done
	ldmia	r0!, {r7 - r10}
	stmia	r1!, {r7 - r10}
	add	r3, r3, #16
	b	_fwd_loop
_rev_copy:
	add	r0, r0, r2
	add	r1, r1, r2
_rev_loop:
	cmp	r3, r2
	bge	_reloc_done
	ldmdb	r0!, {r7 - r10}
	stmdb	r1!, {r7 - r10}
	add	r3, r3, #16
	b	_rev_loop
_reloc_done:
	bx	lr


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
	.word	GIC_CPU_BASE
_sys_flags_addr:
	.word 	V2M_SYS_FLAGS

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

