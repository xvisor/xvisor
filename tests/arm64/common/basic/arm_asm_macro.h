/**
 * Copyright (c) 2010 Sukanto Ghosh.
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
 * @file arm_asm_macros.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief  common assembly macros for ARM test code
 */
#ifndef __ARM_ASM_MACRO_H__
#define __ARM_ASM_MACRO_H__

#include <arm_defines.h>

#ifdef __ASSEMBLY__

/*
 * Register aliases.
 */
lr	.req	x30		/* link register */

#define ENTRY(name) \
  .globl name; \
  .align 4; \
  name:

#define END(name) \
  .size name, .-name


/* Stack pushing/popping (register pairs only). 
   Equivalent to store decrement before, load increment after */

.macro  push, xreg1, xreg2
	stp     \xreg1, \xreg2, [sp, #-16]!
.endm

.macro  pop, xreg1, xreg2
	ldp     \xreg1, \xreg2, [sp], #16
.endm


.macro EXCEPTION_HANDLER irqname
	.align 6
\irqname:
.endm


/* Push registers on stack */
.macro	PUSH_REGS
	sub	sp, sp, #0x20		/* room for LR, SP, SPSR, ELR */
	push	x28, x29
	push	x26, x27
	push	x24, x25
	push	x22, x23
	push	x20, x21
	push	x18, x19
	push	x16, x17
	push	x14, x15
	push	x12, x13
	push	x10, x11
	push	x8, x9
	push	x6, x7
	push	x4, x5
	push	x2, x3
	push	x0, x1
	add	x21, sp, #0x110
	mrs	x22, elr_el1
	mrs	x23, spsr_el1
	stp	lr, x21, [sp, #0xF0]
	stp	x22, x23, [sp, #0x100]
	/*
	 * Registers that may be useful after this macro is invoked:
	 *
	 * x21 - aborted SP
	 * x22 - aborted PC
	 * x23 - aborted PSTATE
	 */
.endm

/* Call C function to handle exception */
.macro CALL_EXCEPTION_CFUNC cfunc
	mov	x0, sp
	bl	\cfunc
.endm

/* Pull registers from stack */
.macro	PULL_REGS
	ldp	x21, x22, [sp, #0x100]		/* load ELR, SPSR */
	msr	elr_el1, x21
	msr	spsr_el1, x22
	pop	x0, x1
	pop	x2, x3				
	pop	x4, x5
	pop	x6, x7
	pop	x8, x9
	pop	x10, x11
	pop	x12, x13
	pop	x14, x15
	pop	x16, x17
	pop	x18, x19
	pop	x20, x21
	pop	x22, x23
	pop	x24, x25
	pop	x26, x27
	pop	x28, x29
	msr	tpidr_el1, x24
	ldp	lr, x24, [sp]
	mov	sp, x24
	mrs	x24, tpidr_el1
	eret
.endm

/*
 * Exception vectors.
 */
.macro	ventry	label
	.align	7
	b	\label
.endm



#endif /* __ASSEMBLY__ */

#endif
