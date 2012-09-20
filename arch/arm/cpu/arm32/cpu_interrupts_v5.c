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
 * @file cpu_interrupts_v5.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling cpu interrupts
 */

#include <cpu_inline_asm.h>
#include <cpu_mmu.h>
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

bool arch_cpu_irq_disabled(void)
{
	unsigned long flags;

	asm volatile (" mrs     %0, cpsr\n\t"
		      :"=r" (flags)
		      :
		      :"memory", "cc");

	return (flags & CPSR_IRQ_DISABLED) ? TRUE : FALSE;
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
	unsigned long reg_r0, reg_r1, reg_r2, reg_r3, reg_ip;

	asm volatile (
		"       mov     %0, #0\n"
                "       mrc     p15, 0, %1, c1, c0, 0   @ Read control register\n"
                "       mcr     p15, 0, %0, c7, c10, 4  @ Drain write buffer\n"
                "       bic     %2, %1, #1 << 12\n"
                "       mrs     %3, cpsr                @ Disable FIQs while Icache\n"
                "       orr     %4, %3, #0x00000040     @ is disabled\n"
                "       msr     cpsr_c, %4\n"
                "       mcr     p15, 0, %2, c1, c0, 0   @ Disable I cache\n"
                "       mcr     p15, 0, %0, c7, c0, 4   @ Wait for interrupt\n"
                "       mcr     p15, 0, %1, c1, c0, 0   @ Restore ICache enable\n"
                "       msr     cpsr_c, %3              @ Restore FIQ state"
		:"=r" (reg_r0), "=r" (reg_r1), "=r" (reg_r2), "=r" (reg_r3), "=r" (reg_ip)::"memory", "cc" );
}
