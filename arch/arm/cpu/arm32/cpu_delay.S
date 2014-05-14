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
	mov	r1, r0
	mov	r2, #3
	mul	r0, r1, r2
	pop	{r1, r2}
	mov	pc, lr

