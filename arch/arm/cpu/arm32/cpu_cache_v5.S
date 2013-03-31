/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file cpu_cache_v5.S
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Low-level implementation of cache ARMv5 functions 
 */

	/* 
	 * Operations on entire data Cache to POC 
	 */

	/* invalidate the entire d-cache */
	.globl invalidate_dcache
invalidate_dcache:
	/* FIXME: */
	bx	lr

	/* clean the entire data cache */	
	.globl clean_dcache
clean_dcache:
	push	{r0}
	/* FIXME: */
	mcr	p15, 0, r0, c7, c10, 4
	pop	{r0}
	bx	lr

	/* clean & invalidate the entire data cache */	
	.globl clean_invalidate_dcache
clean_invalidate_dcache:
	push	{r0}
	/* FIXME: */
	mcr	p15, 0, r0, c7, c10, 4
	pop	{r0}
	bx	lr

	/* 
	 * Operations on data cache by MVA 
	 */

	/* invalidate by MVA */
	.globl invalidate_dcache_mva
invalidate_dcache_mva:
	mcr     p15, 0, r0, c7, c6, 1
	bx	lr
	
	/* clean by mva */
	.globl clean_dcache_mva
clean_dcache_mva:
	mcr     p15, 0, r0, c7, c10, 1
	mcr	p15, 0, r0, c7, c10, 4
	bx	lr

	/* clean by memory region by mva range 
	 *  r0 - start address of region
	 *  r1 - end address of region
	 */
	.globl clean_dcache_mva_range
clean_dcache_mva_range:
	/* FIXME: */
	mcr	p15, 0, r0, c7, c10, 4
	bx	lr

	/* clean and invalidate by mva */
	.globl clean_invalidate_dcache_mva
clean_invalidate_dcache_mva:
	mcr	p15, 0, r0, c7, c14, 1
	mcr	p15, 0, r0, c7, c10, 4
	bx	lr

	/* clean and invalidate a memory region by mva
	 *  r0 - start address of region
	 *  r1 - end address of region
	 */
	.globl clean_invalidate_dcache_mva_range
clean_invalidate_dcache_mva_range:
	/* FIXME: */
	mcr	p15, 0, r0, c7, c10, 4
	bx	lr

	/* 
	 * Operations on data cache line by set/way
	 */

	/* invalidate line by set/way */
	.globl invalidate_dcache_line
invalidate_dcache_line:
	mcr     p15, 0, r0, c7, c6, 2
	bx	lr
	
	/* clean line by set/way */
	.globl clean_dcache_line
clean_dcache_line:
	mcr     p15, 0, r0, c7, c10, 2
	mcr	p15, 0, r0, c7, c10, 4
	bx	lr

	/* clean and invalidate line by set/way */
	.globl clean_invalidate_dcache_line
clean_invalidate_dcache_line:
	mcr     p15, 0, r0, c7, c14, 2
	mcr	p15, 0, r0, c7, c10, 4
	bx	lr

	/* 
	 * Operation on entire Instruction cache 
	 */

	/* invalidate the entire i-cache */
	.globl invalidate_icache
invalidate_icache:
	push	{r0}
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0
	pop	{r0}
	bx	lr

	/* invalidate i-cache by mva */
	.globl invalidate_icache_mva
invalidate_icache_mva:
	mcr     p15, 0, r0, c7, c5, 0
	bx	lr

	/* invalidate the i-cache line by set/way */ 
	/* no such instruction so invalidate everything */
	.globl invalidate_icache_line
invalidate_icache_line:
	push	{r0}
	mov	r0, #0
	mcr     p15, 0, r0, c7, c5, 0 	/* invalidate all */
	pop	{r0}
	bx	lr

	/* 
	 * Operations on entire instruction an data cache 
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
	bl	invalidate_icache
	bl	clean_invalidate_dcache
	pop	{lr}
	bx	lr

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
	nop
	bx	lr

	/* invalidate branch predictor by mva */
	.globl invalidate_bpredictor_mva
invalidate_bpredictor_mva:
	nop
	bx	lr

