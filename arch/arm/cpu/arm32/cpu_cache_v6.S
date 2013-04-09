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
 * @file cpu_cache_v6.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief Low-level implementation of cache ARMv6 functions 
 */

#define HARVARD_CACHE
#define CACHE_LINE_SIZE		32
#define D_CACHE_LINE_SIZE	32
#define BTB_FLUSH_SIZE		8

.macro	ISB treg
	mcr	p15, 0, \treg, c7, c5, 4
.endm

.macro	DSB treg
	mcr	p15, 0, \treg, c7, c10, 4
.endm

	/* 
	 * Operations on entire d-cache
	 */

	/* invalidate the entire d-cache */
	.globl invalidate_dcache
invalidate_dcache:
	push	{r0}
	mov	r0, #0
#ifdef HARVARD_CACHE
	mcr	p15, 0, r0, c7, c6, 0		@ d-cache invalidate
#else
	mcr	p15, 0, r0, c7, c7, 0		@ cache unified invalidate
#endif
	ISB	r0
	pop	{r0}
	bx	lr

	/* clean the entire d-cache */	
	.globl clean_dcache
clean_dcache:
	push	{r0}
	mov	r0, #0
#ifdef HARVARD_CACHE
	mcr	p15, 0, r0, c7, c10, 0		@ d-cache clean
#else
	mcr	p15, 0, r0, c7, c11, 0		@ cache unified clean
#endif
	DSB	r0
	ISB	r0
	pop	{r0}
	mov	pc, lr

	/* clean & invalidate the entire d-cache */	
	.globl clean_invalidate_dcache
clean_invalidate_dcache:
	push	{r0}
	mov	r0, #0
#ifdef HARVARD_CACHE
	mcr	p15, 0, r0, c7, c14, 0		@ d-cache clean+invalidate
#else
	mcr	p15, 0, r0, c7, c15, 0		@ unified cache clean+invalidate
#endif
	DSB	r0
	ISB	r0
	pop	{r0}
	mov	pc, lr

	/* 
	 * Operations on d-cache by MVA 
	 */

	/* invalidate by MVA */
	.globl invalidate_dcache_mva
invalidate_dcache_mva:
#ifdef HARVARD_CACHE
	mcr     p15, 0, r0, c7, c6, 1		@ d-cache invalidate by MVA
#else
	mcr     p15, 0, r0, c7, c7, 1		@ cache unified invalidate by MVA
#endif
	ISB	r0
	mov	pc, lr
	
	/* clean by mva */
	.globl clean_dcache_mva
clean_dcache_mva:
#ifdef HARVARD_CACHE
	mcr     p15, 0, r0, c7, c10, 1		@ d-cache clean by MVA
#else
	mcr     p15, 0, r0, c7, c11, 1		@ cache unified clean by MVA
#endif
	DSB	r0
	ISB	r0
	mov	pc, lr

	/* clean by memory region by mva range 
	 *  r0 - start address of region
	 *  r1 - end address of region
	 */
	.globl clean_dcache_mva_range
clean_dcache_mva_range:
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
	DSB	r0
	ISB	r0
	pop	{r0, r1, r2}
	bx	lr

	/* clean and invalidate by mva */
	.globl clean_invalidate_dcache_mva
clean_invalidate_dcache_mva:
#ifdef HARVARD_CACHE
	mcr     p15, 0, r0, c7, c14, 1		@ d-cache clean+invalidate by MVA
#else
	mcr     p15, 0, r0, c7, c15, 1		@ cache unified clean+invalidate by MVA
#endif
	DSB	r0
	ISB	r0
	mov	pc, lr

	/* clean and invalidate a memory region by mva
	 *  r0 - start address of region
	 *  r1 - end address of region
	 */
	.globl clean_invalidate_dcache_mva_range
clean_invalidate_dcache_mva_range:
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
	DSB	r0
	ISB	r0
	pop	{r0, r1, r2}	
	bx	lr

	/* 
	 * Operations on data cache line by set/way
	 */

	/* invalidate line by set/way */
	.globl invalidate_dcache_line
invalidate_dcache_line:
#ifdef HARVARD_CACHE
	mcr     p15, 0, r0, c7, c6, 2		@ d-cache invalidate by set/way
#else
	mcr     p15, 0, r0, c7, c7, 2		@ cache unified invalidate by set/way
#endif
	ISB	r0
	mov	pc, lr

	/* clean line by set/way */
	.globl clean_dcache_line
clean_dcache_line:
#ifdef HARVARD_CACHE
	mcr     p15, 0, r0, c7, c10, 2		@ d-cache clean by set/way
#else
	mcr     p15, 0, r0, c7, c11, 2		@ cache unified clean by set/way
#endif
	DSB	r0
	ISB	r0
	mov	pc, lr

	/* clean and invalidate line by set/way */
	.globl clean_invalidate_dcache_line
clean_invalidate_dcache_line:
#ifdef HARVARD_CACHE
	mcr     p15, 0, r0, c7, c14, 1		@ d-cache clean+invalidate by set/way
#else
	mcr     p15, 0, r0, c7, c15, 2		@ cache unified clean+invalidate by set/way
#endif
	DSB	r0
	ISB	r0
	mov	pc, lr

	/* 
	 * Operation on entire Instruction cache 
	 */

	/* invalidate the entire i-cache 
	 *
	 * ARM1136 erratum 411920 - Invalidate Instruction Cache operation 
	 * can fail. This erratum is present in 1136, 1156 and 1176. 
	 * It does not affect the MPCore.
	 *
	 * Registers:
	 * r0 - set to 0
	 * r1 - corrupted
	 */
	.globl invalidate_icache
invalidate_icache:
	push	{r0, r1}
	mov	r0, #0
#ifdef CONFIG_ARM_ERRATA_411920
	mrs	r1, cpsr
	cpsid	ifa				@ disable interrupts
	mcr	p15, 0, r0, c7, c5, 0		@ invalidate entire i-cache
	mcr	p15, 0, r0, c7, c5, 0		@ invalidate entire i-cache
	mcr	p15, 0, r0, c7, c5, 0		@ invalidate entire i-cache
	mcr	p15, 0, r0, c7, c5, 0		@ invalidate entire i-cache
	msr	cpsr_cx, r1			@ restore interrupts
	.rept	11				@ ARM Ltd recommends at least
	nop					@ 11 NOPs
	.endr
#else
	mcr	p15, 0, r0, c7, c5, 0		@ invalidate i-cache
#endif
	ISB	r0
	pop	{r0, r1}
	mov	pc, lr

	/* invalidate i-cache by mva */
	.globl invalidate_icache_mva
invalidate_icache_mva:
	mcr     p15, 0, r0, c7, c5, 0		@ i-cache invalidate by MVA
	ISB	r0
	mov	pc, lr

	/* invalidate the i-cache line by set/way */ 
	.globl invalidate_icache_line
invalidate_icache_line:
	mcr     p15, 0, r0, c7, c5, 2		@ i-cache invalidate by set/way
	ISB	r0
	mov	pc, lr

	/* 
	 * Operations on entire instruction and data cache 
	 */

	/* invalidate the entire i-cache and d-cache */
	.globl invalidate_idcache
invalidate_idcache:
        push	{lr}	
	bl	invalidate_icache
	bl	invalidate_dcache
	pop	{lr}
	bx	lr

	/* clean the entire i-cache and d-cache */
	.globl clean_idcache
clean_idcache:
	push	{lr}	
	bl	clean_dcache
	pop	{lr}
	bx	lr

	/* clean and invalidate the entire i-cache and d-cache */
	.globl clean_invalidate_idcache
clean_invalidate_idcache:
	push	{lr}
	bl	clean_invalidate_dcache
	bl	invalidate_icache
	pop	{lr}
	mov	pc, lr

	/* 
	 * operation on both i-cache and d-cache by mva
	 */

	/* invalidate both i-cache and d-cache by mva */
	.globl invalidate_idcache_mva
invalidate_idcache_mva:
	push	{lr}
	bl	invalidate_icache_mva
	bl	invalidate_dcache_mva
	pop	{lr}
	bx	lr

	/* clean both i-cache and d-cache by mva */
	.globl clean_idcache_mva
clean_idcache_mva:
	push	{lr}
	bl	clean_dcache_mva
	pop	{lr}
	bx	lr

	/* clean and invalidate both i-cache and d-cache by mva */
	.globl clean_invalidate_idcache_mva
clean_invalidate_idcache_mva:
	push	{lr}
	bl	invalidate_icache_mva
	bl	clean_invalidate_dcache_mva
	pop	{lr}
	bx	lr

	/* 
	 * operation on both i-cache and d-cache line by set/way
	 */

	/* invalidate both i-cache and d-cache line by set/way */
	.globl invalidate_idcache_line
invalidate_idcache_line:
	push	{lr}
	bl	invalidate_icache_line
	bl	invalidate_dcache_line
	pop	{lr}
	bx	lr

	/* clean both i-cache and d-cache line by set/way */
	.globl clean_idcache_line
clean_idcache_line:
	push	{lr}
	bl	clean_dcache_line
	pop	{lr}
	bx	lr

	/* clean and invalidate both i-cache and d-cache line by set/way */
	.globl clean_invalidate_idcache_line
clean_invalidate_idcache_line:
	push	{lr}
	bl	invalidate_icache
	bl	clean_invalidate_dcache_line
	pop	{lr}
	bx	lr

	/* 
	 * branch predictor maintenence operation 
	 */

	/* invalidate entire branch predictor */
	.globl invalidate_bpredictor
invalidate_bpredictor:
	push	{r0}
	mov	r0, #0
	mcr     p15, 0, r0, c7, c5, 6 	/* invalidate branch predictor all */
	ISB	r0
	pop	{r0}
	bx	lr

	/* invalidate branch predictor by mva */
	.globl invalidate_bpredictor_mva
invalidate_bpredictor_mva:
	mcr     p15, 0, r0, c7, c5, 7 	/* invalidate branch predictor by MVA */
	ISB	r0
	bx	lr

