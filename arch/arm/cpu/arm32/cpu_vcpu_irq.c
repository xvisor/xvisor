/**
 * Copyright (c) 2011 Anup Patel.
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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling vcpu interrupts
 */

#include <vmm_error.h>
#include <vmm_cpu.h>
#include <vmm_vcpu_irq.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_helper.h>
#include <cpu_defines.h>

u32 vmm_vcpu_irq_count(vmm_vcpu_t * vcpu)
{
	return CPU_IRQ_NR;
}

u32 vmm_vcpu_irq_priority(vmm_vcpu_t * vcpu, u32 irq_no)
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

int vmm_vcpu_irq_execute(vmm_vcpu_t * vcpu,
			 vmm_user_regs_t * regs, 
			 u32 irq_no, u32 reason)
{
	u32 old_cpsr, new_cpsr, new_mode, new_flags, lr_off;
	virtual_addr_t new_pc;

	old_cpsr = cpu_vcpu_cpsr_retrieve(vcpu, regs);
	new_cpsr = old_cpsr;
	new_flags = 0x0;

	switch(irq_no) {
	case CPU_RESET_IRQ:
		new_mode = CPSR_MODE_SUPERVISOR;
		new_flags |= CPSR_ASYNC_ABORT_DISABLED;
		new_flags |= CPSR_IRQ_DISABLED;
		new_flags |= CPSR_FIQ_DISABLED;
		lr_off = 0;
		break;
	case CPU_UNDEF_INST_IRQ:
		new_mode = CPSR_MODE_UNDEFINED;
		new_flags |= CPSR_IRQ_DISABLED;
		lr_off = 4;
		break;
	case CPU_SOFT_IRQ:
		new_mode = CPSR_MODE_SUPERVISOR;
		new_flags |= CPSR_IRQ_DISABLED;
		lr_off = 4;
		break;
	case CPU_PREFETCH_ABORT_IRQ:
		new_mode = CPSR_MODE_ABORT;
		new_flags |= CPSR_ASYNC_ABORT_DISABLED;
		new_flags |= CPSR_IRQ_DISABLED;
		lr_off = 4;
		break;
	case CPU_DATA_ABORT_IRQ:
		new_mode = CPSR_MODE_ABORT;
		new_flags |= CPSR_ASYNC_ABORT_DISABLED;
		new_flags |= CPSR_IRQ_DISABLED;
		lr_off = 8;
		break;
	case CPU_NOT_USED_IRQ:
		new_mode = CPSR_MODE_SUPERVISOR;
		new_flags |= CPSR_ASYNC_ABORT_DISABLED;
		new_flags |= CPSR_IRQ_DISABLED;
		new_flags |= CPSR_FIQ_DISABLED;
		lr_off = 0;
		break;
	case CPU_EXTERNAL_IRQ:
		if (old_cpsr & CPSR_IRQ_DISABLED) {
			return VMM_EFAIL;
		}
		new_mode = CPSR_MODE_IRQ;
		new_flags |= CPSR_ASYNC_ABORT_DISABLED;
		new_flags |= CPSR_IRQ_DISABLED;
		lr_off = 4;
		break;
	case CPU_EXTERNAL_FIQ:
		if (old_cpsr & CPSR_FIQ_DISABLED) {
			return VMM_EFAIL;
		}
		new_mode = CPSR_MODE_FIQ;
		new_flags |= CPSR_ASYNC_ABORT_DISABLED;
		new_flags |= CPSR_IRQ_DISABLED;
		new_flags |= CPSR_FIQ_DISABLED;
		lr_off = 4;
		break;
	default:
		return VMM_EFAIL;
		break;
	};

	new_pc = cpu_vcpu_cp15_vector_addr(vcpu, irq_no);
	new_cpsr &= ~CPSR_MODE_MASK;
	new_cpsr |= (new_mode | new_flags);
	new_cpsr &= ~(CPSR_IT1_MASK | CPSR_IT2_MASK);
	if (arm_feature(vcpu, ARM_FEATURE_V4T)) {
		if (arm_sregs(vcpu)->cp15.c1_sctlr & (1 << 30)) {
			new_cpsr |= CPSR_THUMB_ENABLED;
		} else {
			new_cpsr &= ~CPSR_THUMB_ENABLED;
		}
	}
	cpu_vcpu_cpsr_update(vcpu, regs, new_cpsr, CPSR_ALLBITS_MASK);
	cpu_vcpu_spsr_update(vcpu, old_cpsr, CPSR_ALLBITS_MASK);
	regs->lr = regs->pc + lr_off;
	regs->pc = new_pc;

	return VMM_OK;
}
