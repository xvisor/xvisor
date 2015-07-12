/**
 * Copyright (c) 2012 Ankit Jindal.
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
 * @file cpu_cache_v7.S
 * @author Ankit Jindal (thatsjindal@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Low-level implementation of cache ARMv7 functions 
 */

/*
 * dcache_line_size - get the minimum D-cache line size from the CTR register
 * on ARMv7.
 */
.macro	dcache_line_size, reg, tmp
	mrc	p15, 0, \tmp, c0, c0, 1		@ read ctr
	lsr	\tmp, \tmp, #16
	and	\tmp, \tmp, #0xf		@ cache line size encoding
	mov	\reg, #4			@ bytes per word
	mov	\reg, \reg, lsl \tmp		@ actual cache line size
.endm

/* 
 * Generic mechanism for operations on the entire data or unified cache to the Point
 * of Coherence. This code is taken from 'Example code for cache maintenance operations'
 * provided in "ARM Architecture Reference Manual ARMv7-A and ARMv7-R edition 
 * (ARM DDI 0406)". 
 * Registers r0 - r9 are used.
 */
#define ARM_ENTIRE_DCACHE_OP(crm) 	\
					\
        stmfd   sp!, {r0-r9}		/* save registers */;\
					\
	mrc 	p15, 1, r0, c0, c0, 1  	/* Read CLIDR */;\
	ands	r3, r0, #0x7000000 	;\
	mov 	r3, r3, LSR #23 	/* Cache level value (naturally aligned) */; \
	beq 	5f 			;\
	mov 	r8, #0 		;\
1: 					;\
	add 	r2, r8, r8, LSR #1 	/* Work out 3xcachelevel */; \
	mov 	r1, r0, LSR r2 		/* bottom 3 bits are the Cache type for this level */; \
	and 	r1, r1, #7		/* get those 3 bits alone */; \
	cmp 	r1, #2			;\
	blt 	4f 			/* no cache or only instruction cache at this level */; \
	mcr 	p15, 2, r8, c0, c0, 0 	/* write the Cache Size selection register */; \
	isb 				/* isb to sync the change to the CacheSizeID reg */; \
	mrc 	p15, 1, r1, c0, c0, 0 	/* reads current Cache Size ID register */; \
	and 	r2, r1, #7 		/* extract the line length field */; \
	add 	r2, r2, #4 		/* add 4 for the line length offset (log2 16 bytes) */; \
	ldr 	r4, =0x3FF		;\
	ands	r4, r4, r1, LSR #3 	/* r4 is the max number on the way size (right aligned) */; \
	clz 	r5, r4 			/* r5 is the bit position of the way size increment */; \
	ldr 	r6, =0x00007FFF		;\
	ands	r6, r6, r1, LSR #13 	/* r6 is the max number of the index size (right aligned) */; \
2: 					;\
	mov 	r7, r4 			/* r7 working copy of the max way size (right aligned) */; \
3:					;\
	orr 	r9, r8, r7, LSL r5 	/* factor in the way number and cache number into r9 */; \
	orr 	r9, r9, r6, LSL r2 	/* factor in the index number */; \
	mcr 	p15, 0, r9, c7, crm, 2 /* clean by set/way */; \
	subs	r7, r7, #1 		/* decrement the way number */; \
	bge 	3b			;\
	subs 	r6, r6, #1 		/* decrement the index */; \
	bge 	2b			;\
4:					;\
	add 	r8, r8, #2 		/* increment the cache number */; \
	cmp 	r3, r8			;\
	bgt 	1b			;\
					\
5:					;\
	ldmia   sp!, {r0-r9}		/* restore registers */;

	/* 
	 * Operations on entire data Cache to POC 
	 */

	/* invalidate the entire d-cache */
	.globl invalidate_dcache
invalidate_dcache:
	ARM_ENTIRE_DCACHE_OP(c6) 	/* invalidate all */
	isb
	bx	lr

	/* clean the entire data cache */	
	.globl clean_dcache
clean_dcache:
	ARM_ENTIRE_DCACHE_OP(c10) 	/* clean all */
	dsb
	isb
	bx	lr

	/* clean & invalidate the entire data cache */	
	.globl clean_invalidate_dcache
clean_invalidate_dcache:
	ARM_ENTIRE_DCACHE_OP(c14) 	/* clean and invalidate all */
	dsb
	isb
	bx	lr

	/* 
	 * Operations on data cache by MVA 
	 */

	/* invalidate by MVA */
	.globl invalidate_dcache_mva
invalidate_dcache_mva:
	mcr     p15, 0, r0, c7, c6, 1
	isb
	bx	lr

	/* Invalidate by memory region by mva range
	 *  r0 - start address of region
	 *  r1 - end address of region
	 */
	.globl invalidate_dcache_mva_range
invalidate_dcache_mva_range:
	push	{r0, r1, r2, r3}
	dcache_line_size r2, r3
	sub	r3, r2, #1
	tst	r0, r3
	bic	r0, r0, r3
	/* clean & invalidate D / U line */
	mcrne	p15, 0, r0, c7, c14, 1
	tst	r1, r3
	bic	r1, r1, r3
	/* clean & invalidate D / U line */
	mcrne	p15, 0, r1, c7, c14, 1
1:
	/* invalidate D / U line */
	mcr	p15, 0, r0, c7, c6, 1
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb	st
	pop	{r0, r1, r2, r3}
	bx	lr

	/* clean by mva */
	.globl clean_dcache_mva
clean_dcache_mva:
	mcr     p15, 0, r0, c7, c10, 1
	dsb
	isb
	bx	lr

	/* clean by memory region by mva range 
	 *  r0 - start address of region
	 *  r1 - end address of region
	 */
	.globl clean_dcache_mva_range
clean_dcache_mva_range:
	push	{r0, r1, r2, r3}
	dcache_line_size r2, r3
	sub	r3, r2, #1
	bic	r0, r0, r3
1:
	mcr     p15, 0, r0, c7, c10, 1
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb
	isb
	pop	{r0, r1, r2, r3}
	bx	lr

	/* clean and invalidate by mva */
	.globl clean_invalidate_dcache_mva
clean_invalidate_dcache_mva:
	mcr     p15, 0, r0, c7, c14, 1
	dsb
	isb
	bx	lr

	/* clean and invalidate a memory region by mva
	 *  r0 - start address of region
	 *  r1 - end address of region
	 */
	.globl clean_invalidate_dcache_mva_range
clean_invalidate_dcache_mva_range:
	push	{r0, r1, r2, r3}
	dcache_line_size r2, r3
	sub	r3, r2, #1
	bic	r0, r0, r3
1:
	mcr	p15, 0, r0, c7, c14, 1		/* clean & invalidate D / U line */
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb
	isb
	pop	{r0, r1, r2, r3}
	bx	lr

	/* 
	 * Operations on data cache line by set/way
	 */

	/* invalidate line by set/way */
	.globl invalidate_dcache_line
invalidate_dcache_line:
	mcr     p15, 0, r0, c7, c6, 2
	isb
	bx	lr
	
	/* clean line by set/way */
	.globl clean_dcache_line
clean_dcache_line:
	mcr     p15, 0, r0, c7, c10, 2
	dsb
	isb
	bx	lr

	/* clean and invalidate line by set/way */
	.globl clean_invalidate_dcache_line
clean_invalidate_dcache_line:
	mcr     p15, 0, r0, c7, c14, 2
	dsb
	isb
	bx	lr

	/* 
	 * Operation on entire Instruction cache 
	 */

	/* invalidate the entire i-cache */
	.globl invalidate_icache
invalidate_icache:
	push	{r0}
	mov	r0, #0
	mcr     p15, 0, r0, c7, c5, 0 	/* invalidate all */
	isb
	pop	{r0}
	bx	lr

	/* invalidate i-cache by mva */
	.globl invalidate_icache_mva
invalidate_icache_mva:
	mcr     p15, 0, r0, c7, c5, 0 	/* invalidate all */
	isb
	bx	lr

	/* invalidate the i-cache line by set/way */ 
	/* no such instruction so invalidate everything */
	.globl invalidate_icache_line
invalidate_icache_line:
	push	{r0}
	mov	r0, #0
	mcr     p15, 0, r0, c7, c5, 0 	/* invalidate all */
	isb
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
	push	{r0}
	mov	r0, #0
	mcr     p15, 0, r0, c7, c5, 6 	/* invalidate all */
	isb
	pop	{r0}
	bx	lr

	/* invalidate branch predictor by mva */
	.globl invalidate_bpredictor_mva
invalidate_bpredictor_mva:
	mcr     p15, 0, r0, c7, c5, 7 	/* invalidate all */
	isb
	bx	lr

