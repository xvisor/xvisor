/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file cpu_string.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief Low-level implementation of string functions 
 */

	.text
	.align	5
	.word	0
	.globl memcpy
memcpy:
	push	{r0, r3, r4, r5, r6, lr}
	orr	r4, r0, r1

	ands	r3, r4, #15
	beq	memcpy_aligned16

	ands	r3, r4, #7
	beq	memcpy_aligned8

	ands	r3, r4, #3
	beq	memcpy_aligned4

	b	memcpy_unaligned

memcpy_aligned16:
3:	cmp	r2, #16
	blt	memcpy_aligned8

	ldmia	r1!, {r3, r4, r5, r6}
	stmia	r0!, {r3, r4, r5, r6}
	sub	r2, r2, #16
	b	3b

memcpy_aligned8:
2:	cmp	r2, #8
	blt	memcpy_aligned4

	ldmia	r1!, {r3, r4}
	stmia	r0!, {r3, r4}
	sub	r2, r2, #8
	b	2b

memcpy_aligned4:
1:	cmp	r2, #4
	blt	memcpy_unaligned

	ldr	r3, [r1], #4
	str	r3, [r0], #4
	sub	r2, r2, #4
	b	1b

memcpy_unaligned:
0:	cmp	r2, #0
	beq	memcpy_done

	ldrb	r3, [r1], #1
	strb	r3, [r0], #1
	sub	r2, r2, #1
	b	0b

memcpy_done:
	pop	{r0, r3, r4, r5, r6, lr}
	mov	pc, lr



	.text
	.align	5
	.word	0
	.globl memset
memset:
	push	{r0, r3, r4, r5, r6, r7, lr}

	mov	r3, r1
	lsl	r3, #8
	orr	r3, r3, r1
	lsl	r3, #8
	orr	r3, r3, r1
	lsl	r3, #8
	orr	r3, r3, r1
	mov	r4, r3
	mov	r5, r3
	mov	r6, r3

	ands	r7, r0, #15
	beq	memset_aligned16

	ands	r7, r0, #7
	beq	memset_aligned8

	ands	r7, r0, #3
	beq	memset_aligned4

	b	memset_unaligned

memset_aligned16:
3:	cmp	r2, #16
	blt	memset_aligned8

	stmia	r0!, {r3, r4, r5, r6}
	sub	r2, r2, #16
	b	3b

memset_aligned8:
2:	cmp	r2, #8
	blt	memset_aligned4

	stmia	r0!, {r3, r4}
	sub	r2, r2, #8
	b	2b

memset_aligned4:
1:	cmp	r2, #4
	blt	memset_unaligned

	str	r3, [r0], #4
	sub	r2, r2, #4
	b	1b

memset_unaligned:
0:	cmp	r2, #0
	beq	memset_done

	strb	r1, [r0], #1
	sub	r2, r2, #1
	b	0b

memset_done:
	pop	{r0, r3, r4, r5, r6, r7, lr}
	mov	pc, lr

