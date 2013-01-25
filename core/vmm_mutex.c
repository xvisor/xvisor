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
 * @file vmm_mutex.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of mutext locks for Orphan VCPU (or Thread).
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_mutex.h>

bool vmm_mutex_avail(struct vmm_mutex *mut)
{
	bool ret;
	irq_flags_t flags;

	BUG_ON(!mut);

	vmm_spin_lock_irqsave(&mut->wq.lock, flags);

	ret = (mut->lock) ? FALSE : TRUE;

	vmm_spin_unlock_irqrestore(&mut->wq.lock, flags);

	return ret;
}

struct vmm_vcpu *vmm_mutex_owner(struct vmm_mutex *mut)
{
	struct vmm_vcpu *ret;
	irq_flags_t flags;

	BUG_ON(!mut);

	vmm_spin_lock_irqsave(&mut->wq.lock, flags);

	ret = mut->owner;

	vmm_spin_unlock_irqrestore(&mut->wq.lock, flags);

	return ret;
}

int vmm_mutex_unlock(struct vmm_mutex *mut)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	BUG_ON(!mut);
	BUG_ON(!vmm_scheduler_orphan_context());

	vmm_spin_lock_irqsave(&mut->wq.lock, flags);

	if (mut->lock && mut->owner == vmm_scheduler_current_vcpu()) {
		mut->lock = 0;
		mut->owner = NULL;
		rc = __vmm_waitqueue_wakeall(&mut->wq);
	}

	vmm_spin_unlock_irqrestore(&mut->wq.lock, flags);

	return rc;
}

static int mutex_lock_common(struct vmm_mutex *mut, u64 *timeout)
{
	int rc = VMM_OK;

	BUG_ON(!mut);
	BUG_ON(!vmm_scheduler_orphan_context());

	vmm_spin_lock_irq(&mut->wq.lock);

	while (mut->lock) {
		rc = __vmm_waitqueue_sleep(&mut->wq, timeout);
		if (rc) {
			/* Timeout or some other failure */
			break;
		}
	}
	if (rc == VMM_OK) {
		mut->lock = 1;
		mut->owner = vmm_scheduler_current_vcpu();
	}

	vmm_spin_unlock_irq(&mut->wq.lock);

	return rc;
}

int vmm_mutex_lock(struct vmm_mutex *mut)
{
	return mutex_lock_common(mut, NULL);
}

int vmm_mutex_lock_timeout(struct vmm_mutex *mut, u64 *timeout)
{
	return mutex_lock_common(mut, timeout);
}

