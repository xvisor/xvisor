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
 * @file cpu_proc_v6.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief Low-level implementation of ARMv6 specific quirky functions
 */

#include <cpu_defines.h>

#define D_CACHE_LINE_SIZE	32

#define TTB_C		(1 << 0)
#define TTB_S		(1 << 1)
#define TTB_IMP		(1 << 2)
#define TTB_RGN_NC	(0 << 3)
#define TTB_RGN_WBWA	(1 << 3)
#define TTB_RGN_WT	(2 << 3)
#define TTB_RGN_WB	(3 << 3)

#if 0 /* Note: We don't use cachable page-table walks */
#define TTB_FLAGS_UP	TTB_RGN_WBWA
#else
#define TTB_FLAGS_UP	0x0
#endif

#if 0 /* Note: We don't use cachable page-table walks */
#define TTB_FLAGS_SMP	TTB_RGN_WBWA|TTB_S
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
	mov	r1, #0
	mcr	p15, 0, r1, c7, c10, 4		@ DWB - WFI may enter a low-power mode
	mcr	p15, 0, r1, c7, c0, 4		@ wait for interrupt
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
	mcr	p15, 0, r2, c7, c5, 6		@ flush BTAC/BTB
	mcr	p15, 0, r2, c7, c10, 4		@ drain write buffer
	mcr	p15, 0, r0, c2, c0, 0		@ set TTBR0
#ifdef CONFIG_PID_IN_CONTEXTIDR
	mrc	p15, 0, r2, c13, c0, 1		@ read current context ID
	bic	r2, r2, #0xff			@ extract the PID
	and	r1, r1, #0xff
	orr	r1, r1, r2			@ insert into new context ID
#endif
	mcr	p15, 0, r1, c13, c0, 1		@ set context ID
	bx 	lr

/*
 *	Boot-time processor setup
 *
 *	Initialise TLB, Caches, and MMU state ready to switch the MMU
 *	on.  Return in r0 the new CP15 C1 control register setting.
 *
 *	We automatically detect if we have a Harvard cache, and use the
 *	Harvard cache control instructions insead of the unified cache
 *	control instructions.
 *
 *	This should be able to cover all ARMv6 cores.
 *
 *	It is assumed that:
 *	- cache type register is implemented
 *
 *	Note: We blindly use all registers because this will be
 *	called at boot-time when there is not stack
 */
	.globl proc_setup
proc_setup:
	mrc	p15, 0, r0, c0, c0, 0		@ Read Main ID register
	and	r10, r0, #0xff000000		@ ARM?
	teq	r10, #0x41000000
	bne	arm_proc_setup_skip
	lsl	r0, r0, #16
	lsr	r0, r0, #20

	ldr	r10, =0x00000b02		@ ARM11MP primary part number
	teq	r0, r10
	bne	arm_arm11mp_skip
#ifdef CONFIG_SMP
	mrc	p15, 0, r0, c1, c0, 1		@ Enable SMP/nAMP mode
	nop
	orr	r0, r0, #0x20
	mcr	p15, 0, r0, c1, c0, 1
	nop
#endif
	b	proc_specific_setup_done
arm_arm11mp_skip:
arm_proc_setup_skip:

proc_specific_setup_done:
	mov	r0, #0
	mcr	p15, 0, r0, c7, c14, 0		@ clean+invalidate D cache
	mcr	p15, 0, r0, c7, c5, 0		@ invalidate I cache
	mcr	p15, 0, r0, c7, c15, 0		@ clean+invalidate cache
	mcr	p15, 0, r0, c7, c10, 4		@ drain write buffer

	mcr	p15, 0, r0, c8, c7, 0		@ invalidate I + D TLBs
	mcr	p15, 0, r0, c2, c0, 2		@ TTB control register

	adr	r5, v6_crval
	ldmia	r5, {r5, r6}
	mrc	p15, 0, r0, c1, c0, 0		@ read control register
	bic	r0, r0, r5			@ clear bits them
	orr	r0, r0, r6			@ set them
#ifdef CONFIG_ARM_ERRATA_364296
	/*
	 * Workaround for the 364296 ARM1136 r0p2 erratum (possible cache data
	 * corruption with hit-under-miss enabled). The conditional code below
	 * (setting the undocumented bit 31 in the auxiliary control register
	 * and the FI bit in the control register) disables hit-under-miss
	 * without putting the processor into full low interrupt latency mode.
	 */
	ldr	r6, =0x4107b362			@ id for ARM1136 r0p2
	mrc	p15, 0, r5, c0, c0, 0		@ get processor id
	teq	r5, r6				@ check for the faulty core
	mrceq	p15, 0, r5, c1, c0, 1		@ load aux control reg
	orreq	r5, r5, #(1 << 31)		@ set the undocumented bit 31
	mcreq	p15, 0, r5, c1, c0, 1		@ write aux control reg
	orreq	r0, r0, #(1 << 21)		@ low interrupt latency configuration
#endif
	mov	pc, lr				@ return 

	/*
	 *         V X F   I D LR
	 * .... ...E PUI. .T.T 4RVI ZFRS BLDP WCAM
	 * rrrr rrrx xxx0 0101 xxxx xxxx x111 xxxx < forced
	 *         0 110       0001 1.00 .111 1101 < we want
	 */
	.align	2
	.type	v6_crval, #object
v6_crval:
	.word	0x01e0fb7f /* clear */
	.word	0x00c0187d /* mmuset */

#ifdef CONFIG_SMP
	/* 
	 * Retrive SMP ID of current processor
	 */
	.globl arch_smp_id
arch_smp_id:
	mrc	p15, 0, r0, c13, c0, 4
	bx 	lr

	/* 
	 * Setup SMP ID of current processor
	 */
	.globl proc_setup_smp_id
proc_setup_smp_id:
	/* Ensure that next SMP ID in r0
	 * is less than CONFIG_CPU_COUNT
	 */
	ldr	r1, =CONFIG_CPU_COUNT
	cmp	r0, r1
	blt	proc_setup_smp_id_done
	b	.

proc_setup_smp_id_done:
	mcr	p15, 0, r0, c13, c0, 4
	bx	lr
#endif
