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
 * @file vmm_semaphore.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of sempahore locks for Orphan VCPU (or Thread).
 */

#include <arch_atomic.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_semaphore.h>

bool vmm_semaphore_avail(struct vmm_semaphore * sem)
{
	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	return arch_atomic_read(&(sem)->value) ? TRUE : FALSE;
}

u32 vmm_semaphore_limit(struct vmm_semaphore * sem)
{
	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	return sem->limit;
}

int vmm_semaphore_up(struct vmm_semaphore * sem)
{
	int rc;
	u32 value;

	/* Sanity Check */
	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);

	/* Try to increment the semaphore */
	rc = VMM_EFAIL;
	value = arch_atomic_read(&sem->value);
	while ((value < sem->limit) && 
		(rc = arch_atomic_testnset(&sem->value, value, value + 1))) {
		value = arch_atomic_read(&sem->value);
	}

	/* If successful then wakeup all sleeping threads */
	if (!rc) {
		vmm_waitqueue_wakeall(&sem->wq);
	}

	return rc;
}

int vmm_semaphore_down(struct vmm_semaphore * sem)
{
	int rc;
	u32 value;

	/* Sanity Check */
	BUG_ON(!sem, "%s: NULL poniter to semaphore\n", __func__);
	BUG_ON(!vmm_scheduler_orphan_context(), 
		"%s: Down allowed in Orphan VCPU (or Thread) context only\n",
		 __func__);

	/* Decrement the semaphore */
	rc = VMM_EFAIL;
	while (rc) {
		/* Sleep if semaphore not available */
		while (!(value = arch_atomic_read(&sem->value))) {
			vmm_waitqueue_sleep(&sem->wq);
		}

		/* Try to decrement the semaphore */
		rc = arch_atomic_testnset(&sem->value, value, value - 1);
	}

	return rc;
}

