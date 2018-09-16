/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file arch_irq.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for arch specific interrupt handling
 */

#include <arch_irq.h>
#include <basic_irq.h>

extern void arm_sync_abort(struct pt_regs *regs);

void do_bad_mode(struct pt_regs *regs)
{
	while(1);
}

void do_sync(struct pt_regs *regs)
{
	arm_sync_abort(regs);
}

void do_irq(struct pt_regs *regs)
{
	if (basic_irq_exec_handler(regs)) {
		while (1);
	}
}

void do_fiq(struct pt_regs *regs)
{
}

void arch_irq_setup(void)
{
	/*
	 * Nothing to do here.
	 */
}

void arch_irq_enable(void)
{
	asm volatile("msr daifclr, #2":::"memory");
}

void arch_irq_disable(void)
{
	asm volatile("msr daifset, #2":::"memory");
}

void arch_irq_wfi(void)
{
	__asm ("wfi\n");
}
