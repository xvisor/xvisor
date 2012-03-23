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
 * @file cpu_vcpu_irq.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling vcpu interrupts
 */

#include <vmm_error.h>
#include <vmm_vcpu_irq.h>
#include <arch_cpu.h>
#include <cpu_vcpu_helper.h>
#include <cpu_defines.h>

u32 arch_vcpu_irq_count(struct vmm_vcpu * vcpu)
{
	return CPU_IRQ_NR;
}

u32 arch_vcpu_irq_priority(struct vmm_vcpu * vcpu, u32 irq_no)
{
	u32 ret = 3;

	switch (irq_no) {
	case CPU_RESET_IRQ:
		ret = 0;
		break;
	case CPU_UNDEF_INST_IRQ:
		ret = 1;
		break;
	case CPU_SOFT_IRQ:
		ret = 2;
		break;
	case CPU_PREFETCH_ABORT_IRQ:
		ret = 2;
		break;
	case CPU_DATA_ABORT_IRQ:
		ret = 2;
		break;
	case CPU_NOT_USED_IRQ:
		ret = 2;
		break;
	case CPU_EXTERNAL_IRQ:
		ret = 2;
		break;
	case CPU_EXTERNAL_FIQ:
		ret = 2;
		break;
	default:
		break;
	};
	return ret;
}

/* FIXME: */
int arch_vcpu_irq_execute(struct vmm_vcpu * vcpu,
			 arch_regs_t * regs, 
			 u32 irq_no, u32 reason)
{
	return VMM_OK;
}
