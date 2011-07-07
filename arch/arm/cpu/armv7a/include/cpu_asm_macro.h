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
 * @file cpu_asm_macros.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief  common assembly macros 
 */
#ifndef __CPU_ASM_MACRO_H__
#define __CPU_ASM_MACRO_H__

#include <cpu_defines.h>

#ifdef __ASSEMBLY__

.macro SET_CURRENT_FLAGS flags, treg
	mrs	\treg, cpsr
	orr	\treg, \treg, #(\flags)
	msr 	cpsr, \treg
.endm

.macro SET_CURRENT_MODE mode
	cps	#(\mode)
.endm

.macro SET_CURRENT_STACK new_stack
	ldr	sp, \new_stack
.endm

.macro START_EXCEPTION_HANDLER irqname, lroffset
	.align 5
\irqname:
	sub	lr, lr, #\lroffset
.endm

/* Save User Registers */
.macro PUSH_USER_REGS
	str     lr, [sp, #-4]!;         /* Push the return address */
	sub     sp, sp, #(4*15);        /* Adjust the stack pointer */
	stmia   sp, {r0-r12};           /* Push user mode registers */
	add     r0, sp, #(4*13);        /* Adjust the stack pointer */
	stmia   r0, {r13-r14}^;         /* Push user mode registers */
	mov     r0, r0;                 /* NOP for previous inst */
	mrs     r0, spsr_all;           /* Put the SPSR on the stack */
	str     r0, [sp, #-4]!
.endm

/* If came from priviledged mode then push banked registers */
.macro PUSH_BANKED_REGS skip_lable
	mov	r4, r0
	and	r0, r0, #CPSR_MODE_MASK
	cmp	r0, #CPSR_MODE_USER
	beq	\skip_lable
	add	r1, sp, #(4*14)
	mrs	r5, cpsr
	orr	r4, r4, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED)
	msr	cpsr, r4
	str	sp, [r1, #0]
	str	lr, [r1, #4]
	msr	cpsr, r5
	\skip_lable:
.endm

/* Call C function to handle exception */
.macro CALL_EXCEPTION_CFUNC cfunc
	mov	r0, sp
	bl	\cfunc
.endm

/* If going back to priviledged mode then pull banked registers */
.macro PULL_BANKED_REGS skip_lable
	ldr     r0, [sp, #0]
	mov	r4, r0
	and	r0, r0, #CPSR_MODE_MASK
	cmp	r0, #CPSR_MODE_USER
	beq	\skip_lable
	add	r1, sp, #(4*14)
	mrs	r5, cpsr
	orr	r4, r4, #(CPSR_IRQ_DISABLED | CPSR_FIQ_DISABLED)
	msr	cpsr, r4
	ldr	sp, [r1, #0]
	ldr	lr, [r1, #4]
	msr	cpsr, r5
	\skip_lable:
.endm

/* Restore User Registers */
.macro PULL_USER_REGS
	ldr     r0, [sp], #0x0004;      /* Get SPSR from stack */
	msr     spsr_all, r0;
	ldmia   sp, {r0-r14}^;          /* Restore registers (user) */
	mov     r0, r0;                 /* NOP for previous isnt */
	add     sp, sp, #(4*15);        /* Adjust the stack pointer */
	ldr     lr, [sp], #0x0004       /* Pull return address */
.endm

.macro END_EXCEPTION_HANDLER
	movs	pc, lr
.endm

#endif

#endif
