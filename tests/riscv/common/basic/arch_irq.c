/**
 * Copyright (c) 2019 Anup Patel.
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

#include <riscv_asm.h>
#include <riscv_defines.h>
#include <arch_irq.h>
#include <basic_irq.h>

void do_exec(struct pt_regs *regs)
{
	if (csr_read(scause) & SCAUSE_INTERRUPT_MASK) {
		if (basic_irq_exec_handler(regs)) {
			while (1);
		}
	} else {
		while (1);
	}
}

void arch_irq_setup(void)
{
	/*
	 * Nothing to do here.
	 */
}

void arch_irq_enable(void)
{
	csr_set(sstatus, SR_SIE);
}

void arch_irq_disable(void)
{
	csr_clear(sstatus, SR_SIE);
}

void arch_irq_wfi(void)
{
	wfi();
}
