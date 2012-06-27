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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementaion of Orphan VCPU (or Thread) wait queue. 
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_scheduler.h>
#include <vmm_waitqueue.h>

u32 vmm_waitqueue_count(struct vmm_waitqueue * wq) 
{
	BUG_ON(!wq, "%s: NULL poniter to waitqueue\n", __func__);

	return wq->vcpu_count;
}

static void waitqueue_timeout(struct vmm_timer_event * event)
{
	struct vmm_vcpu *vcpu = event->priv;

	vmm_waitqueue_wake(vcpu);
}

static int __vmm_waitqueue_sleep(struct vmm_waitqueue * wq, u64 * timeout_nsecs)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct vmm_vcpu * vcpu;
	struct vmm_timer_event *wake_event;

	/* Sanity checks */
	BUG_ON(!wq, "%s: NULL poniter to waitqueue\n", __func__);
	BUG_ON(!vmm_scheduler_orphan_context(), 
		"%s: Sleep allowed in Orphan VCPU (or Thread) context only\n",
		 __func__);

	/* Get current VCPU */
	vcpu = vmm_scheduler_current_vcpu();

	/* Lock waitqueue */
	vmm_spin_lock_irqsave(&wq->lock, flags);
	
	/* Add VCPU to waitqueue */
	list_add_tail(&wq->vcpu_list, &vcpu->wq_head);

	/* Increment VCPU count in waitqueue */
	wq->vcpu_count++;

	/* Update VCPU waitqueue context */
	vcpu->wq_priv = wq;

	/* If timeout is required then create timer event */
	if (timeout_nsecs) {
		wake_event = vmm_timer_event_create(&waitqueue_timeout, vcpu);
		vmm_timer_event_start(wake_event, *timeout_nsecs);
	}

	/* Unlock waitqueue */
	vmm_spin_unlock_irqrestore(&wq->lock, flags);
	
	/* Try to Pause VCPU */
	if ((rc = vmm_manager_vcpu_pause(vcpu))) {
		/* Failed to pause VCPU so remove from waitqueue */

		if(timeout_nsecs) {
			vmm_timer_event_destroy(wake_event);
		}

		/* Lock waitqueue */
		vmm_spin_lock_irqsave(&wq->lock, flags);

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

	/* If timeout was used than destroy timer event */
	if(timeout_nsecs) {
		u64 now, expiry;
		expiry = wake_event->expiry_tstamp;
		vmm_timer_event_destroy(wake_event);
		now = vmm_timer_timestamp();
		*timeout_nsecs = (now > expiry) ? 0 : (expiry - now);
	}

	return VMM_OK;
}

int vmm_waitqueue_sleep(struct vmm_waitqueue * wq) 
{
	return __vmm_waitqueue_sleep(wq, NULL);
}

int vmm_waitqueue_sleep_timeout(struct vmm_waitqueue * wq, u64 * timeout_usecs)
{
	return __vmm_waitqueue_sleep(wq, timeout_usecs);
}

int vmm_waitqueue_wake(struct vmm_vcpu * vcpu)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct vmm_waitqueue * wq;

	/* Sanity checks */
	if (!vcpu || !vcpu->wq_priv) {
		return VMM_EFAIL;
	}

	/* Get the waitqueue */
	wq = vcpu->wq_priv;

	/* Lock waitqueue */
	vmm_spin_lock_irqsave(&wq->lock, flags);
	
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

int vmm_waitqueue_wakefirst(struct vmm_waitqueue * wq)
{
	int rc = VMM_OK;
	struct dlist *l;
	struct vmm_vcpu * vcpu;
	irq_flags_t flags;

	/* Sanity checks */
	BUG_ON(!wq, "%s: NULL poniter to waitqueue\n", __func__);

	/* Lock waitqueue */
	vmm_spin_lock_irqsave(&wq->lock, flags);

	/* Get first VCPU from waitqueue list */
	l = list_pop(&wq->vcpu_list);
	vcpu = list_entry(l, struct vmm_vcpu, wq_head);

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

int vmm_waitqueue_wakeall(struct vmm_waitqueue * wq)
{
	int i, wake_count, rc = VMM_OK;
	struct dlist *l;
	struct vmm_vcpu * vcpu;
	irq_flags_t flags;

	/* Sanity checks */
	BUG_ON(!wq, "%s: NULL poniter to waitqueue\n", __func__);

	/* Lock waitqueue */
	vmm_spin_lock_irqsave(&wq->lock, flags);

	/* For each VCPU in waitqueue */
	wake_count = 0;
	for (i = 0; i < wq->vcpu_count; i++) {
		/* Get VCPU from waitqueue list */
		l = list_pop(&wq->vcpu_list);
		vcpu = list_entry(l, struct vmm_vcpu, wq_head);

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

