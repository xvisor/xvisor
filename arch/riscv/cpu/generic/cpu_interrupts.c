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
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <vmm_scheduler.h>
#include <arch_vcpu.h>

#include <riscv_csr.h>

static virtual_addr_t preempt_orphan_pc =
			(virtual_addr_t)&arch_vcpu_preempt_orphan;

void do_handle_async(arch_regs_t *regs,
		     unsigned long exc, unsigned long baddr)
{
	vmm_scheduler_irq_enter(regs, FALSE);

	/* NOTE: Only exec <= 0xFFFFFFFFUL will be handled */
	if (exc <= 0xFFFFFFFFUL) {
		vmm_host_active_irq_exec(exc);
	}

	vmm_scheduler_irq_exit(regs);
}

void do_handle_sync(arch_regs_t *regs,
		    unsigned long exc, unsigned long baddr)
{
	if ((exc == EXC_STORE_AMO_PAGE_FAULT) &&
	    (regs->sstatus & SR_SPP) &&
	    (regs->sepc == preempt_orphan_pc)) {
		regs->sepc += 4;
		vmm_scheduler_preempt_orphan(regs);
		return;
	}

	vmm_scheduler_irq_enter(regs, TRUE);

	/* TODO: */

	vmm_scheduler_irq_exit(regs);
}

void do_handle_exception(arch_regs_t *regs)
{
	unsigned long baddr = csr_read(sbadaddr);
	unsigned long scause = csr_read(scause);

	if (scause & SCAUSE_INTERRUPT_MASK) {
		do_handle_async(regs, scause & SCAUSE_EXC_MASK, baddr);
	} else {
		do_handle_sync(regs, scause & SCAUSE_EXC_MASK, baddr);
	}
}

int __cpuinit arch_cpu_irq_setup(void)
{
	extern unsigned long _handle_exception[];

	/* Setup final exception handler */
	csr_write(stvec, (virtual_addr_t)&_handle_exception);

	return VMM_OK;
}

