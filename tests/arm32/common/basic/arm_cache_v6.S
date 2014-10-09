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
 * @file arm_cache_v6.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARMv6 cache operations
 */

#include <arm_asm_macro.h>

#define HARVARD_CACHE
#define CACHE_LINE_SIZE		32
#define D_CACHE_LINE_SIZE	32
#define BTB_FLUSH_SIZE		8

	/* clean the entire d-cache */	
	.globl arm_clean_dcache
arm_clean_dcache:
	push	{r0}
	mov	r0, #0
#ifdef HARVARD_CACHE
	mcr	p15, 0, r0, c7, c10, 0		@ d-cache clean
#else
	mcr	p15, 0, r0, c7, c11, 0		@ cache unified clean
#endif
	mcr	p15, 0, r0, c7, c10, 4
	mcr	p15, 0, r0, c7, c5, 4
	pop	{r0}
	mov	pc, lr

	/* clean & invalidate the entire d-cache */	
	.globl arm_clean_invalidate_dcache
arm_clean_invalidate_dcache:
	push	{r0}
	mov	r0, #0
#ifdef HARVARD_CACHE
	mcr	p15, 0, r0, c7, c14, 0		@ d-cache clean+invalidate
#else
	mcr	p15, 0, r0, c7, c15, 0		@ unified cache clean+invalidate
#endif
	mcr	p15, 0, r0, c7, c10, 4
	mcr	p15, 0, r0, c7, c5, 4
	pop	{r0}
	mov	pc, lr

	/* clean by memory region by mva range 
	 *  r0 - start address of region
	 *  r1 - end address of region
	 */
	.globl arm_clean_dcache_mva_range
arm_clean_dcache_mva_range:
	push	{r0, r1, r2}
#ifdef HARVARD_CACHE
	mov	r2, #D_CACHE_LINE_SIZE
	bic	r0, r0, #D_CACHE_LINE_SIZE - 1
1:
	mcr     p15, 0, r0, c7, c10, 1		@ d-cache clean by MVA
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
#else
	mov	r2, #CACHE_LINE_SIZE
	bic	r0, r0, #CACHE_LINE_SIZE - 1
1:
	mcr     p15, 0, r0, c7, c11, 1		@ cache unified clean by MVA
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
#endif
	mcr	p15, 0, r0, c7, c10, 4
	mcr	p15, 0, r0, c7, c5, 4
	pop	{r0, r1, r2}
	bx	lr

	/* clean and invalidate a memory region by mva
	 *  r0 - start address of region
	 *  r1 - end address of region
	 */
	.globl arm_clean_invalidate_dcache_mva_range
arm_clean_invalidate_dcache_mva_range:
	push	{r0, r1, r2}
#ifdef HARVARD_CACHE
	mov	r2, #D_CACHE_LINE_SIZE
	bic	r0, r0, #D_CACHE_LINE_SIZE - 1
1:
	mcr     p15, 0, r0, c7, c14, 1		@ d-cache clean+invalidate by MVA
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
#else
	mov	r2, #CACHE_LINE_SIZE
	bic	r0, r0, #CACHE_LINE_SIZE - 1
1:
	mcr     p15, 0, r0, c7, c15, 1		@ cache unified clean+invalidate by MVA
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
#endif
	mcr	p15, 0, r0, c7, c10, 4
	mcr	p15, 0, r0, c7, c5, 4
	pop	{r0, r1, r2}	
	bx	lr

	/* invalidate the entire i-cache  */
	.globl arm_invalidate_icache
arm_invalidate_icache:
	push	{r0, r1}
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0		@ invalidate i-cache
	mcr	p15, 0, r0, c7, c5, 4
	pop	{r0, r1}
	mov	pc, lr

