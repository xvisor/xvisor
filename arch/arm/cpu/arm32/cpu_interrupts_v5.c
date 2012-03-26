/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file cpu_interrupts.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling cpu interrupts
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>
#include <cpu_inline_asm.h>
#include <cpu_mmu.h>
#include <cpu_vcpu_emulate_arm.h>
#include <cpu_vcpu_emulate_thumb.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_helper.h>
#include <cpu_defines.h>

void arch_cpu_irq_enable(void)
{
	unsigned long temp;

	asm volatile(
		"	mrs	%0, cpsr\n"
		"	bic	%0, %0, #128\n"
		"	msr	cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

void arch_cpu_irq_disable(void)
{
	unsigned long temp;

	asm volatile(
		"	mrs	%0, cpsr\n"
		"	orr	%0, %0, #128\n"
		"	msr	cpsr_c, %0"
		: "=r" (temp)
		:
		: "memory", "cc");
}

irq_flags_t arch_cpu_irq_save(void)
{
	unsigned long flags, temp;

	asm volatile(
		"	mrs	%0, cpsr\n"
		"	orr	%1, %0, #128\n"
		"	msr	cpsr_c, %1"
		: "=r" (flags), "=r" (temp)
		:
		: "memory", "cc");

	return flags;
}

void arch_cpu_irq_restore(irq_flags_t flags)
{
	asm volatile (" msr     cpsr_c, %0"::"r" (flags)
		      :"memory", "cc");
}

void arch_cpu_wait_for_irq(void)
{
	asm volatile (
		"	mov     r0, #0\n"
		"	mrc     p15, 0, r1, c1, c0, 0	@ Read control register\n"
		"	mcr	p15, 0, r0, c7, c10, 4	@ Drain write buffer\n"
		"	bic	r2, r1, #1 << 12\n"
		"	mrs	r3, cpsr		@ Disable FIQs while Icache\n"
		"	orr	ip, r3, #0x00000040	@ is disabled\n"
		"	msr	cpsr_c, ip\n"
		"	mcr	p15, 0, r2, c1, c0, 0	@ Disable I cache\n"
		"	mcr	p15, 0, r0, c7, c0, 4	@ Wait for interrupt\n"
		"	mcr	p15, 0, r1, c1, c0, 0	@ Restore ICache enable\n"
		"	msr	cpsr_c, r3		@ Restore FIQ state\n"
		"	mov	pc, lr"
		:::"memory", "cc" );
}
