/**
 * Copyright (c) 2012 Anup Patel.
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
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_helper.h>
#include <cpu_defines.h>

/* FIXME: */
void do_undef_inst(arch_regs_t * uregs)
{
	return;
}

/* FIXME: */
void do_soft_irq(arch_regs_t * uregs)
{
	return;
}

/* FIXME: */
void do_prefetch_abort(arch_regs_t * uregs)
{
	return;
}

/* FIXME: */
void do_data_abort(arch_regs_t * uregs)
{
	return;
}

/* FIXME: */
void do_not_used(arch_regs_t * uregs)
{
	return;
}

/* FIXME: */
void do_irq(arch_regs_t * uregs)
{
	return;
}

/* FIXME: */
void do_fiq(arch_regs_t * uregs)
{
	return;
}

int __init arch_cpu_irq_setup(void)
{
	extern u32 _start_vect[];

	/* Update HVBAR to point to hypervisor vector table */
	write_hvbar((virtual_addr_t)&_start_vect);

	return VMM_OK;
}

void arch_cpu_irq_enable(void)
{
	__asm("cpsie i");
}

void arch_cpu_irq_disable(void)
{
	__asm("cpsid i");
}

irq_flags_t arch_cpu_irq_save(void)
{
	unsigned long retval;

	asm volatile (" mrs     %0, cpsr\n\t" " cpsid   i"	/* Syntax CPSID <iflags> {, #<p_mode>}
								 * Note: This instruction is supported 
								 * from ARM6 and above
								 */
		      :"=r" (retval)::"memory", "cc");

	return retval;
}

void arch_cpu_irq_restore(irq_flags_t flags)
{
	asm volatile (" msr     cpsr_c, %0"::"r" (flags)
		      :"memory", "cc");
}

void arch_cpu_wait_for_irq(void)
{
	/* We could also use soft delay here. */
	asm volatile (" wfi ");
}
