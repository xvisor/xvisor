/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source code for handling cpu interrupts
 */

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_scheduler.h>
#include <vmm_host_irq.h>
#include <vmm_stdio.h>
#include <cpu_interrupts.h>
#include <cpu_timer.h>
#include <cpu_mmu.h>

int arch_cpu_irq_setup(void)
{
}

void arch_cpu_irq_enable(void)
{
}

void arch_cpu_irq_disable(void)
{
}

irq_flags_t arch_cpu_irq_save(void)
{
        return 0;
}

void arch_cpu_irq_restore(irq_flags_t flags)
{
}

void arch_cpu_wait_for_irq(void)
{
	/* FIXME: Use some hardware functionality to wait for interrupt */
	/* OR */
	/* FIXME: Use some soft delay */
}
