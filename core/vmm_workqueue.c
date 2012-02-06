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
 * @file vmm_workqueue.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of workqueues (special worker threads).
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_scheduler.h>
#include <vmm_workqueue.h>

struct vmm_workqueue_ctrl {
	vmm_spinlock_t lock;
	struct dlist wq_list;
	u32 wq_count;
	struct vmm_workqueue * syswq;
};

static struct vmm_workqueue_ctrl wqctrl;

bool vmm_workqueue_work_isnew(struct vmm_work * work)
{
	bool ret = FALSE;
	irq_flags_t flags;

	if (!work) {
		return FALSE;
	}

	flags = vmm_spin_lock_irqsave(&work->lock);

	ret = (work->flags & VMM_WORK_STATE_CREATED) ? TRUE : FALSE;

	vmm_spin_unlock_irqrestore(&work->lock, flags);

	return ret;
}

bool vmm_workqueue_work_inprogress(struct vmm_work * work)
{
	bool ret = FALSE;
	irq_flags_t flags;

	if (!work) {
		return FALSE;
	}

	flags = vmm_spin_lock_irqsave(&work->lock);

	ret = (work->flags & VMM_WORK_STATE_INPROGRESS) ? TRUE : FALSE;

	vmm_spin_unlock_irqrestore(&work->lock, flags);

	return ret;
}

bool vmm_workqueue_work_completed(struct vmm_work * work)
{
	bool ret = FALSE;
	irq_flags_t flags;

	if (!work) {
		return FALSE;
	}

	flags = vmm_spin_lock_irqsave(&work->lock);

	ret = (work->flags & VMM_WORK_STATE_STOPPED) ? TRUE : FALSE;

	vmm_spin_unlock_irqrestore(&work->lock, flags);

	return ret;
}

int vmm_workqueue_stop_work(struct vmm_work * work)
{
	irq_flags_t flags, flags1;

	if (!work) {
		return VMM_EFAIL;
	}

	flags = vmm_spin_lock_irqsave(&work->lock);

	if (work->flags & 
		(VMM_WORK_STATE_INPROGRESS | VMM_WORK_STATE_STOPPED)) {
		vmm_spin_unlock_irqrestore(&work->lock, flags);
		return VMM_OK;
	}

	if (work->wq && (work->flags & VMM_WORK_STATE_SCHEDULED)) {
		flags1 = vmm_spin_lock_irqsave(&(work->wq)->lock);
		list_del(&work->head);
		vmm_spin_unlock_irqrestore(&(work->wq)->lock, flags1);
	}

	work->flags = VMM_WORK_STATE_STOPPED;
	work->wq = NULL;

	vmm_spin_unlock_irqrestore(&work->lock, flags);

	return VMM_OK;
}

struct vmm_thread *vmm_workqueue_get_thread(struct vmm_workqueue * wq)
{
	return (wq) ? wq->thread : NULL;
}

struct vmm_workqueue *vmm_workqueue_index2workqueue(int index)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_workqueue *ret;

	if (index < 0) {
		return NULL;
	}

	ret = NULL;
	found = FALSE;

	flags = vmm_spin_lock_irqsave(&wqctrl.lock);

	list_for_each(l, &wqctrl.wq_list) {
		ret = list_entry(l, struct vmm_workqueue, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_spin_unlock_irqrestore(&wqctrl.lock, flags);

	if (!found) {
		return NULL;
	}

	return ret;
}

u32 vmm_workqueue_count(void)
{
	return wqctrl.wq_count;
}

int vmm_workqueue_flush(struct vmm_workqueue * wq)
{
	int rc;

	if (!wq) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_threads_wakeup(wq->thread))) {
		return rc;
	}

	while (!list_empty(&wq->work_list)) {
		/* We release the processor to let the wq thread do its job */
		vmm_scheduler_yield();
	}

	return VMM_OK;
}

int vmm_workqueue_schedule_work(struct vmm_workqueue * wq, 
				struct vmm_work * work)
{
	irq_flags_t flags, flags1;

	if (!work) {
		return VMM_EFAIL;
	}

	if (!wq) {
		/* For SMP we will have to do load balancing 
		 * among multiple system workqueues */
		wq = wqctrl.syswq;
	}

	flags = vmm_spin_lock_irqsave(&work->lock);

	if ((work->flags != VMM_WORK_STATE_CREATED) &&
	    (work->flags != VMM_WORK_STATE_STOPPED)) {
		vmm_spin_unlock_irqrestore(&work->lock, flags);
		return VMM_EFAIL;
	}

	work->flags = VMM_WORK_STATE_SCHEDULED;
	work->wq = wq;

	flags1 = vmm_spin_lock_irqsave(&wq->lock);
	list_add_tail(&wq->work_list, &work->head);
	vmm_spin_unlock_irqrestore(&wq->lock, flags1);

	vmm_spin_unlock_irqrestore(&work->lock, flags);

	vmm_threads_wakeup(wq->thread);

	return VMM_OK;
}

static int workqueue_main(void *data)
{
	bool do_work;
	irq_flags_t flags;
	struct dlist * l;
	struct vmm_workqueue * wq = data;
	struct vmm_work * work = NULL;

	if (!wq) {
		return VMM_EFAIL;
	}

	while (1) {
		flags = vmm_spin_lock_irqsave(&wq->lock);
		
		while (!list_empty(&wq->work_list)) {
			l = list_pop(&wq->work_list);
			vmm_spin_unlock_irqrestore(&wq->lock, flags);

			work = list_entry(l, struct vmm_work, head);
			do_work = FALSE;
			flags = vmm_spin_lock_irqsave(&work->lock);
			if (work->flags & VMM_WORK_STATE_SCHEDULED) {
				work->flags = VMM_WORK_STATE_INPROGRESS;
				do_work = TRUE;
			}
			vmm_spin_unlock_irqrestore(&work->lock, flags);

			if (do_work) {
				work->func(work);
				flags = vmm_spin_lock_irqsave(&work->lock);
				work->flags = VMM_WORK_STATE_STOPPED;
				vmm_spin_unlock_irqrestore(&work->lock, flags);
			}

			flags = vmm_spin_lock_irqsave(&wq->lock);
		}

		vmm_spin_unlock_irqrestore(&wq->lock, flags);

		vmm_threads_sleep(wq->thread);
	}

	return VMM_OK;
}

struct vmm_workqueue * vmm_workqueue_create(const char *name, u8 priority)
{
	struct vmm_workqueue * wq;
	irq_flags_t flags;

	if (!name) {
		return NULL;
	}

	wq = vmm_malloc(sizeof(struct vmm_workqueue));
	if (!wq) {
		return NULL;
	}

	INIT_SPIN_LOCK(&wq->lock);
	INIT_LIST_HEAD(&wq->head);
	INIT_LIST_HEAD(&wq->work_list);

	wq->thread = vmm_threads_create(name, workqueue_main, wq, 
					priority, VMM_THREAD_DEF_TIME_SLICE);
	if (!wq->thread) {
		vmm_free(wq);
		return NULL;
	}

	if (vmm_threads_start(wq->thread)) {
		vmm_threads_destroy(wq->thread);
		vmm_free(wq);
		return NULL;
	}

	flags = vmm_spin_lock_irqsave(&wqctrl.lock);

	list_add_tail(&wqctrl.wq_list, &wq->head);
	wqctrl.wq_count++;

	vmm_spin_unlock_irqrestore(&wqctrl.lock, flags);

	return wq;
}

int vmm_workqueue_destroy(struct vmm_workqueue * wq)
{
	int rc;
	irq_flags_t flags;

	if (!wq) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_workqueue_flush(wq))) {
		return rc;
	}

	if ((rc = vmm_threads_stop(wq->thread))) {
		return rc;
	}

	flags = vmm_spin_lock_irqsave(&wqctrl.lock);

	list_del(&wq->head);
	wqctrl.wq_count--;

	vmm_spin_unlock_irqrestore(&wqctrl.lock, flags);

	vmm_free(wq);

	return VMM_OK;
}

int __init vmm_workqueue_init(void)
{
	/* Reset control structure */
	vmm_memset(&wqctrl, 0, sizeof(wqctrl));

	/* Initialize lock in control structure */
	INIT_SPIN_LOCK(&wqctrl.lock);

	/* Initialize workqueue list */
	INIT_LIST_HEAD(&wqctrl.wq_list);

	/* Initialize workqueue count */
	wqctrl.wq_count = 0;

	/* Create system workqueus (Per Host CPU) */
	wqctrl.syswq = vmm_workqueue_create("syswq/0", 
					    VMM_THREAD_DEF_PRIORITY);
	if (!wqctrl.syswq) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}
