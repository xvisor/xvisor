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
 * @file cpu_vcpu_helper.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source of VCPU helper functions
 */

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <vmm_guest_aspace.h>
#include <cpu_mmu.h>
#include <libs/stringlib.h>

extern char _stack_start;

#define VMM_REGION_TYPE_ROM	0
#define VMM_REGION_TYPE_RAM	1

int arch_guest_init(struct vmm_guest * guest)
{
	return VMM_OK;
}

int arch_guest_deinit(struct vmm_guest * guest)
{
	return VMM_OK;
}

int arch_vcpu_init(struct vmm_vcpu *vcpu)
{
	struct x86_64_interrupt_frame *frame;
	u64 stack_start;

	if (!vcpu->is_normal) {
		vcpu->arch_priv = (void *)vmm_malloc(sizeof(struct x86_64_interrupt_frame));
		BUG_ON(!vcpu->arch_priv);

		memset(vcpu->arch_priv, 0, sizeof(struct x86_64_interrupt_frame));
		/* For orphan vcpu */
		stack_start = vcpu->stack_va + vcpu->stack_sz - sizeof(u64);
		frame = (struct x86_64_interrupt_frame *)vcpu->arch_priv;
		frame->rip = vcpu->start_pc;
		frame->rsp = stack_start;
		frame->cs = VMM_CODE_SEG_SEL;
		frame->ss = VMM_DATA_SEG_SEL;
		frame->rflags = (X86_EFLAGS_IF | X86_EFLAGS_PF | X86_EFLAGS_CF);
	} else {
		vmm_panic("Non orphan VCPU intialization not supported yet.\n");
	}

	return VMM_OK;
}

int arch_vcpu_deinit(struct vmm_vcpu * vcpu)
{
	return VMM_OK;
}

void arch_vcpu_switch(struct vmm_vcpu *tvcpu, 
		      struct vmm_vcpu *vcpu,
		      arch_regs_t *regs)
{
	if (!tvcpu) {
		/* first time rescheduling */
		memcpy(regs, &vcpu->regs, sizeof(arch_regs_t));
		struct x86_64_interrupt_frame *ret_frame =
			(struct x86_64_interrupt_frame *)((u64)regs + sizeof(arch_regs_t));
		ret_frame->rip = vcpu->start_pc;
		ret_frame->rsp = vcpu->stack_va;
		ret_frame->cs = VMM_CODE_SEG_SEL;
		ret_frame->ss = VMM_DATA_SEG_SEL;
	} else {
		memcpy(&tvcpu->regs, regs, sizeof(arch_regs_t));
		struct x86_64_interrupt_frame *ret_frame =
			(struct x86_64_interrupt_frame *)((u64)regs + sizeof(arch_regs_t));
		memcpy(tvcpu->arch_priv, ret_frame, sizeof(struct x86_64_interrupt_frame));

		memcpy(regs, &vcpu->regs, sizeof(arch_regs_t));
		memcpy(ret_frame, vcpu->arch_priv, sizeof(struct x86_64_interrupt_frame));
	}
}

void arch_vcpu_regs_dump(struct vmm_vcpu *vcpu) 
{
	BUG_ON(1);
}

void arch_vcpu_stat_dump(struct vmm_vcpu *vcpu) 
{
}
