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
	b	0f
	
1:	subs	r2, r2, #4		@ 1 do we have enough
	blt	5f			@ 1 bytes to align with?
	cmp	r3, #2			@ 1
	strltb	r1, [r0], #1		@ 1
	strleb	r1, [r0], #1		@ 1
	strb	r1, [r0], #1		@ 1
	add	r2, r2, r3		@ 1 (r2 = r2 - (4 - r3))
/*
 * The pointer is now aligned and the length is adjusted.  Try doing the
 * memset again.
 */
0:	ands	r3, r0, #3		@ 1 unaligned?
	bne	1b			@ 1
/*
 * we know that the pointer in r0 is aligned to a word boundary.
 */
	orr	r1, r1, r1, lsl #8
	orr	r1, r1, r1, lsl #16
	mov	r3, r1
	cmp	r2, #16
	blt	4f

/*
 * This version aligns the destination pointer in order to write
 * whole cache lines at once.
 */

	stmfd	sp!, {r4-r7, lr}
	mov	r4, r1
	mov	r5, r1
	mov	r6, r1
	mov	r7, r1
	mov	ip, r1
	mov	lr, r1

	cmp	r2, #96
	tstgt	r0, #31
	ble	3f

	and	ip, r0, #31
	rsb	ip, ip, #32
	sub	r2, r2, ip
	movs	ip, ip, lsl #(32 - 4)
	stmcsia	r0!, {r4, r5, r6, r7}
	stmmiia	r0!, {r4, r5}
	tst	ip, #(1 << 30)
	mov	ip, r1
	strne	r1, [r0], #4

3:	subs	r2, r2, #64
	stmgeia	r0!, {r1, r3-r7, ip, lr}
	stmgeia	r0!, {r1, r3-r7, ip, lr}
	bgt	3b
	ldmeqfd	sp!, {r4-r7, pc}

	tst	r2, #32
	stmneia	r0!, {r1, r3-r7, ip, lr}
	tst	r2, #16
	stmneia	r0!, {r4-r7}
	ldmfd	sp!, {r4-r7, lr}

4:	tst	r2, #8
	stmneia	r0!, {r1, r3}
	tst	r2, #4
	strne	r1, [r0], #4
/*
 * When we get here, we've got less than 4 bytes to zero.  We
 * may have an unaligned pointer as well.
 */
5:	tst	r2, #2
	strneb	r1, [r0], #1
	strneb	r1, [r0], #1
	tst	r2, #1
	strneb	r1, [r0], #1
	mov	pc, lr

