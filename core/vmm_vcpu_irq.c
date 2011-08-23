/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_vcpu_irq.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for vcpu irq processing
 */

#include <vmm_cpu.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_scheduler.h>
#include <vmm_devtree.h>
#include <vmm_vcpu_irq.h>

void vmm_vcpu_irq_process(vmm_user_regs_t * regs)
{
	u32 irq_reason;
	s32 irq_no, irq_priority, act_first, act_int, act_priority;
	vmm_vcpu_t *vcpu = vmm_scheduler_current_vcpu();

	/* Sanity Checks */
	if (vcpu == NULL) {
		return;
	}

	/* For non-normal vcpu dont do anything */
	if (!vcpu->is_normal) {
		return;
	}

	/* Emulate a pending irq for current vcpu */
	irq_no = vcpu->irqs->pending_first;
	act_first = vcpu->irqs->active_first;
	act_int = (-1 < act_first) ? vcpu->irqs->active[act_first] : -1;
	act_priority = (-1 < act_int) ? 
				vmm_vcpu_irq_priority(vcpu, act_int) : -1;
	if (irq_no != -1) {
		irq_reason = vcpu->irqs->reason[irq_no];
		irq_priority = vmm_vcpu_irq_priority(vcpu, irq_no);
		if (act_first == -1 || act_priority > irq_priority) {
			if (vmm_vcpu_irq_execute(vcpu, regs, irq_no, irq_reason)
			    == VMM_OK) {
				vcpu->irqs->pending_first =
				    vcpu->irqs->pending[irq_no];
				vcpu->irqs->pending[irq_no] = -1;
				act_first++;
				vcpu->irqs->active_first = act_first;
				vcpu->irqs->active[act_first] = irq_no;
			}
		}
	}
}

void vmm_vcpu_irq_assert(vmm_vcpu_t *vcpu, u32 irq_no, u32 reason)
{
	u32 irq_count = vmm_vcpu_irq_count(vcpu);
	s32 irq_prev, irq_curr, irq_curr_prio, irq_prio;

	/* Sanity Checks */
	if (vcpu == NULL) {
		return;
	}
	if (irq_count <= irq_no) {
		return;
	}

	/* For non-normal vcpu dont do anything */
	if (!vcpu->is_normal) {
		return;
	}

	/* Locate insertion postion for asserted irq */
	irq_prio = vmm_vcpu_irq_priority(vcpu, irq_no);
	irq_prev = -1;
	irq_curr = vcpu->irqs->pending_first;
	while (irq_curr != -1) {
		if (irq_no == irq_curr) {
			return;
		}
		irq_curr_prio = vmm_vcpu_irq_priority(vcpu, irq_curr);
		if (irq_prio < irq_curr_prio) {
			break;
		}
		irq_prev = irq_curr;
		irq_curr = vcpu->irqs->pending[irq_curr];
	}

	/* Add the asserted irq to correct position */
	vcpu->irqs->reason[irq_no] = reason;
	vcpu->irqs->pending[irq_no] = irq_curr;
	if (irq_prev != -1) {
		vcpu->irqs->pending[irq_prev] = irq_no;
	} else {
		vcpu->irqs->pending_first = irq_no;
	}
}

void vmm_vcpu_irq_deassert(vmm_vcpu_t *vcpu)
{
	s32 act_first;

	/* Sanity check */
	if (!vcpu) {
		return;
	}

	/* For non-normal vcpu dont do anything */
	if (!vcpu->is_normal) {
		return;
	}

	/* Deassert current active irq */
	act_first = vcpu->irqs->active_first;
	if (-1 < act_first) {
		vcpu->irqs->active[act_first] = -1;
		act_first--;
		vcpu->irqs->active_first = act_first;
	}
}

int vmm_vcpu_irq_init(vmm_vcpu_t *vcpu)
{
	u32 ite, irq_count = vmm_vcpu_irq_count(vcpu);

	/* Only first time */
	if (!vcpu->reset_count) {
		/* Clear the memory of irq */
		vmm_memset(vcpu->irqs, 0, sizeof(vmm_vcpu_irqs_t));

		/* Allocate memory for arrays */
		vcpu->irqs->reason = vmm_malloc(sizeof(s32 *) * irq_count);
		vcpu->irqs->pending = vmm_malloc(sizeof(s32 *) * irq_count);
		vcpu->irqs->active = vmm_malloc(sizeof(s32 *) * irq_count);
	}

	/* Reset irq processing data structures for VCPU */
	for (ite = 0; ite < irq_count; ite++) {
		vcpu->irqs->reason[ite] = 0;
		vcpu->irqs->pending[ite] = -1;
		vcpu->irqs->active[ite] = -1;
	}
	vcpu->irqs->pending_first = -1;
	vcpu->irqs->active_first = -1;

	return VMM_OK;
}

