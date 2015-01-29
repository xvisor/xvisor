/**
 * Copyright (c) 2011 Himanshu Chauhan
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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source code for handling vcpu interrupts
 */

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_vcpu_irq.h>
#include <cpu_vm.h>
#include <arch_guest_helper.h>

u32 arch_vcpu_irq_count(struct vmm_vcpu *vcpu)
{
	return 256;
}

u32 arch_vcpu_irq_priority(struct vmm_vcpu *vcpu, u32 irq_no)
{
	/* all at same priority */
	return 1;
}

/*
 * NOTE:
 *
 * arch_vcpu_irq_XXassert has to be called by the last PIC in the
 * chain. For example, if software has configured 8259 along with
 * LAPIC, this function should be finally called by LAPIC. All other
 * PICs should become slave of LAPIC.
 */
int arch_vcpu_irq_assert(struct vmm_vcpu *vcpu, u32 irq_no, u64 reason)
{
	struct x86_vcpu_priv *vcpu_priv = NULL;
	vcpu_priv = x86_vcpu_priv(vcpu);

	mark_guest_interrupt_pending(vcpu_priv->hw_context, irq_no);

	return VMM_OK;
}

/* FIXME: */
int arch_vcpu_irq_execute(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs, 
			 u32 irq_no, u64 reason)
{
	return VMM_OK;
}

int arch_vcpu_irq_deassert(struct vmm_vcpu *vcpu, u32 irq_no, u64 reason)
{
	struct x86_vcpu_priv *vcpu_priv = NULL;
	irq_flags_t flags;
	vcpu_priv = x86_vcpu_priv(vcpu);

	vmm_spin_lock_irqsave_lite(&vcpu_priv->lock, flags);

	if (vcpu_priv->int_pending != irq_no) {
		vmm_printf("%s: WARNING!!! IRQ %d on vcpu %s not active to deassert! Currently active: %d\n",
			   __func__, irq_no, vcpu->name, vcpu_priv->int_pending);
		vmm_spin_unlock_irqrestore_lite(&vcpu_priv->lock, flags);
		return VMM_EFAIL;
	}

	x86_vcpu_priv(vcpu)->int_pending = irq_no;
	vmm_spin_unlock_irqrestore_lite(&vcpu_priv->lock, flags);

	return VMM_OK;
}

