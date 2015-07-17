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
 * @file cpu_proc_v7.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief Low-level implementation of ARMv7 specific quirky functions
 */

#include <cpu_defines.h>

#define TTB_S		(1 << 1)
#define TTB_RGN_NC	(0 << 3)
#define TTB_RGN_OC_WBWA	(1 << 3)
#define TTB_RGN_OC_WT	(2 << 3)
#define TTB_RGN_OC_WB	(3 << 3)
#define TTB_NOS		(1 << 5)
#define TTB_IRGN_NC	((0 << 0) | (0 << 6))
#define TTB_IRGN_WBWA	((0 << 0) | (1 << 6))
#define TTB_IRGN_WT	((1 << 0) | (0 << 6))
#define TTB_IRGN_WB	((1 << 0) | (1 << 6))

#if 0 /* Note: We don't use cachable page-table walks */
/* PTWs cacheable, inner WB not shareable, outer WB not shareable */
#define TTB_FLAGS_UP	TTB_IRGN_WB|TTB_RGN_OC_WB
#else
#define TTB_FLAGS_UP	0x0
#endif

#if 0 /* Note: We don't use cachable page-table walks */
/* PTWs cacheable, inner WBWA shareable, outer WBWA not shareable */
#define TTB_FLAGS_SMP	TTB_IRGN_WBWA|TTB_S|TTB_NOS|TTB_RGN_OC_WBWA
#else
#define TTB_FLAGS_SMP	0x0
#endif

/*
 *	Idle the processor (eg, wait for interrupt).
 *
 *	IRQs are already disabled.
 */
	.globl proc_do_idle
proc_do_idle:
	dsb
	wfi
	mov	pc, lr

/*
 *	MMU context switch
 *
 *	Set the translation table base pointer and context ID
 *
 *	It is assumed that:
 *	- we are not using split page tables
 */
	.globl proc_mmu_switch
proc_mmu_switch:
	mov	r2, #0
#ifdef CONFIG_SMP
	orr	r0, r0, #TTB_FLAGS_SMP
#else
	orr	r0, r0, #TTB_FLAGS_UP
#endif
#ifdef CONFIG_ARM_ERRATA_430973
	mcr	p15, 0, r2, c7, c5, 6		@ flush BTAC/BTB
#endif
#ifdef CONFIG_PID_IN_CONTEXTIDR
	mrc	p15, 0, r2, c13, c0, 1		@ read current context ID
	lsr	r2, r2, #8			@ extract the PID
	bfi	r1, r2, #8, #24			@ insert into new context ID
#endif
#ifdef CONFIG_ARM_ERRATA_754322
	dsb
#endif
	mcr	p15, 0, r0, c2, c0, 0		@ set TTBR0
	isb
	mcr	p15, 0, r1, c13, c0, 1		@ set context ID
	isb
	bx 	lr

/*
 *	Boot-time processor setup
 *
 *	Initialise TLB, Caches, and MMU state ready to switch the MMU
 *	on.  Return in r0 the new CP15 C1 control register setting.
 *
 *	This should be able to cover all ARMv7 cores.
 *
 *	It is assumed that:
 *	- cache type register is implemented
 *
 *	Note: We blindly use all registers because this will be
 *	called at boot-time when there is not stack
 */
	.globl proc_setup
proc_setup:
	mrc	p15, 0, r0, c0, c0, 0		@ read main ID register
	and	r10, r0, #0xff000000		@ ARM?
	teq	r10, #0x41000000
	bne	arm_proc_setup_skip
	and	r5, r0, #0x00f00000		@ variant
	and	r6, r0, #0x0000000f		@ revision
	orr	r6, r6, r5, lsr #20-4		@ combine variant and revision
	ubfx	r0, r0, #4, #12			@ primary part number

	/* Cortex-A8 Errata */
	ldr	r10, =0x00000c08		@ Cortex-A8 primary part number
	teq	r0, r10
	bne	arm_cortex_a8_skip
#ifdef CONFIG_ARM_ERRATA_430973
	teq	r5, #0x00100000			@ only present in r1p*
	mrceq	p15, 0, r10, c1, c0, 1		@ read aux control register
	orreq	r10, r10, #(1 << 6)		@ set IBE to 1
	mcreq	p15, 0, r10, c1, c0, 1		@ write aux control register
#endif
#ifdef CONFIG_ARM_ERRATA_458693
	teq	r6, #0x20			@ only present in r2p0
	mrceq	p15, 0, r10, c1, c0, 1		@ read aux control register
	orreq	r10, r10, #(1 << 5)		@ set L1NEON to 1
	orreq	r10, r10, #(1 << 9)		@ set PLDNOP to 1
	mcreq	p15, 0, r10, c1, c0, 1		@ write aux control register
#endif
#ifdef CONFIG_ARM_ERRATA_460075
	teq	r6, #0x20			@ only present in r2p0
	mrceq	p15, 1, r10, c9, c0, 2		@ read L2 cache aux ctrl register
	tsteq	r10, #1 << 22
	orreq	r10, r10, #(1 << 22)		@ set the Write Allocate disable bit
	mcreq	p15, 1, r10, c9, c0, 2		@ write the L2 cache aux ctrl register
#endif
	b	proc_specific_setup_done
arm_cortex_a8_skip:

	/* Cortex-A9 Errata */
	ldr	r10, =0x00000c09		@ Cortex-A9 primary part number
	teq	r0, r10
	bne	arm_cortex_a9_skip
#ifdef CONFIG_SMP
	mov	r10, #(1 << 0)			@ TLB ops broadcasting
	mrc	p15, 0, r0, c1, c0, 1
	tst	r0, #(1 << 6)			@ SMP/nAMP mode enabled?
	orreq	r0, r0, #(1 << 6)		@ Enable SMP/nAMP mode
	orreq	r0, r0, r10			@ Enable CPU-specific SMP bits
	mcreq	p15, 0, r0, c1, c0, 1
#endif
#ifdef CONFIG_ARM_ERRATA_742230
	cmp	r6, #0x22			@ only present up to r2p2
	mrcle	p15, 0, r10, c15, c0, 1		@ read diagnostic register
	orrle	r10, r10, #1 << 4		@ set bit #4
	mcrle	p15, 0, r10, c15, c0, 1		@ write diagnostic register
#endif
#ifdef CONFIG_ARM_ERRATA_742231
	teq	r6, #0x20			@ present in r2p0
	teqne	r6, #0x21			@ present in r2p1
	teqne	r6, #0x22			@ present in r2p2
	mrceq	p15, 0, r10, c15, c0, 1		@ read diagnostic register
	orreq	r10, r10, #1 << 12		@ set bit #12
	orreq	r10, r10, #1 << 22		@ set bit #22
	mcreq	p15, 0, r10, c15, c0, 1		@ write diagnostic register
#endif
#ifdef CONFIG_ARM_ERRATA_743622
	teq	r5, #0x00200000			@ only present in r2p*
	mrceq	p15, 0, r10, c15, c0, 1		@ read diagnostic register
	orreq	r10, r10, #1 << 6		@ set bit #6
	mcreq	p15, 0, r10, c15, c0, 1		@ write diagnostic register
#endif
#if defined(CONFIG_ARM_ERRATA_751472) && defined(CONFIG_SMP)
	cmp	r6, #0x30			@ present prior to r3p0
	mrclt	p15, 0, r10, c15, c0, 1		@ read diagnostic register
	orrlt	r10, r10, #1 << 11		@ set bit #11
	mcrlt	p15, 0, r10, c15, c0, 1		@ write diagnostic register
#endif
	b	proc_specific_setup_done
arm_cortex_a9_skip:

	/* Cortex-A15 Errata */
	ldr	r10, =0x00000c0f		@ Cortex-A15 primary part number
	teq	r0, r10
	bne	arm_cortex_a15_skip
#ifdef CONFIG_SMP
	mov	r10, #0
	mrc	p15, 0, r0, c1, c0, 1
	tst	r0, #(1 << 6)			@ SMP/nAMP mode enabled?
	orreq	r0, r0, #(1 << 6)		@ Enable SMP/nAMP mode
	orreq	r0, r0, r10			@ Enable CPU-specific SMP bits
	mcreq	p15, 0, r0, c1, c0, 1
#endif
#ifdef CONFIG_ARM_ERRATA_773022
	cmp	r6, #0x4			@ only present up to r0p4
	mrcle	p15, 0, r10, c1, c0, 1		@ read aux control register
	orrle	r10, r10, #1 << 1		@ disable loop buffer
	mcrle	p15, 0, r10, c1, c0, 1		@ write aux control register
#endif
arm_cortex_a15_skip:

arm_proc_setup_skip:

proc_specific_setup_done:
	mov	r10, #0
	mcr	p15, 0, r10, c7, c5, 0		@ I+BTB cache invalidate
	dsb

	adr	r5, v7_crval
	ldmia	r5, {r5, r6}
#ifdef CONFIG_SWP_EMULATE
	orr     r5, r5, #(1 << 10)              @ set SW bit in "clear"
	bic     r6, r6, #(1 << 10)              @ clear it in "mmuset"
#endif
   	mrc	p15, 0, r0, c1, c0, 0		@ read control register
	bic	r0, r0, r5			@ clear bits them
	orr	r0, r0, r6			@ set them
	mov	pc, lr

	/*   AT
	 *  TFR   EV X F   I D LR    S
	 * .EEE ..EE PUI. .T.T 4RVI ZWRS BLDP WCAM
	 * rxxx rrxx xxx0 0101 xxxx xxxx x111 xxxx < forced
	 *    0    0 110       0001 1100 .111 1101 < we want
	 */
	.align	2
	.type	v7_crval, #object
v7_crval:
	.word	0x0120c302 /* clear */
	.word	0x00c01c7d /* mmuset */

#ifdef CONFIG_SMP
	/* 
	 * Retrive SMP ID of current processor
	 */
	.globl arch_smp_id
arch_smp_id:
	mrc	p15, 0, r0, c13, c0, 4
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
1:	ldrex	r0, [r1]
	add	r0, r0, #1
	strex	r2, r0, [r1]
	teq	r2, #0
	bne	1b
	sub	r0, r0, #1

	/* Ensure that next SMP ID in r0
	 * is less than CONFIG_CPU_COUNT
	 */
	ldr	r1, =CONFIG_CPU_COUNT
	cmp	r0, r1
	blt	setup_smp_id_done
	b	.

setup_smp_id_done:
	mcr	p15, 0, r0, c13, c0, 4
	bx	lr
#endif
