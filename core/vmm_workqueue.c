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
#include <vmm_compiler.h>
#include <vmm_smp.h>
#include <vmm_heap.h>
#include <vmm_delay.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_completion.h>
#include <vmm_workqueue.h>
#include <libs/stringlib.h>

struct vmm_workqueue {
	vmm_spinlock_t lock;
	struct dlist head;
	struct dlist work_list;
	struct vmm_completion work_avail;
	struct vmm_thread *thread;
};

struct vmm_workqueue_ctrl {
	vmm_spinlock_t lock;
	struct dlist wq_list;
	u32 wq_count;
	struct vmm_workqueue *syswq[CONFIG_CPU_COUNT];
};

static struct vmm_workqueue_ctrl wqctrl;

bool vmm_workqueue_work_isnew(struct vmm_work *work)
{
	bool ret = FALSE;
	irq_flags_t flags;

	if (!work) {
		return FALSE;
	}

	vmm_spin_lock_irqsave(&work->lock, flags);

	ret = (work->flags & VMM_WORK_STATE_CREATED) ? TRUE : FALSE;

	vmm_spin_unlock_irqrestore(&work->lock, flags);

	return ret;
}

bool vmm_workqueue_work_inprogress(struct vmm_work *work)
{
	bool ret = FALSE;
	irq_flags_t flags;

	if (!work) {
		return FALSE;
	}

	vmm_spin_lock_irqsave(&work->lock, flags);

	ret = (work->flags & VMM_WORK_STATE_INPROGRESS) ? TRUE : FALSE;

	vmm_spin_unlock_irqrestore(&work->lock, flags);

	return ret;
}

bool vmm_workqueue_work_completed(struct vmm_work *work)
{
	bool ret = FALSE;
	irq_flags_t flags;

	if (!work) {
		return FALSE;
	}

	vmm_spin_lock_irqsave(&work->lock, flags);

	if (work->flags & VMM_WORK_STATE_CREATED) {
		ret = FALSE;
	} else {
		if (!(work->flags & VMM_WORK_STATE_INPROGRESS) &&
		    !(work->flags & VMM_WORK_STATE_SCHEDULED)) {
			ret = TRUE;
		} else {
			ret = FALSE;
		}
	}

	vmm_spin_unlock_irqrestore(&work->lock, flags);

	return ret;
}

int vmm_workqueue_stop_work(struct vmm_work *work)
{
	irq_flags_t flags, flags1;

	if (!work) {
		return VMM_EFAIL;
	}

stop_retry:
	vmm_spin_lock_irqsave(&work->lock, flags);

	if (work->flags & VMM_WORK_STATE_INPROGRESS) {
		vmm_spin_unlock_irqrestore(&work->lock, flags);
		vmm_udelay(VMM_THREAD_DEF_TIME_SLICE / 1000);
		goto stop_retry;
	}

	if (work->wq && (work->flags & VMM_WORK_STATE_SCHEDULED)) {
		vmm_spin_lock_irqsave(&(work->wq)->lock, flags1);
		list_del(&work->head);
		vmm_spin_unlock_irqrestore(&(work->wq)->lock, flags1);
	}

	work->flags &= ~VMM_WORK_STATE_CREATED;
	work->flags &= ~VMM_WORK_STATE_INPROGRESS;
	work->flags &= ~VMM_WORK_STATE_SCHEDULED;
	work->wq = NULL;

	vmm_spin_unlock_irqrestore(&work->lock, flags);

	return VMM_OK;
}

int vmm_workqueue_stop_delayed_work(struct vmm_delayed_work *work)
{
	int rc;

	if (!work) {
		return VMM_EFAIL;
	}

	rc = vmm_timer_event_stop(&work->event);
	if (rc) {
		return rc;
	}

	return vmm_workqueue_stop_work(&work->work);
}

struct vmm_thread *vmm_workqueue_get_thread(struct vmm_workqueue *wq)
{
	return (wq) ? wq->thread : NULL;
}

struct vmm_workqueue *vmm_workqueue_index2workqueue(int index)
{
	bool found;
	irq_flags_t flags;
	struct vmm_workqueue *wq;

	if (index < 0) {
		return NULL;
	}

	wq = NULL;
	found = FALSE;

	vmm_spin_lock_irqsave(&wqctrl.lock, flags);

	list_for_each_entry(wq, &wqctrl.wq_list, head) {
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

	return wq;
}

u32 vmm_workqueue_count(void)
{
	return wqctrl.wq_count;
}

int vmm_workqueue_flush(struct vmm_workqueue *wq)
{
	irq_flags_t flags;

	if (!wq) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&wq->lock, flags);

	while (!list_empty(&wq->work_list)) {
		vmm_spin_unlock_irqrestore(&wq->lock, flags);

		/* Make sure thread is running */
		vmm_threads_wakeup(wq->thread);

		/* We release the processor to let the wq thread do its job */
		vmm_scheduler_yield();

		vmm_spin_lock_irqsave(&wq->lock, flags);
	}

	vmm_spin_unlock_irqrestore(&wq->lock, flags);

	return VMM_OK;
}

int vmm_workqueue_schedule_work(struct vmm_workqueue *wq, 
				struct vmm_work *work)
{
	irq_flags_t flags, flags1;

	if (!work) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&work->lock, flags);

	if (work->flags & VMM_WORK_STATE_SCHEDULED) {
		vmm_spin_unlock_irqrestore(&work->lock, flags);
		return VMM_EALREADY;
	}

	if (!wq) {
		wq = wqctrl.syswq[vmm_smp_processor_id()];
	}

	work->flags &= ~VMM_WORK_STATE_CREATED;
	work->flags |= VMM_WORK_STATE_SCHEDULED;
	work->wq = wq;

	vmm_spin_lock_irqsave(&wq->lock, flags1);
	list_add_tail(&work->head, &wq->work_list);
	vmm_spin_unlock_irqrestore(&wq->lock, flags1);

	vmm_spin_unlock_irqrestore(&work->lock, flags);

	vmm_completion_complete(&wq->work_avail);

	return VMM_OK;
}

static void delayed_work_timer_event(struct vmm_timer_event *ev)
{
	struct vmm_delayed_work *work = ev->priv;

	vmm_workqueue_schedule_work(work->work.wq, &work->work);
}

int vmm_workqueue_schedule_delayed_work(struct vmm_workqueue *wq, 
					struct vmm_delayed_work *work,
					u64 nsecs)
{
	if (!wq || !work) {
		return VMM_EFAIL;
	}

	if (!nsecs) {
		return vmm_workqueue_schedule_work(wq, &work->work);
	}

	work->work.wq = wq;
	INIT_TIMER_EVENT(&work->event, delayed_work_timer_event, work);

	return vmm_timer_event_start(&work->event, nsecs);
}

static int workqueue_main(void *data)
{
	bool do_work;
	irq_flags_t flags;
	struct vmm_workqueue *wq = data;
	struct vmm_work *work = NULL;

	if (!wq) {
		return VMM_EFAIL;
	}

	while (1) {
		vmm_completion_wait(&wq->work_avail);

		vmm_spin_lock_irqsave(&wq->lock, flags);
		
		while (!list_empty(&wq->work_list)) {
			work = list_first_entry(&wq->work_list,
						struct vmm_work, head);
			list_del(&work->head);
			vmm_spin_unlock_irqrestore(&wq->lock, flags);

			do_work = FALSE;
			vmm_spin_lock_irqsave(&work->lock, flags);
			if (work->flags & VMM_WORK_STATE_SCHEDULED) {
				work->flags &= ~VMM_WORK_STATE_SCHEDULED;
				work->flags |= VMM_WORK_STATE_INPROGRESS;
				do_work = TRUE;
			}
			vmm_spin_unlock_irqrestore(&work->lock, flags);

			if (do_work) {
				work->func(work);
				vmm_spin_lock_irqsave(&work->lock, flags);
				work->flags &= ~VMM_WORK_STATE_INPROGRESS;
				vmm_spin_unlock_irqrestore(&work->lock, flags);
			}

			vmm_spin_lock_irqsave(&wq->lock, flags);
		}

		vmm_spin_unlock_irqrestore(&wq->lock, flags);
	}

	return VMM_OK;
}

struct vmm_workqueue *vmm_workqueue_create(const char *name, u8 priority)
{
	struct vmm_workqueue *wq;
	irq_flags_t flags;

	if (!name) {
		return NULL;
	}

	wq = vmm_zalloc(sizeof(struct vmm_workqueue));
	if (!wq) {
		return NULL;
	}

	INIT_SPIN_LOCK(&wq->lock);
	INIT_LIST_HEAD(&wq->head);
	INIT_LIST_HEAD(&wq->work_list);
	INIT_COMPLETION(&wq->work_avail);

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

	vmm_spin_lock_irqsave(&wqctrl.lock, flags);

	list_add_tail(&wq->head, &wqctrl.wq_list);
	wqctrl.wq_count++;

	vmm_spin_unlock_irqrestore(&wqctrl.lock, flags);

	return wq;
}

int vmm_workqueue_destroy(struct vmm_workqueue *wq)
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

	vmm_spin_lock_irqsave(&wqctrl.lock, flags);

	list_del(&wq->head);
	wqctrl.wq_count--;

	vmm_spin_unlock_irqrestore(&wqctrl.lock, flags);

	vmm_free(wq);

	return VMM_OK;
}

int __cpuinit vmm_workqueue_init(void)
{
	char syswq_name[VMM_FIELD_NAME_SIZE];
	u32 cpu = vmm_smp_processor_id();

	if (vmm_smp_is_bootcpu()) {
		/* Reset control structure */
		memset(&wqctrl, 0, sizeof(wqctrl));

		/* Initialize lock in control structure */
		INIT_SPIN_LOCK(&wqctrl.lock);

		/* Initialize workqueue list */
		INIT_LIST_HEAD(&wqctrl.wq_list);

		/* Initialize workqueue count */
		wqctrl.wq_count = 0;
	}

	/* Create one system workqueue with thread priority
	 * as default priority.
	 */
	vmm_snprintf(syswq_name, sizeof(syswq_name), "syswq/%d", cpu);
	wqctrl.syswq[cpu] = vmm_workqueue_create(syswq_name,
						 VMM_THREAD_DEF_PRIORITY);
	if (!wqctrl.syswq) {
		return VMM_EFAIL;
	}

	return vmm_threads_set_affinity(wqctrl.syswq[cpu]->thread,
					vmm_cpumask_of(cpu));
}
