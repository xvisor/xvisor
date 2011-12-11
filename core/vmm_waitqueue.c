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
 * @file vmm_waitqueue.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementaion of Orphan VCPU (or Thread) wait queue. 
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_waitqueue.h>

int vmm_waitqueue_sleep(vmm_waitqueue_t * wq) 
{
	int rc = VMM_OK;
	irq_flags_t flags;
	vmm_vcpu_t * vcpu;

	/* Sanity checks */
	if (!wq) {
		return VMM_EFAIL;
	}
	if (!vmm_scheduler_orphan_context()) {
		BUG_ON("%s: Sleep allowed in Orphan VCPU (or Thread)"
		       " context only\n", __func__);
	}

	/* Get current VCPU */
	vcpu = vmm_scheduler_current_vcpu();

	/* Lock waitqueue */
	flags = vmm_spin_lock_irqsave(&wq->lock);
	
	/* Add VCPU to waitqueue */
	list_add_tail(&wq->vcpu_list, &vcpu->wq_head);

	/* Increment VCPU count in waitqueue */
	wq->vcpu_count++;

	/* Update VCPU waitqueue context */
	vcpu->wq_priv = wq;

	/* Unlock waitqueue */
	vmm_spin_unlock_irqrestore(&wq->lock, flags);
	
	/* Try to Pause VCPU */
	if ((rc = vmm_manager_vcpu_pause(vcpu))) {
		/* Failed to pause VCPU so remove from waitqueue */

		/* Lock waitqueue */
		flags = vmm_spin_lock_irqsave(&wq->lock);

		/* Remove VCPU from waitqueue */
		list_del(&vcpu->wq_head);

		/* Decrement VCPU count in waitqueue */
		if (wq->vcpu_count) {
			wq->vcpu_count--;
		}

		/* Set VCPU waitqueue context to NULL */
		vcpu->wq_priv = NULL;

		/* Unlock waitqueue */
		vmm_spin_unlock_irqrestore(&wq->lock, flags);

		return rc;
	}

	return VMM_OK;
}

int vmm_waitqueue_wake(vmm_vcpu_t * vcpu)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	vmm_waitqueue_t * wq;

	/* Sanity checks */
	if (!vcpu || !vcpu->wq_priv) {
		return VMM_EFAIL;
	}

	/* Get the waitqueue */
	wq = vcpu->wq_priv;

	/* Lock waitqueue */
	flags = vmm_spin_lock_irqsave(&wq->lock);
	
	/* Try to Resume VCPU */
	if ((rc = vmm_manager_vcpu_resume(vcpu))) {
		/* Failed to resume VCPU so do nothing */

		/* Unlock waitqueue */
		vmm_spin_unlock_irqrestore(&wq->lock, flags);

		return rc;
	}

	/* Remove VCPU from waitqueue */
	list_del(&vcpu->wq_head);

	/* Decrement VCPU count in waitqueue */
	if (wq->vcpu_count) {
		wq->vcpu_count--;
	}

	/* Set VCPU waitqueue context to NULL */
	vcpu->wq_priv = NULL;

	/* Unlock waitqueue */
	vmm_spin_unlock_irqrestore(&wq->lock, flags);
	
	return VMM_OK;
}

int vmm_waitqueue_wakefirst(vmm_waitqueue_t * wq)
{
	int rc = VMM_OK;
	struct dlist *l;
	vmm_vcpu_t * vcpu;
	irq_flags_t flags;

	/* Sanity checks */
	if (!wq) {
		return VMM_EFAIL;
	}

	/* Lock waitqueue */
	flags = vmm_spin_lock_irqsave(&wq->lock);

	/* Get first VCPU from waitqueue list */
	l = list_pop(&wq->vcpu_list);
	vcpu = list_entry(l, vmm_vcpu_t, wq_head);

	/* Try to Resume VCPU */
	if ((rc = vmm_manager_vcpu_resume(vcpu))) {
		/* Failed to resume VCPU so continue */
		list_add_tail(&wq->vcpu_list, &vcpu->wq_head);

		/* Unlock waitqueue */
		vmm_spin_unlock_irqrestore(&wq->lock, flags);

		/* Return Failure */
		return rc;
	}

	/* Set VCPU waitqueue context to NULL */
	vcpu->wq_priv = NULL;

	/* Decrement VCPU count in waitqueue */
	wq->vcpu_count--;

	/* Unlock waitqueue */
	vmm_spin_unlock_irqrestore(&wq->lock, flags);
	
	return VMM_OK;
}

int vmm_waitqueue_wakeall(vmm_waitqueue_t * wq)
{
	int i, wake_count, rc = VMM_OK;
	struct dlist *l;
	vmm_vcpu_t * vcpu;
	irq_flags_t flags;

	/* Sanity checks */
	if (!wq) {
		return VMM_EFAIL;
	}

	/* Lock waitqueue */
	flags = vmm_spin_lock_irqsave(&wq->lock);

	/* For each VCPU in waitqueue */
	wake_count = 0;
	for (i = 0; i < wq->vcpu_count; i++) {
		/* Get VCPU from waitqueue list */
		l = list_pop(&wq->vcpu_list);
		vcpu = list_entry(l, vmm_vcpu_t, wq_head);

		/* Try to Resume VCPU */
		if ((rc = vmm_manager_vcpu_resume(vcpu))) {
			/* Failed to resume VCPU so continue */
			list_add_tail(&wq->vcpu_list, &vcpu->wq_head);
			continue;
		}

		/* Set VCPU waitqueue context to NULL */
		vcpu->wq_priv = NULL;

		/* Increment count of VCPUs woken up */
		wake_count++;
	}

	/* Decrement VCPU count in waitqueue */
	wq->vcpu_count -= wake_count;

	/* Unlock waitqueue */
	vmm_spin_unlock_irqrestore(&wq->lock, flags);
	
	return VMM_OK;
}

