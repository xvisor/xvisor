/**
 * Copyright (c) 2018 Anup Patel.
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
#include <vmm_host_irq.h>
#include <arch_regs.h>

#include <riscv_csr.h>

void do_handle_exception(arch_regs_t *regs)
{
	/* TODO: */
	while (1);
}

int __cpuinit arch_cpu_irq_setup(void)
{
	extern unsigned long _handle_exception[];

	/* Setup final exception handler */
	csr_write(stvec, (virtual_addr_t)&_handle_exception);

	return VMM_OK;
}

