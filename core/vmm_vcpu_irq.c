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
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for vcpu irq processing
 */

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_scheduler.h>
#include <vmm_devtree.h>
#include <vmm_vcpu_irq.h>

void vmm_vcpu_irq_process(arch_regs_t * regs)
{
	int irq_no;
	u32 i, irq_prio, irq_reas, tmp_prio, irq_count;
	struct vmm_vcpu * vcpu = vmm_scheduler_current_vcpu();

	/* For non-normal vcpu dont do anything */
	if (!vcpu || !vcpu->is_normal) {
		return;
	}

	/* Get irq count */
	irq_count = arch_vcpu_irq_count(vcpu);

	/* Find the irq number to process */
	irq_no = -1;
	irq_prio = 0x0;
	irq_reas = 0x0;
	for (i = 0; i < irq_count; i++) {
		if (vcpu->irqs.assert[i]) {
			tmp_prio = arch_vcpu_irq_priority(vcpu, i);
			if (tmp_prio > irq_prio) {
				irq_no = i;
				irq_prio = tmp_prio;
				irq_reas = vcpu->irqs.reason[irq_no];
			}
		}
	}

	/* If irq number found then execute it */
	if (irq_no != -1) {
		if (arch_vcpu_irq_execute(vcpu, regs, irq_no, irq_reas) == VMM_OK) {
			vcpu->irqs.assert[irq_no] = FALSE;
			vcpu->irqs.execute[irq_no] = TRUE;
			vcpu->irqs.execute_count++;
		}
	}
}

void vmm_vcpu_irq_assert(struct vmm_vcpu *vcpu, u32 irq_no, u32 reason)
{
	/* For non-normal vcpu dont do anything */
	if (!vcpu || !vcpu->is_normal) {
		return;
	}

	if (irq_no > arch_vcpu_irq_count(vcpu)) {
		return;
	}

	/* Assert the irq */
	if (!vcpu->irqs.assert[irq_no]) {
		if (arch_vcpu_irq_assert(vcpu, irq_no, reason) == VMM_OK) {
			vcpu->irqs.reason[irq_no] = reason;
			vcpu->irqs.assert[irq_no] = TRUE;
			vcpu->irqs.assert_count++;
		}
	}

	/* If vcpu was waiting for irq then resume it. */
	if (vcpu->irqs.wait_for_irq) {
		vmm_manager_vcpu_resume(vcpu);
		vcpu->irqs.wait_for_irq = FALSE;
	}
}

void vmm_vcpu_irq_deassert(struct vmm_vcpu *vcpu, u32 irq_no)
{
	u32 reason;

	/* For non-normal vcpu dont do anything */
	if (!vcpu || !vcpu->is_normal) {
		return;
	}

	/* Deassert the irq */
	if (vcpu->irqs.execute[irq_no]) {
		reason = vcpu->irqs.reason[irq_no];
		if (arch_vcpu_irq_deassert(vcpu, irq_no, reason) == VMM_OK) {
			vcpu->irqs.deassert_count++;
		}
	}

	/* Ensure irq is not asserted and not executing */
	vcpu->irqs.reason[irq_no] = 0x0;
	vcpu->irqs.assert[irq_no] = FALSE;
	vcpu->irqs.execute[irq_no] = FALSE;
}

int vmm_vcpu_irq_wait(struct vmm_vcpu *vcpu)
{
	int rc = VMM_EFAIL;

	/* Sanity Checks */
	if (!vcpu || !vcpu->is_normal) {
		return rc;
	}

	/* Pause VCPU only if required */
	if (!(rc = vmm_manager_vcpu_pause(vcpu))) {
		/* Set wait for irq flag */
		vcpu->irqs.wait_for_irq = TRUE;
	}

	return rc;
}

int vmm_vcpu_irq_init(struct vmm_vcpu *vcpu)
{
	u32 ite, irq_count;

	/* Sanity Checks */
	if (!vcpu) {
		return VMM_EFAIL;
	}

	/* For Orphan VCPU just return */
	if (!vcpu->is_normal) {
		return VMM_OK;
	}

	/* Get irq count */
	irq_count = arch_vcpu_irq_count(vcpu);

	/* Only first time */
	if (!vcpu->reset_count) {
		/* Clear the memory of irq */
		vmm_memset(&vcpu->irqs, 0, sizeof(struct vmm_vcpu_irqs));

		/* Allocate memory for arrays */
		vcpu->irqs.assert = vmm_malloc(sizeof(bool) * irq_count);
		vcpu->irqs.execute = vmm_malloc(sizeof(bool) * irq_count);
		vcpu->irqs.reason = vmm_malloc(sizeof(u32) * irq_count);
	}

	/* Set default irq depth */
	vcpu->irqs.depth = 0;

	/* Set default assert & deassert count */
	vcpu->irqs.assert_count = 0;
	vcpu->irqs.execute_count = 0;
	vcpu->irqs.deassert_count = 0;

	/* Reset irq processing data structures for VCPU */
	for (ite = 0; ite < irq_count; ite++) {
		vcpu->irqs.reason[ite] = 0;
		vcpu->irqs.assert[ite] = FALSE;
		vcpu->irqs.execute[ite] = FALSE;
	}

	/* Clear wait for irq flag */
	vcpu->irqs.wait_for_irq = FALSE;

	return VMM_OK;
}
