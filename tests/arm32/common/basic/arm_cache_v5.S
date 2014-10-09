/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file arm_cache_v5.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARMv5 cache operations
 */

#include <arm_asm_macro.h>

	/* clean the entire data cache */	
	.globl arm_clean_dcache
arm_clean_dcache:
	push	{r0}
	/* FIXME: */
	mcr	p15, 0, r0, c7, c10, 4
	pop	{r0}
	bx	lr

	/* clean & invalidate the entire data cache */	
	.globl arm_clean_invalidate_dcache
arm_clean_invalidate_dcache:
	push	{r0}
	/* FIXME: */
	mcr	p15, 0, r0, c7, c10, 4
	pop	{r0}
	bx	lr

	/* clean by memory region by mva range 
	 *  r0 - start address of region
	 *  r1 - end address of region
	 */
	.globl arm_clean_dcache_mva_range
arm_clean_dcache_mva_range:
	/* FIXME: */
	mcr	p15, 0, r0, c7, c10, 4
	bx	lr

	/* clean and invalidate a memory region by mva
	 *  r0 - start address of region
	 *  r1 - end address of region
	 */
	.globl arm_clean_invalidate_dcache_mva_range
arm_clean_invalidate_dcache_mva_range:
	/* FIXME: */
	mcr	p15, 0, r0, c7, c10, 4
	bx	lr

	/* invalidate the entire i-cache */
	.globl arm_invalidate_icache
arm_invalidate_icache:
	push	{r0}
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0
	pop	{r0}
	bx	lr

