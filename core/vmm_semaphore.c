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
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_semaphore.h>
#include <arch_cpu_irq.h>

struct vmm_semaphore_resource {
	struct dlist head;
	u32 count;
	struct vmm_semaphore *sem;
	struct vmm_vcpu *vcpu;
	struct vmm_vcpu_resource res;
};

/* Note: This function must be called with semaphore waitqueue lock held */
static struct vmm_semaphore_resource *__semaphore_find_resource(
			struct vmm_semaphore *sem, struct vmm_vcpu *vcpu)
{
	bool found = FALSE;
	struct vmm_semaphore_resource *sres;

	list_for_each_entry(sres, &sem->res_list, head) {
		if (sres->vcpu == vcpu) {
			found = TRUE;
			break;
		}
	}

	return (found) ? sres : NULL;
}

/* Note: This function must be called with semaphore waitqueue lock held */
static struct vmm_semaphore_resource *__semaphore_first_resource(
						struct vmm_semaphore *sem)
{
	if (list_empty(&sem->res_list))
		return NULL;
	return list_first_entry(&sem->res_list,
				struct vmm_semaphore_resource, head);
}

static void __vmm_semaphore_cleanup(struct vmm_vcpu *vcpu,
				    struct vmm_vcpu_resource *vcpu_res)
{
	irq_flags_t flags;
	bool wake_all = FALSE;
	struct vmm_semaphore_resource *sres =
		container_of(vcpu_res, struct vmm_semaphore_resource, res);
	struct vmm_semaphore *sem = sres->sem;

	if (!sres || !sem || (sres->vcpu != vcpu)) {
		return;
	}

	vmm_spin_lock_irqsave(&sem->wq.lock, flags);

	if (sres->count) {
		sem->value += sres->count;
		if (sem->value > sem->limit)
			sem->value = sem->limit;
		sres->count = 0;
		wake_all = TRUE;
	}

	list_del(&sres->head);
	vmm_free(sres);

	if (wake_all) {
		__vmm_waitqueue_wakeall(&sem->wq);
	}

	vmm_spin_unlock_irqrestore(&sem->wq.lock, flags);
}

u32 vmm_semaphore_avail(struct vmm_semaphore *sem)
{
	u32 ret;
	irq_flags_t flags;

	BUG_ON(!sem);

	vmm_spin_lock_irqsave(&sem->wq.lock, flags);

	ret = sem->value;

	vmm_spin_unlock_irqrestore(&sem->wq.lock, flags);

	return ret;
}

u32 vmm_semaphore_limit(struct vmm_semaphore *sem)
{
	u32 ret;
	irq_flags_t flags;

	BUG_ON(!sem);

	vmm_spin_lock_irqsave(&sem->wq.lock, flags);

	ret = sem->limit;

	vmm_spin_unlock_irqrestore(&sem->wq.lock, flags);

	return ret;
}

int vmm_semaphore_up(struct vmm_semaphore *sem)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct vmm_vcpu *current_vcpu = vmm_scheduler_current_vcpu();
	struct vmm_semaphore_resource *sres;

	BUG_ON(!sem);
	BUG_ON(!sem->limit);

	vmm_spin_lock_irqsave(&sem->wq.lock, flags);

	if (sem->value < sem->limit) {
		sem->value++;

		sres = __semaphore_find_resource(sem, current_vcpu);
		if (!sres) {
			sres = __semaphore_first_resource(sem);
		}
		if (sres) {
			if (sres->count) {
				sres->count--;
			}
			if (!sres->count) {
				vmm_manager_vcpu_resource_remove(sres->vcpu,
								 &sres->res);
				list_del(&sres->head);
				vmm_free(sres);
			}
		}

		rc = __vmm_waitqueue_wakeall(&sem->wq);
		if (rc == VMM_ENOENT) {
			rc = VMM_OK;
		}
	}

	vmm_spin_unlock_irqrestore(&sem->wq.lock, flags);

	return rc;
}

static int semaphore_down_common(struct vmm_semaphore *sem, u64 *timeout)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct vmm_vcpu *current_vcpu = vmm_scheduler_current_vcpu();
	struct vmm_semaphore_resource *sres;

	BUG_ON(!sem);
	BUG_ON(!sem->limit);
	BUG_ON(!vmm_scheduler_orphan_context());

	vmm_spin_lock_irqsave(&sem->wq.lock, flags);

	while (!sem->value) {
		rc = __vmm_waitqueue_sleep(&sem->wq, timeout);
		if (rc) {
			/* Timeout or some other failure */
			break;
		}
	}
	if (rc == VMM_OK) {
		sres = __semaphore_find_resource(sem, current_vcpu);
		if (!sres) {
			sres = vmm_zalloc(sizeof(*sres));
			BUG_ON(!sres);
			INIT_LIST_HEAD(&sres->head);
			sres->count = 0;
			sres->sem = sem;
			sres->vcpu = current_vcpu;
			sres->res.name = "vmm_semaphore";
			sres->res.cleanup = __vmm_semaphore_cleanup;
			list_add_tail(&sres->head, &sem->res_list);
			vmm_manager_vcpu_resource_add(current_vcpu,
						      &sres->res);
		}
		sres->count++;
		sem->value--;
	}

	vmm_spin_unlock_irqrestore(&sem->wq.lock, flags);

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

