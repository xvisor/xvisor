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

#include <cpu_defines.h>
#include <cpu_asm_macro.h>

	/* 
	 * Xvisor could be loaded any where in memory by boot loaders.
	 * The _start ensures that Xvisor exectues from intended base
	 * address provided at compile time.
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
	 * Hang if execution start is not 1 MB aligned
	 */
	mov	r0, r6
	bfc	r0, #0, #20
	cmp	r0, r6
	blt	_start_hang
	/*
	 * Hang if execution end is not 1 MB aligned
	 */
	mov	r0, r7
	bfc	r0, #0, #20
	cmp	r0, r7
	blt	_start_hang
	/*
	 * Relocate code if load start is not 1 MB aligned
	 */
	mov	r0, r4
	bfc	r0, #0, #20
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
	bfc	r1, #0, #20
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
	bfc	r0, #0, #20
	subs	r1, r4, r0
	subs	r4, r4, r1
	subs	r5, r5, r1
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
	 * Clear default Level1 Table 
	 */
	/* r0 -> default Level1 Table base address */
	ldr	r0, __defl1_mem
	sub	r0, r0, r6
	add	r0, r0, r4
	/* r1 -> size of Level1 Table */
	mov	r1, #1
	lsl	r1, #14
	/* r2 -> zero value to store */
	mov	r2, #0
_start_mmu_defl1_clear:
	str	r2, [r0]
	add	r0, r0, #4
	sub	r1, r1, #4
	cmp	r1, #0
	bgt	_start_mmu_defl1_clear
	/*
	 * Create entries in default Level1 Table
	 * for execution & load addresses
	 */
	/* r0 -> default Level1 Table base address */
	ldr	r0, __defl1_mem
	sub	r0, r0, r6
	add	r0, r0, r4
	/* r3 -> entry attributes */
	ldr	r3, __mmu_section_attr
	/* r1 -> load entry start address */
	mov	r1, r4
	lsr	r1, #18 /* r1 >> 20, r1 << 2 */
	add	r1, r0, r1
	/* Setup load entry */	
	orr	r2, r3, r4
	str	r2, [r1]
	/* r1 -> execute entry start address */
	mov	r1, r6
	lsr	r1, #18 /* r1 >> 20, r1 << 2 */
	add	r1, r0, r1
	/* r2 -> execute entry end address */
	sub	r2, r7, r6
	lsr	r2, #18 /* r2 >> 20, r2 << 2 */
	add	r2, r1, r2
	/* r5 -> tempory value */
	mov	r5, #0
_start_mmu_defl1_set:
	orr	r5, r3, r4
	str	r5, [r1]
	lsr	r4, #20
	add	r4, r4, #1
	lsl	r4, #20
	add	r1, r1, #4
	cmp	r1, r2
	blt	_start_mmu_defl1_set
	/*
	 * Setup Translation Table Base Register 0
	 */
	mcr     p15, 0, r0, c2, c0, 0
	/*
	 * Setup Domain Control Register
	 */
	ldr	r1, __dacr_mmu_val
	mcr	p15, 0, r1, c3, c0, 0
	/*
	 * Setup System Control Register
	 */
	ldr	r1, __sctrl_mmu_val
	mrc     p15, 0, r2, c1, c0, 0
	orr	r2, r2, r1
	mcr     p15, 0, r2, c1, c0, 0
	/*
	 * Jump to reset code
	 */
	ldr	pc, __reset
_start_hang:
	b	.
#define SECTION_ATTR 	((TTBL_AP_SRW_U << TTBL_L1TBL_TTE_AP_SHIFT) | \
			(TTBL_L1TBL_TTE_DOM_RESERVED << TTBL_L1TBL_TTE_DOM_SHIFT) | \
			(0x1 << TTBL_L1TBL_TTE_C_SHIFT) | \
			TTBL_L1TBL_TTE_TYPE_SECTION)
__mmu_section_attr:
	.word SECTION_ATTR
__dacr_mmu_val:
	.word (TTBL_DOM_CLIENT << (2 * TTBL_L1TBL_TTE_DOM_RESERVED))
__sctrl_mmu_val:
	.word (SCTLR_M_MASK | SCTLR_C_MASK | SCTLR_I_MASK)
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
__defl1_mem:
	.word defl1_mem

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
START_EXCEPTION_HANDLER _undef_inst, 4
	PUSH_REGS _undef_inst_pushskip
	CALL_EXCEPTION_CFUNC do_undefined_instruction
	PULL_REGS _undef_inst_pullskip _undef_inst_pullskip1 _undef_inst_pullskip2
END_EXCEPTION_HANDLER

	/* 
	 * Software interrupt exception handler.
	 */
START_EXCEPTION_HANDLER _soft_irq, 4
	PUSH_REGS _soft_irq_pushskip
	CALL_EXCEPTION_CFUNC do_software_interrupt
	PULL_REGS _soft_irq_pullskip _soft_irq_pullskip1 _soft_irq_pullskip2
END_EXCEPTION_HANDLER

	/* 
	 * Prefetch abort exception handler.
	 */
START_EXCEPTION_HANDLER _prefetch_abort, 4
	PUSH_REGS _prefetch_abort_pushskip
	CALL_EXCEPTION_CFUNC do_prefetch_abort
	PULL_REGS _prefetch_abort_pullskip _prefetch_abort_pullskip1 _prefetch_abort_pullskip2
END_EXCEPTION_HANDLER

	/* 
	 * Data abort exception handler.
	 */
START_EXCEPTION_HANDLER _data_abort, 8
	PUSH_REGS _data_abort_pushskip
	CALL_EXCEPTION_CFUNC do_data_abort
	PULL_REGS _data_abort_pullskip _data_abort_pullskip1 _data_abort_pullskip2
END_EXCEPTION_HANDLER

	/* 
	 * Not used exception handler.
	 */
START_EXCEPTION_HANDLER _not_used, 4
	PUSH_REGS _not_used_pushskip
	CALL_EXCEPTION_CFUNC do_not_used
	PULL_REGS _not_used_pullskip _not_used_pullskip1 _not_used_pullskip2
END_EXCEPTION_HANDLER

	/* 
	 * IRQ exception handler.
	 */
START_EXCEPTION_HANDLER _irq, 4
	PUSH_REGS _irq_pushskip
	CALL_EXCEPTION_CFUNC do_irq
	PULL_REGS _irq_pullskip _irq_pullskip1 _irq_pullskip2
END_EXCEPTION_HANDLER

	/* 
	 * FIQ exception handler.
	 */
START_EXCEPTION_HANDLER _fiq, 4
	PUSH_REGS _fiq_pushskip
	CALL_EXCEPTION_CFUNC do_fiq
	PULL_REGS _fiq_pullskip _fiq_pullskip1 _fiq_pullskip2
END_EXCEPTION_HANDLER

