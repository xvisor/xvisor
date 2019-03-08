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
 * @file cpu_vcpu_irq.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling vcpu interrupts
 */

#include <vmm_error.h>
#include <vmm_vcpu_irq.h>
#include <vmm_stdio.h>
#include <arch_vcpu.h>
#include <riscv_csr.h>

u32 arch_vcpu_irq_count(struct vmm_vcpu *vcpu)
{
	return ARCH_BITS_PER_LONG;
}

u32 arch_vcpu_irq_priority(struct vmm_vcpu *vcpu, u32 irq_no)
{
	/* Same priority for all VCPU interrupts */
	return 2;
}

int arch_vcpu_irq_assert(struct vmm_vcpu *vcpu, u32 irq_no, u64 reason)
{
	irq_flags_t f;
	unsigned long irq_mask;

	if (irq_no >= ARCH_BITS_PER_LONG) {
		return VMM_EINVALID;
	}
	irq_mask = 1UL << irq_no;
	if (!(riscv_priv(vcpu)->hideleg & irq_mask)) {
		return VMM_OK;
	}

	vmm_spin_lock_irqsave(&riscv_priv(vcpu)->bsip_lock, f);

	riscv_priv(vcpu)->bsip |= irq_mask;
	if (vcpu == vmm_scheduler_current_vcpu()) {
		csr_write(CSR_BSIP, riscv_priv(vcpu)->bsip);
	}

	vmm_spin_unlock_irqrestore(&riscv_priv(vcpu)->bsip_lock, f);

	return VMM_OK;
}


int arch_vcpu_irq_execute(struct vmm_vcpu *vcpu,
			  arch_regs_t *regs, 
			  u32 irq_no, u64 reason)
{
	irq_flags_t f;
	unsigned long irq_mask;

	if (irq_no >= ARCH_BITS_PER_LONG) {
		return VMM_EINVALID;
	}
	irq_mask = 1UL << irq_no;
	if (!(riscv_priv(vcpu)->hideleg & irq_mask)) {
		return VMM_OK;
	}

	vmm_spin_lock_irqsave(&riscv_priv(vcpu)->bsip_lock, f);
	csr_write(CSR_BSIP, riscv_priv(vcpu)->bsip);
	vmm_spin_unlock_irqrestore(&riscv_priv(vcpu)->bsip_lock, f);

	return VMM_OK;
}

int arch_vcpu_irq_deassert(struct vmm_vcpu *vcpu, u32 irq_no, u64 reason)
{
	irq_flags_t f;
	unsigned long irq_mask;

	if (irq_no >= ARCH_BITS_PER_LONG) {
		return VMM_EINVALID;
	}
	irq_mask = 1UL << irq_no;
	if (!(riscv_priv(vcpu)->hideleg & irq_mask)) {
		return VMM_OK;
	}

	vmm_spin_lock_irqsave(&riscv_priv(vcpu)->bsip_lock, f);

	riscv_priv(vcpu)->bsip &= ~irq_mask;
	if (vcpu == vmm_scheduler_current_vcpu()) {
		csr_write(CSR_BSIP, riscv_priv(vcpu)->bsip);
	}

	vmm_spin_unlock_irqrestore(&riscv_priv(vcpu)->bsip_lock, f);

	return VMM_OK;
}

bool arch_vcpu_irq_pending(struct vmm_vcpu *vcpu)
{
	bool ret;
	irq_flags_t f;

	vmm_spin_lock_irqsave(&riscv_priv(vcpu)->bsip_lock, f);
	ret = (riscv_priv(vcpu)->bsip) ? TRUE : FALSE;
	vmm_spin_unlock_irqrestore(&riscv_priv(vcpu)->bsip_lock, f);

	return ret;
}
