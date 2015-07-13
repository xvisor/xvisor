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
 * @file cpu_cache.S
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Low-level implementation of cache ARMv8 functions
 */

/* Stack pushing/popping (register pairs only).
   Equivalent to store decrement before, load increment after */

.macro  push, xreg1, xreg2
	stp     \xreg1, \xreg2, [sp, #-16]!
.endm

.macro  pop, xreg1, xreg2
	ldp     \xreg1, \xreg2, [sp], #16
.endm

#define	lr	x30

/*
 * dcache_line_size - get the minimum D-cache line size from the CTR register
 * on ARMv8.
 */
.macro	dcache_line_size, reg, tmp
	mrs	\tmp, ctr_el0			/* read ctr_el0 */
	lsr	\tmp, \tmp, #16
	and	\tmp, \tmp, #0xf		/* cache line size encoding */
	mov	\reg, #4			/* bytes per word */
	lsl	\reg, \reg, \tmp		/* actual cache line size */
.endm

/*
 * Registers x0 - x10 are used.
 */
#define ARM_ENTIRE_DCACHE_OP(cacheop) 	\
					\
	push	x0, x1			/* save registers */;\
	push	x2, x3			/* save registers */;\
	push	x4, x5			/* save registers */;\
	push	x6, x7			/* save registers */;\
	push	x8, x9			/* save registers */;\
	push	x10, x11		/* save registers */;\
	dsb	sy			/* ensure ordering with previous memory accesses */;\
					\
	mrs 	x0, clidr_el1	  	/* Read CLIDR */;\
	ands	x3, x0, #0x7000000 	;\
	lsr 	x3, x3, #23 		/* Cache level value (naturally aligned) */; \
	cbz 	x3, 5f 			;\
	mov 	x8, #0 			;\
1: 					;\
	add 	x2, x8, x8, lsr #1 	/* Work out 3xcachelevel */; \
	lsr 	x1, x0, x2 		/* bottom 3 bits are the Cache type for this level */; \
	and 	x1, x1, #7		/* get those 3 bits alone */; \
	cmp 	x1, #2			;\
	blt 	4f 			/* no cache or only instruction cache at this level */; \
	msr 	csselr_el1, x8	 	/* write the Cache Size selection register */; \
	isb 				/* isb to sync the change to the CacheSizeID reg */; \
	mrs 	x1, ccsidr_el1 		/* reads current Cache Size ID register */; \
	and 	x2, x1, #7 		/* extract the line length field */; \
	add 	x2, x2, #4 		/* add 4 for the line length offset (log2 16 bytes) */; \
	mov 	x4, #0x3ff		;\
	ands	x4, x4, x1, lsr #3 	/* x4 is the max number on the way size (right aligned) */; \
	clz 	w5, w4 			/* x5 is the bit position of the way size increment */; \
	mov 	x6, #0x7fff		;\
	ands	x6, x6, x1, lsr #13 	/* x6 is the max number of the index size (right aligned) */; \
2: 					;\
	mov 	x7, x4 			/* x7 working copy of the max way size (right aligned) */; \
3:					;\
	lsl	x1, x7, x5		;\
	orr	x9, x10, x1		/* factor way and cache number into x9 */; \
	lsl	x1, x6, x2		;\
	orr	x9, x9, x1		/* factor index number into x9 */; \
	dc 	cacheop, x9		/* clean/invalidate/flush by set/way */; \
	subs	x7, x7, #1 		/* decrement the way number */; \
	bge 	3b			;\
	subs 	x6, x6, #1 		/* decrement the index */; \
	bge 	2b			;\
4:					;\
	add 	x8, x8, #2 		/* increment the cache number */; \
	cmp 	x3, x8			;\
	bgt 	1b			;\
5:					;\
	pop	x10, x11		/* restore registers */;\
	pop	x8, x9			/* restore registers */;\
	pop	x6, x7			/* restore registers */;\
	pop	x4, x5			/* restore registers */;\
	pop	x2, x3			/* restore registers */;\
	pop	x0, x1			/* restore registers */;\

	/*
	 * Operations on entire data Cache to POC
	 */

	/* clean the entire data cache */	
	.globl clean_dcache
clean_dcache:
	ARM_ENTIRE_DCACHE_OP(csw) 	/* clean all */
	dsb	sy
	isb
	ret

	/* clean & invalidate the entire data cache */	
	.globl clean_invalidate_dcache
clean_invalidate_dcache:
	ARM_ENTIRE_DCACHE_OP(cisw) 	/* clean and invalidate all */
	dsb	sy
	isb
	ret

	/*
	 * Operations on data cache by MVA
	 */

	/* invalidate by mva */
	.globl invalidate_dcache_mva
invalidate_dcache_mva:
	dc	ivac, x0
	dsb	sy
	isb
	ret

	/* invalidate memory region by mva range
	 *  x0 - start address of region
	 *  x1 - end address of region
	 */
	.globl invalidate_dcache_mva_range
invalidate_dcache_mva_range:
	push	x2, x3
	dcache_line_size x2, x3
	sub	x3, x2, #1
	tst	x0, x3
	bic	x0, x0, x3
	beq	1f
	dc	cvac, x0
1:
	tst	x1, x3
	bic	x1, x1, x3
	beq	2f
	dc	cvac, x1
2:
	dc	ivac, x0
	add	x0, x0, x2
	cmp	x0, x1
	blo	2b
	dsb	sy
	isb
	pop	x2, x3
	ret

	/* clean by mva */
	.globl clean_dcache_mva
clean_dcache_mva:
	dc	cvac, x0
	dsb	sy
	isb
	ret

	/* clean memory region by mva range
	 *  x0 - start address of region
	 *  x1 - end address of region
	 */
	.globl clean_dcache_mva_range
clean_dcache_mva_range:
	push	x2, x3
	dcache_line_size x2, x3
	sub	x3, x2, #1
	bic	x0, x0, x3
1:
	dc	cvac, x0
	add	x0, x0, x2
	cmp	x0, x1
	blo	1b
	dsb	sy
	isb
	pop	x2, x3
	ret

	/* clean and invalidate by mva */
	.globl clean_invalidate_dcache_mva
clean_invalidate_dcache_mva:
	dc	civac, x0
	dsb	sy
	isb
	ret

	/* clean and invalidate a memory region by mva
	 *  x0 - start address of region
	 *  x1 - end address of region
	 */
	.globl clean_invalidate_dcache_mva_range
clean_invalidate_dcache_mva_range:
	push	x2, x3
	dcache_line_size x2, x3
	sub	x3, x2, #1
	bic	x0, x0, x3
1:
	dc	civac, x0		/* clean & invalidate D / U line */
	add	x0, x0, x2
	cmp	x0, x1
	blo	1b
	dsb	sy
	isb
	pop	x2, x3
	ret

	/*
	 * Operations on data cache line by set/way
	 */

	/* clean line by set/way */
	.globl clean_dcache_line
clean_dcache_line:
	dc	cvac, x0
	dsb	sy
	isb
	ret

	/* clean and invalidate line by set/way */
	.globl clean_invalidate_dcache_line
clean_invalidate_dcache_line:
	dc	civac, x0
	dsb	sy
	isb
	ret

	/*
	 * Operation on entire Instruction cache
	 */

	/* invalidate the entire i-cache */
	.globl invalidate_icache
invalidate_icache:
	ic	ialluis 	/* invalidate all */
	isb
	ret

	/* invalidate i-cache by mva */
	.globl invalidate_icache_mva
invalidate_icache_mva:
	ic	ivau, x0
	isb
	ret

	/*
	 * Operations on entire instruction an data cache
	 */

	/* clean the entire i-cache and d-cache */
	.globl clean_idcache
clean_idcache:
	b	clean_dcache

	/* clean and invalidate the entire i-cache and d-cache */
	.globl clean_invalidate_idcache
clean_invalidate_idcache:
	str	lr, [sp, #8]!
	bl	invalidate_icache
	bl	clean_invalidate_dcache
	ldr	lr, [sp], #8
	ret

	/*
	 * operation on both i-cache and d-cache by mva
	 */

	/* clean both i-cache and d-cache by mva */
	.globl clean_idcache_mva
clean_idcache_mva:
	b	clean_dcache_mva

	/* clean and invalidate both i-cache and d-cache by mva */
	.globl clean_invalidate_idcache_mva
clean_invalidate_idcache_mva:
	str	lr, [sp, #8]!
	bl	invalidate_icache_mva
	bl	clean_invalidate_dcache_mva
	ldr	lr, [sp], #8
	ret

	/*
	 * operation on both i-cache and d-cache line by set/way
	 */

	/* clean both i-cache and d-cache line by set/way */
	.globl clean_idcache_line
clean_idcache_line:
	b	clean_dcache_line

	/* clean and invalidate both i-cache and d-cache line by set/way */
	.globl clean_invalidate_idcache_line
clean_invalidate_idcache_line:
	str	lr, [sp, #8]!
	bl	invalidate_icache
	bl	clean_invalidate_dcache_line
	ldr	lr, [sp], #8
	ret

