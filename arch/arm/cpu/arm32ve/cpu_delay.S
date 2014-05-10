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
 * @file cpu_delay.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief Low-level implementation of delay functions 
 */

	/* 
	 * arch specific delay loop
	 */
	.globl arch_delay_loop
arch_delay_loop:
	subs	r0, r0, #1
	bhi	arch_delay_loop
	mov	pc, lr

	/*
	 * arch specific machine cycle count from delay loop count
	 */
	.globl arch_delay_loop_cycles
arch_delay_loop_cycles:
	push	{r1, r2}

	mrc	p15, 0, r1, c0, c0, 0		@ Read Main ID register
	and	r2, r1, #0xff000000		@ ARM?
	teq	r2, #0x41000000
	bne	arm_cpu_skip
	ubfx	r1, r1, #4, #12			@ primary part number

	/* Machine cycle count for:
	 *   Cortex-A7
	 * Calulation:
	 *   Machine cycle count = Delay loop count
	 */
	ldr	r2, =0x00000c07			@ Cortex-A7 primary part number
	teq	r1, r2
	bne	arm_cpu_skip
arm_cpu_1x:
	mov	r0, r0
	b	arch_delay_loop_cycles_done
arm_cpu_skip:

	/* Machine cycle count for:
	 *   Unknown CPUs
	 * Calulation:
	 *   Machine cycle count = Delay loop count * 3
	 */
unknown_cpu:
	mov	r1, #3
	mul	r0, r0, r1

arch_delay_loop_cycles_done:
	pop	{r1, r2}
	mov	pc, lr

