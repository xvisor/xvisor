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
 * @file vmm_completion.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of completion events for Orphan VCPU (or Thread).
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_completion.h>
#include <arch_cpu_irq.h>

bool vmm_completion_done(struct vmm_completion *cmpl)
{
	bool ret = TRUE;
	irq_flags_t flags;

	BUG_ON(!cmpl);

	vmm_spin_lock_irqsave(&cmpl->wq.lock, flags);

	if (!cmpl->done) {
		ret = FALSE;
	}

	vmm_spin_unlock_irqrestore(&cmpl->wq.lock, flags);

	return ret;
}

static int completion_wait_common(struct vmm_completion *cmpl, u64 *timeout)
{
	int rc = VMM_OK;

	BUG_ON(!cmpl);
	BUG_ON(!vmm_scheduler_orphan_context());

	vmm_spin_lock_irq(&cmpl->wq.lock);

	if (!cmpl->done) {
		rc = __vmm_waitqueue_sleep(&cmpl->wq, timeout);
	}
	if (cmpl->done) {
		cmpl->done--;
	}

	vmm_spin_unlock_irq(&cmpl->wq.lock);

	return rc;
}

int vmm_completion_wait(struct vmm_completion *cmpl)
{
	return completion_wait_common(cmpl, NULL);
}

int vmm_completion_wait_timeout(struct vmm_completion *cmpl, u64 *timeout)
{
	return completion_wait_common(cmpl, timeout);
}

int vmm_completion_complete(struct vmm_completion *cmpl)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	BUG_ON(!cmpl);

	vmm_spin_lock_irqsave(&cmpl->wq.lock, flags);

	cmpl->done++;
	rc = __vmm_waitqueue_wakefirst(&cmpl->wq);

	vmm_spin_unlock_irqrestore(&cmpl->wq.lock, flags);

	return rc;
}

int vmm_completion_complete_once(struct vmm_completion *cmpl)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	BUG_ON(!cmpl);

	vmm_spin_lock_irqsave(&cmpl->wq.lock, flags);

	if (!cmpl->done) {
		cmpl->done++;
		rc = __vmm_waitqueue_wakefirst(&cmpl->wq);
	}

	vmm_spin_unlock_irqrestore(&cmpl->wq.lock, flags);

	return rc;
}

int vmm_completion_complete_all(struct vmm_completion *cmpl)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	BUG_ON(!cmpl);

	vmm_spin_lock_irqsave(&cmpl->wq.lock, flags);

	cmpl->done += 0xFFFFFFFFUL / 2;
	rc = __vmm_waitqueue_wakeall(&cmpl->wq);

	vmm_spin_unlock_irqrestore(&cmpl->wq.lock, flags);

	return rc;
}

