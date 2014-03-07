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
 * @file cpu_proc_v5.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief Low-level implementation of ARMv5 specific quirky functions
 *
 * Please note that these functions are primarily for ARM926.
 */

#include <cpu_defines.h>

/*
 *	Idle the processor (eg, wait for interrupt).
 *
 *	IRQs are already disabled.
 */
	.globl proc_do_idle
proc_do_idle:
	mov	r0, #0
	mrc	p15, 0, r1, c1, c0, 0		@ Read control register
	mcr	p15, 0, r0, c7, c10, 4		@ Drain write buffer
	bic	r2, r1, #1 << 12
	mrs	r3, cpsr			@ Disable FIQs while Icache
	orr	ip, r3, #CPSR_FIQ_DISABLED	@ is disabled
	msr	cpsr_c, ip
	mcr	p15, 0, r2, c1, c0, 0		@ Disable I cache
	mcr	p15, 0, r0, c7, c0, 4		@ Wait for interrupt
	mcr	p15, 0, r1, c1, c0, 0		@ Restore ICache enable
	msr	cpsr_c, r3			@ Restore FIQ state
	mov	pc, lr

/*
 *	MMU context switch
 *
 *	Set the translation table base pointer and context ID
 *
 *	It is assumed that:
 *	- we are not using split page tables
 *
 *	Note: For ARM926, we ignore the context ID
 */
	.globl proc_mmu_switch
proc_mmu_switch:
	mov	ip, #0
#ifdef CONFIG_CPU_DCACHE_WRITETHROUGH
	mcr	p15, 0, ip, c7, c6, 0		@ invalidate D cache
#else
@ && 'Clean & Invalidate whole DCache'
1:	mrc	p15, 0, r15, c7, c14, 3 	@ test,clean,invalidate
	bne	1b
#endif
	mcr	p15, 0, ip, c7, c5, 0		@ invalidate I cache
	mcr	p15, 0, ip, c7, c10, 4		@ drain WB
	mcr	p15, 0, r0, c2, c0, 0		@ load page table pointer
	mcr	p15, 0, ip, c8, c7, 0		@ invalidate I & D TLBs
	bx 	lr

/*
 *	Boot-time processor setup
 *
 *	Initialise TLB, Caches, and MMU state ready to switch the MMU
 *	on.  Return in r0 the new CP15 C1 control register setting.
 *
 *	Note: We blindly use all registers because this will be
 *	called at boot-time when there is not stack
 */
	.globl proc_setup
proc_setup:
	mov	r0, #0
	mcr	p15, 0, r0, c7, c7		@ invalidate I,D caches on v4
	mcr	p15, 0, r0, c7, c10, 4		@ drain write buffer on v4

	mcr	p15, 0, r0, c8, c7		@ invalidate I,D TLBs on v4

#ifdef CONFIG_CPU_DCACHE_WRITETHROUGH
	mov	r0, #4				@ disable write-back on caches explicitly
	mcr	p15, 7, r0, c15, c0, 0
#endif 

	adr	r5, arm926_crval
	ldmia	r5, {r5, r6}
	mrc	p15, 0, r0, c1, c0		@ get control register v4
	bic	r0, r0, r5
	orr	r0, r0, r6
#ifdef CONFIG_CPU_CACHE_ROUND_ROBIN
	orr	r0, r0, #0x4000			@ .1.. .... .... ....
#endif
	mov	pc, lr

	/*
	 *  R
	 * .RVI ZFRS BLDP WCAM
	 * .001 0001 ..11 0101
	 * 
	 */
	.align	2
	.type	arm926_crval, #object
arm926_crval:
	.word	0x00007f3f /* clear */
	.word	0x00001135 /* mmuset */

