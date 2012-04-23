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
#include <cpu_inline_asm.h>
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
	case CPU_HYP_TRAP_IRQ:
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

int arch_vcpu_irq_assert(struct vmm_vcpu * vcpu, u32 irq_no, u32 reason)
{
	u32 hcr = arm_priv(vcpu)->hcr;

	switch(irq_no) {
	case CPU_DATA_ABORT_IRQ:
		hcr |= HCR_VA_MASK;
		/* VA bit is auto-cleared */
		break;
	case CPU_EXTERNAL_IRQ:
		hcr |= HCR_VI_MASK;
		/* VI bit will be cleared on deassertion */
		break;
	case CPU_EXTERNAL_FIQ:
		hcr |= HCR_VF_MASK;
		/* VF bit will be cleared on deassertion */
		break;
	default:
		return VMM_EFAIL;
		break;
	};

	arm_priv(vcpu)->hcr = hcr;
	write_hcr(arm_priv(vcpu)->hcr);

	return VMM_OK;
}


int arch_vcpu_irq_execute(struct vmm_vcpu * vcpu,
			 arch_regs_t * regs, 
			 u32 irq_no, u32 reason)
{
	return VMM_OK;
}

int arch_vcpu_irq_deassert(struct vmm_vcpu * vcpu, u32 irq_no, u32 reason)
{
	u32 hcr = read_hcr();

	switch(irq_no) {
	case CPU_EXTERNAL_IRQ:
		hcr &= ~HCR_VI_MASK;
		break;
	case CPU_EXTERNAL_FIQ:
		hcr &= ~HCR_VF_MASK;
		break;
	default:
		return VMM_EFAIL;
		break;
	};

	arm_priv(vcpu)->hcr = hcr;
	write_hcr(hcr);

	return VMM_OK;
}

