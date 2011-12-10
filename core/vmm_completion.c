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
 * @file vmm_completion.h
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of completion locks for Orphan VCPU (or Thread).
 */

#include <vmm_error.h>
#include <vmm_completion.h>

bool vmm_completion_done(vmm_completion_t * cmpl)
{
	bool ret = TRUE;
	irq_flags_t flags;

	if (cmpl) {
		/* Lock completion */
		flags = vmm_spin_lock_irqsave(&cmpl->lock);

		if (cmpl->done) {
			ret = FALSE;
		}

		/* Unlock completion */
		vmm_spin_unlock_irqrestore(&cmpl->lock, flags);
	}

	return ret;
}

int vmm_completion_wait(vmm_completion_t * cmpl)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	if (cmpl) {
		/* Lock completion */
		flags = vmm_spin_lock_irqsave(&cmpl->lock);

		/* Wait for completion on waitqueue */
		if (!(rc = vmm_waitqueue_sleep(&cmpl->wq))) {
			cmpl->done++;
		}

		/* Unlock completion */
		vmm_spin_unlock_irqrestore(&cmpl->lock, flags);
	}

	return rc;
}

int vmm_completion_complete_first(vmm_completion_t * cmpl)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	if (cmpl) {
		/* Lock completion */
		flags = vmm_spin_lock_irqsave(&cmpl->lock);

		/* Wake Orphan VCPU (or Thread) waiting for completion */
		if (!(rc = vmm_waitqueue_wakefirst(&cmpl->wq))) {
			if (cmpl->done) {
				cmpl->done--;
			}
		}

		/* Unlock completion */
		vmm_spin_unlock_irqrestore(&cmpl->lock, flags);
	}

	return rc;
}

int vmm_completion_complete_all(vmm_completion_t * cmpl)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	if (cmpl) {
		/* Lock completion */
		flags = vmm_spin_lock_irqsave(&cmpl->lock);

		/* Wake all Orphan VCPUs (or Threads) waiting for completion */
		if (!(rc = vmm_waitqueue_wakeall(&cmpl->wq))) {
			cmpl->done = 0;
		}

		/* Unlock completion */
		vmm_spin_unlock_irqrestore(&cmpl->lock, flags);
	}

	return rc;
}

