/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Low-level implementation of delay functions 
 */

	/* 
	 * arch specific delay loop
	 */
	.globl arch_delay_loop
arch_delay_loop:
	subs	x0, x0, #1
	b.hi	arch_delay_loop
	ret

	/*
	 * arch specific machine cycle count from delay loop count
	 */
	.globl arch_delay_loop_cycles
arch_delay_loop_cycles:
	stp     x1, x2, [sp, #-16]!
	mov	x1, x0
	mov	x2, #3
	mul	x0, x1, x2
	ldp     x1, x2, [sp], #16
	ret

