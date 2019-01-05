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
 * @file riscv_intc.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V local interrupt controller driver
 */

#include <arch_asm.h>
#include <arch_defines.h>
#include <pic/riscv_intc.h>

u32 riscv_intc_nr_irqs(void)
{
	return __riscv_xlen;
}

u32 riscv_intc_active_irq(void)
{
	unsigned long scause = csr_read(scause);

	if (scause & SCAUSE_INTERRUPT_MASK)
		return scause & SCAUSE_CAUSE_MASK;

	return -1;
}

int riscv_intc_ack_irq(u32 irq)
{
	if (irq >= __riscv_xlen)
		return -1;

	return 0;
}

int riscv_intc_eoi_irq(u32 irq)
{
	if (irq >= __riscv_xlen)
		return -1;

	csr_clear(sip, (1UL << irq));
	return 0;
}

int riscv_intc_mask(u32 irq)
{
	if (irq >= __riscv_xlen)
		return -1;

	csr_clear(sie, (1UL << irq));
	return 0;
}

int riscv_intc_unmask(u32 irq)
{
	if (irq >= __riscv_xlen)
		return -1;

	csr_set(sie, (1UL << irq));
	return 0;
}

int riscv_intc_init(void)
{
	csr_write(sie, 0);
	csr_write(sip, 0);
	return 0;
}
