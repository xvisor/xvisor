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
 * @file vmm_semaphore.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of sempahore locks for Orphan VCPU (or Thread).
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_semaphore.h>

bool vmm_semaphore_avail(struct vmm_semaphore *sem)
{
	bool ret;
	irq_flags_t flags;

	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	vmm_spin_lock_irqsave(&sem->wq.lock, flags);

	ret = (sem->value) ? TRUE : FALSE;

	vmm_spin_unlock_irqrestore(&sem->wq.lock, flags);

	return ret;
}

u32 vmm_semaphore_limit(struct vmm_semaphore *sem)
{
	u32 ret;
	irq_flags_t flags;

	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	vmm_spin_lock_irqsave(&sem->wq.lock, flags);

	ret = sem->limit;

	vmm_spin_unlock_irqrestore(&sem->wq.lock, flags);

	return ret;
}

int vmm_semaphore_up(struct vmm_semaphore *sem)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	vmm_spin_lock_irqsave(&sem->wq.lock, flags);

	if (sem->value < sem->limit) {
		sem->value++;
		rc = __vmm_waitqueue_wakeall(&sem->wq);
	}

	vmm_spin_unlock_irqrestore(&sem->wq.lock, flags);

	return rc;
}

static int semaphore_down_common(struct vmm_semaphore *sem, u64 *timeout)
{
	int rc = VMM_OK;

	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	vmm_spin_lock_irq(&sem->wq.lock);

	if (!sem->value) {
		rc = __vmm_waitqueue_sleep(&sem->wq, timeout);
		if((timeout != NULL) && (*timeout == 0)) {
			sem->value++;
		}
	}
	sem->value--;

	vmm_spin_unlock_irq(&sem->wq.lock);

	return rc;
}

int vmm_semaphore_down(struct vmm_semaphore *sem)
{
	return semaphore_down_common(sem, NULL);
}

int vmm_semaphore_down_timeout(struct vmm_semaphore *sem, u64 *timeout)
{
	return semaphore_down_common(sem, timeout);
}

