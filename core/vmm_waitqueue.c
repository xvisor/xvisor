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
#include <vmm_timer.h>
#include <vmm_waitqueue.h>
#include <arch_cpu_irq.h>

u32 vmm_waitqueue_count(struct vmm_waitqueue *wq) 
{
	BUG_ON(!wq);

	return wq->vcpu_count;
}

static void waitqueue_timeout(struct vmm_timer_event *event)
{
	struct vmm_vcpu *vcpu = event->priv;

	vmm_waitqueue_wake(vcpu);
}

int __vmm_waitqueue_sleep(struct vmm_waitqueue *wq, u64 *timeout_nsecs)
{
	int rc = VMM_OK;
	struct vmm_vcpu *vcpu;
	struct vmm_timer_event wake_event;

	/* Sanity checks */
	BUG_ON(!wq);
	BUG_ON(!vmm_scheduler_orphan_context());

	if (timeout_nsecs && (*timeout_nsecs == 0)) {
		return VMM_ETIMEDOUT;
	}

	/* Get current VCPU */
	vcpu = vmm_scheduler_current_vcpu();

	/* Add VCPU to waitqueue */
	list_add_tail(&vcpu->wq_head, &wq->vcpu_list);

	/* Increment VCPU count in waitqueue */
	wq->vcpu_count++;

	/* Update VCPU waitqueue context */
	vcpu->wq_priv = wq;

	/* If timeout is required then create timer event */
	if (timeout_nsecs) {
		INIT_TIMER_EVENT(&wake_event, &waitqueue_timeout, vcpu);
		vmm_timer_event_start(&wake_event, *timeout_nsecs);
	}

	/* Unlock waitqueue */
	vmm_spin_unlock_irq(&wq->lock);

	/* Try to Pause VCPU */
	rc = vmm_manager_vcpu_pause(vcpu);

	/* Lock waitqueue */
	vmm_spin_lock_irq(&wq->lock);

	/* Remove VCPU from waitqueue */
	list_del(&vcpu->wq_head);

	/* Decrement VCPU count in waitqueue */
	if (wq->vcpu_count) {
		wq->vcpu_count--;
	}

	/* Set VCPU waitqueue context to NULL */
	vcpu->wq_priv = NULL;

	if (rc) {
		/* Failed to pause VCPU so remove from waitqueue */
		/* Destroy timeout event */
		if (timeout_nsecs) {
			vmm_timer_event_stop(&wake_event);
		}
	} else {
		/* VCPU Wakeup so remove from waitqueue */
		/* If timeout was used than destroy timer event */
		if (timeout_nsecs) {
			u64 now, expiry;
			expiry = wake_event.expiry_tstamp;
			vmm_timer_event_stop(&wake_event);
			now = vmm_timer_timestamp();
			*timeout_nsecs = (now > expiry) ? 0 : (expiry - now);
			if (*timeout_nsecs == 0) {
				rc = VMM_ETIMEDOUT;
			}
		}
	}

	return rc;
}

int vmm_waitqueue_sleep(struct vmm_waitqueue *wq)
{
	int rc;

	/* Sanity checks */
	BUG_ON(!wq);
	BUG_ON(arch_cpu_irq_disabled());

	/* Lock waitqueue */
	vmm_spin_lock_irq(&wq->lock);

	/* Put VCPU to sleep without timeout */
	rc = __vmm_waitqueue_sleep(wq, NULL);

	/* Unlock waitqueue */
	vmm_spin_unlock_irq(&wq->lock);

	return rc;
}

int vmm_waitqueue_sleep_timeout(struct vmm_waitqueue *wq, u64 *timeout_usecs)
{
	int rc;

	/* Sanity checks */
	BUG_ON(!wq);
	BUG_ON(arch_cpu_irq_disabled());

	/* Lock waitqueue */
	vmm_spin_lock_irq(&wq->lock);

	/* Put VCPU to sleep with timeout */
	rc = __vmm_waitqueue_sleep(wq, timeout_usecs);

	/* Unlock waitqueue */
	vmm_spin_unlock_irq(&wq->lock);

	return rc;
}

int vmm_waitqueue_forced_remove(struct vmm_vcpu *vcpu)
{
	irq_flags_t flags;
	struct vmm_waitqueue *wq = vcpu->wq_priv;

	/* Sanity check */
	if (!wq) {
		return VMM_EFAIL;
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

	return VMM_OK;
}

int vmm_waitqueue_wake(struct vmm_vcpu *vcpu)
{
	int rc = VMM_OK;

	/* Sanity checks */
	if (!vcpu || !vcpu->wq_priv) {
		return VMM_EFAIL;
	}

	/* Try to Resume VCPU */
	if ((rc = vmm_manager_vcpu_resume(vcpu))) {
		return rc;
	}

	return VMM_OK;
}

int __vmm_waitqueue_wakefirst(struct vmm_waitqueue *wq)
{
	int rc = VMM_OK;
	struct dlist *l;
	struct vmm_vcpu *vcpu;

	/* Sanity checks */
	BUG_ON(!wq);

	/* We should have atleast one VCPU in waitqueue list */
	if (!wq->vcpu_count) {
		return VMM_ENOENT;
	}

	/* Get first VCPU from waitqueue list */
	l = list_first(&wq->vcpu_list);
	vcpu = list_entry(l, struct vmm_vcpu, wq_head);

	/* Try to Resume VCPU */
	if ((rc = vmm_manager_vcpu_resume(vcpu))) {
		/* Return Failure */
		return rc;
	}

	return VMM_OK;
}

int vmm_waitqueue_wakefirst(struct vmm_waitqueue *wq)
{
	int rc;
	irq_flags_t flags;

	/* Sanity checks */
	BUG_ON(!wq);

	/* Lock waitqueue */
	vmm_spin_lock_irqsave(&wq->lock, flags);

	/* Wakeup first VCPU from waitqueue list */
	rc = __vmm_waitqueue_wakefirst(wq);

	/* Unlock waitqueue */
	vmm_spin_unlock_irqrestore(&wq->lock, flags);

	return rc;
}

int __vmm_waitqueue_wakeall(struct vmm_waitqueue *wq)
{
	int rc;
	struct dlist *l;
	struct vmm_vcpu *vcpu;

	/* Sanity checks */
	BUG_ON(!wq);

	/* We should have atleast one VCPU in waitqueue list */
	if (!wq->vcpu_count) {
		return VMM_ENOENT;
	}

	/* Try resume every VCPU till empty */
	list_for_each(l, &wq->vcpu_list) {
		vcpu = list_entry(l, struct vmm_vcpu, wq_head);

		/* Try to Resume VCPU */
		if ((rc = vmm_manager_vcpu_resume(vcpu))) {
			/* Return Failure */
			return rc;
		}
	}

	return VMM_OK;
}

int vmm_waitqueue_wakeall(struct vmm_waitqueue *wq)
{
	int rc;
	irq_flags_t flags;

	/* Sanity checks */
	BUG_ON(!wq);

	/* Lock waitqueue */
	vmm_spin_lock_irqsave(&wq->lock, flags);

	/* Wake every first VCPU till empty */
	rc = __vmm_waitqueue_wakeall(wq);

	/* Unlock waitqueue */
	vmm_spin_unlock_irqrestore(&wq->lock, flags);

	return rc;
}

