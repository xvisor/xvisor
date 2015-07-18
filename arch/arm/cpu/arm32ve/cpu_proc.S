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
 * @file cpu_proc.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief Low-level implementation of ARMv7 with virtualization extension
 * specific quirky functions
 */

#include <cpu_defines.h>

/*
 *	Boot-time processor setup
 *
 *	Initialise TLB, Caches, and MMU state ready to switch the MMU
 *	on.  Return in r0 the new CP15 C1 hypervisor control register setting.
 *
 *	This should cover all ARMv7 with virtualization extension cores.
 *
 *	Note: We blindly use all registers because this will be
 *	called at boot-time when there is not stack
 */
	.globl proc_setup
proc_setup:
#ifdef CONFIG_SMP
	mov	r10, #0
	mrc	p15, 0, r0, c1, c0, 1
	tst	r0, #(1 << 6)			@ SMP/nAMP mode enabled?
	orreq	r0, r0, #(1 << 6)		@ Enable SMP/nAMP mode
	orreq	r0, r0, r10			@ Enable CPU-specific SMP bits
	mcreq	p15, 0, r0, c1, c0, 1
#endif

	mrc	p15, 0, r0, c0, c0, 0		@ read main ID register
	and	r10, r0, #0xff000000		@ ARM?
	teq	r10, #0x41000000
	bne	3f
	and	r5, r0, #0x00f00000		@ variant
	and	r6, r0, #0x0000000f		@ revision
	orr	r6, r6, r5, lsr #20-4		@ combine variant and revision
	ubfx	r0, r0, #4, #12			@ primary part number

	/* Cortex-A15 Errata */
2:	ldr	r10, =0x00000c0f		@ Cortex-A15 primary part number
	teq	r0, r10
	bne	3f
#ifdef CONFIG_ARM_ERRATA_773022
	cmp	r6, #0x4			@ only present up to r0p4
	mrcle	p15, 0, r10, c1, c0, 1		@ read aux control register
	orrle	r10, r10, #1 << 1		@ disable loop buffer
	mcrle	p15, 0, r10, c1, c0, 1		@ write aux control register
#endif

3:	mov	r10, #0
	mcr	p15, 0, r10, c7, c5, 0		@ I+BTB cache invalidate
	mcr     p15, 4, r10, c8, c3, 0		@ invalidate all hyp TLBs
	dsb

	adr	r5, v7_hcrval
	ldmia	r5, {r5, r6}
	mrc	p15, 4, r0, c1, c0, 0		@ read HSCTLR register
	bic	r0, r0, r5			@ clear bits them
	orr	r0, r0, r6			@ set them
	mov	pc, lr

	/*                W                B
	 *  T     E    F  X                E
	 * .E.. ..E. ..I. N... ...I .... ..N. .CAM
	 * 0x11 00x0 11x0 x101 000x 1000 01x1 1xxx < forced
	 *  0     0    0  0       1        1   101 < we want
	 */
	.align	2
	.type	v7_hcrval, #object
v7_hcrval:
	.word	0x42280002 /* clear */
	.word	0x00001025 /* mmuset */

#ifdef CONFIG_SMP
	/* 
	 * Retrive SMP ID of current processor
	 */
	.globl arch_smp_id
arch_smp_id:
	mrc	p15, 4, r0, c13, c0, 2
	bx 	lr

	.align	2
	.globl proc_smp_counter
proc_smp_counter:
	.word	0x0

	/* 
	 * Setup SMP ID of current processor
	 */
	.globl proc_setup_smp_id
proc_setup_smp_id:
	/* Determine next SMP ID in r0 */
	adr	r1, proc_smp_counter
	ldr	r0, [r1]
	add	r2, r0, #1
	str	r2, [r1]

	/* Ensure that next SMP ID in r0
	 * is less than CONFIG_CPU_COUNT
	 */
	ldr	r1, =CONFIG_CPU_COUNT
	cmp	r0, r1
	blt	setup_smp_id_done
	b	.

setup_smp_id_done:
	mcr	p15, 4, r0, c13, c0, 2
	bx	lr
#endif
