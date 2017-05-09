/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file vmm_blockrq.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for generic blockdev request queue
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_limits.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_host_aspace.h>
#include <block/vmm_blockrq.h>

struct blockrq_work {
	struct vmm_blockrq *brq;
	struct dlist head;
	struct vmm_work work;
	bool is_rw;
	union {
		struct {
			struct vmm_request *r;
			void *priv;
		} rw;
		struct {
			void (*func)(struct vmm_blockrq *, void *);
			void *priv;
		} w;
	} d;
	bool is_free;
};

static int blockrq_queue_rw(struct vmm_blockrq *brq,
			    struct vmm_request *r)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct blockrq_work *bwork;

	vmm_spin_lock_irqsave(&brq->wq_lock, flags);

	if (list_empty(&brq->wq_rw_free_list)) {
		rc = VMM_ENOMEM;
		goto done;
	}

	bwork = list_first_entry(&brq->wq_rw_free_list,
				 struct blockrq_work, head);
	list_del(&bwork->head);
	bwork->is_rw = TRUE;
	bwork->d.rw.r = r;
	if (r) {
		bwork->d.rw.priv = r->priv;
		r->priv = bwork;
	} else {
		bwork->d.rw.priv = NULL;
	}
	bwork->is_free = FALSE;
	list_add_tail(&bwork->head, &brq->wq_pending_list);

	vmm_workqueue_schedule_work(brq->wq, &bwork->work);

done:
	vmm_spin_unlock_irqrestore(&brq->wq_lock, flags);

	return rc;
}

static int blockrq_queue_work(struct vmm_blockrq *brq,
			      void (*w_func)(struct vmm_blockrq *, void *),
			      void *w_priv)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct blockrq_work *bwork;

	vmm_spin_lock_irqsave(&brq->wq_lock, flags);

	if (list_empty(&brq->wq_w_free_list)) {
		rc = VMM_ENOMEM;
		goto done;
	}

	bwork = list_first_entry(&brq->wq_w_free_list,
				 struct blockrq_work, head);
	list_del(&bwork->head);
	bwork->is_rw = FALSE;
	bwork->d.w.func = w_func;
	bwork->d.w.priv = w_priv;
	bwork->is_free = FALSE;
	list_add_tail(&bwork->head, &brq->wq_pending_list);

	vmm_workqueue_schedule_work(brq->wq, &bwork->work);

done:
	vmm_spin_unlock_irqrestore(&brq->wq_lock, flags);

	return rc;
}

static void blockrq_dequeue_work(struct blockrq_work *bwork)
{
	irq_flags_t flags;
	struct vmm_blockrq *brq = bwork->brq;

	vmm_spin_lock_irqsave(&brq->wq_lock, flags);

	list_del(&bwork->head);
	bwork->is_free = TRUE;
	if (bwork->is_rw) {
		if (bwork->d.rw.r) {
			bwork->d.rw.r->priv = bwork->d.rw.priv;
		}
		bwork->d.rw.r = NULL;
		bwork->d.rw.priv = NULL;
		list_add_tail(&bwork->head, &brq->wq_rw_free_list);
	} else {
		bwork->d.w.func = NULL;
		bwork->d.w.priv = NULL;
		list_add_tail(&bwork->head, &brq->wq_w_free_list);
	}

	vmm_spin_unlock_irqrestore(&brq->wq_lock, flags);
}

static int blockrq_abort_rw(struct vmm_blockrq *brq,
			    struct vmm_request *r)
{
	int rc = VMM_OK;
	struct blockrq_work *bwork;

	if (!brq || !r || !r->priv) {
		return VMM_EINVALID;
	}
	bwork = r->priv;

	rc = vmm_workqueue_stop_work(&bwork->work);
	if (rc) {
		return rc;
	}

	if (!bwork->is_free) {
		blockrq_dequeue_work(bwork);
	}

	if (brq->abort) {
		rc = brq->abort(brq, r, brq->priv);
	}

	return rc;
}

void blockrq_rw_done(struct blockrq_work *bwork, int error)
{
	struct vmm_request *r;

	if (!bwork || !bwork->is_rw) {
		return;
	}
	if (!bwork->d.rw.r || !bwork->d.rw.r->priv) {
		return;
	}
	r = bwork->d.rw.r;

	blockrq_dequeue_work(bwork);
	if (error) {
		vmm_blockdev_fail_request(r);
	} else {
		vmm_blockdev_complete_request(r);
	}
}

static void blockrq_work_func(struct vmm_work *work)
{
	int rc = VMM_OK;
	void *w_priv;
	void (*w_func)(struct vmm_blockrq *, void *);
	struct blockrq_work *bwork =
		container_of(work, struct blockrq_work, work);
	struct vmm_blockrq *brq = bwork->brq;

	if (!bwork->is_rw) {
		w_func = bwork->d.w.func;
		w_priv = bwork->d.w.priv;
		blockrq_dequeue_work(bwork);
		if (w_func) {
			w_func(brq, w_priv);
		}
		return;
	}

	switch (bwork->d.rw.r->type) {
	case VMM_REQUEST_READ:
		if (brq->read) {
			rc = brq->read(brq, bwork->d.rw.r, brq->priv);
		} else {
			rc = VMM_EIO;
		}
		break;
	case VMM_REQUEST_WRITE:
		if (brq->write) {
			rc = brq->write(brq, bwork->d.rw.r, brq->priv);
		} else {
			rc = VMM_EIO;
		}
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};
	if (!brq->async_rw) {
		blockrq_rw_done(bwork, rc);
	}
}

static void blockrq_flush_work(struct vmm_blockrq *brq, void *priv)
{
	if (brq->flush) {
		brq->flush(brq, brq->priv);
	}
}

static int blockrq_make_request(struct vmm_request_queue *rq, 
				struct vmm_request *r)
{
	return blockrq_queue_rw(vmm_blockrq_from_rq(rq), r);
}

static int blockrq_abort_request(struct vmm_request_queue *rq, 
				 struct vmm_request *r)
{
	return blockrq_abort_rw(vmm_blockrq_from_rq(rq), r);
}

static int blockrq_flush_cache(struct vmm_request_queue *rq)
{
	return blockrq_queue_work(vmm_blockrq_from_rq(rq),
				  blockrq_flush_work, NULL);
}

void vmm_blockrq_async_done(struct vmm_blockrq *brq,
			    struct vmm_request *r, int error)
{
	struct blockrq_work *bwork;

	if (!brq || !brq->async_rw || !r || !r->priv) {
		return;
	}
	bwork = r->priv;

	blockrq_rw_done(bwork, error);
}
VMM_EXPORT_SYMBOL(vmm_blockrq_async_done);

int vmm_blockrq_queue_work(struct vmm_blockrq *brq,
			void (*w_func)(struct vmm_blockrq *, void *),
			void *w_priv)
{
	if (!brq || !w_func) {
		return VMM_EINVALID;
	}

	return blockrq_queue_work(brq, w_func, w_priv);
}
VMM_EXPORT_SYMBOL(vmm_blockrq_queue_work);

int vmm_blockrq_destroy(struct vmm_blockrq *brq)
{
	int rc;

	if (!brq) {
		return VMM_EINVALID;
	}

	rc = vmm_workqueue_destroy(brq->wq);
	if (rc) {
		return rc;
	}

	vmm_host_free_pages(brq->wq_page_va, brq->wq_page_count);

	vmm_free(brq);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_blockrq_destroy);

struct vmm_blockrq *vmm_blockrq_create(
	const char *name, u32 max_pending, bool async_rw,
	int (*read)(struct vmm_blockrq *,struct vmm_request *, void *),
	int (*write)(struct vmm_blockrq *,struct vmm_request *, void *),
	int (*abort)(struct vmm_blockrq *,struct vmm_request *, void *),
	void (*flush)(struct vmm_blockrq *,void *),
	void *priv)
{
	u32 i;
	struct vmm_blockrq *brq;
	struct blockrq_work *bwork;

	if (!name || !max_pending) {
		goto fail;
	}

	brq = vmm_zalloc(sizeof(*brq));
	if (!brq) {
		goto fail;
	}

	if (strlcpy(brq->name, name, sizeof(brq->name)) >=
	    sizeof(brq->name)) {
		goto fail_free_brq;
	}
	brq->max_pending = max_pending;
	brq->async_rw = async_rw;
	brq->read = read;
	brq->write = write;
	brq->abort = abort;
	brq->flush = flush;
	brq->priv = priv;

	brq->wq_page_count =
		VMM_SIZE_TO_PAGE(max_pending * sizeof(*bwork) * 2);
	brq->wq_page_va = vmm_host_alloc_pages(brq->wq_page_count,
					       VMM_MEMORY_FLAGS_NORMAL);
	INIT_SPIN_LOCK(&brq->wq_lock);
	INIT_LIST_HEAD(&brq->wq_rw_free_list);
	INIT_LIST_HEAD(&brq->wq_w_free_list);
	INIT_LIST_HEAD(&brq->wq_pending_list);

	for (i = 0; i < brq->max_pending; i++) {
		bwork = (struct blockrq_work *)(brq->wq_page_va +
						i * sizeof(*bwork));
		bwork->brq = brq;
		INIT_LIST_HEAD(&bwork->head);
		INIT_WORK(&bwork->work, blockrq_work_func);
		bwork->d.rw.r = NULL;
		bwork->d.rw.priv = NULL;
		bwork->is_rw = TRUE;
		bwork->is_free = TRUE;
		list_add_tail(&bwork->head, &brq->wq_rw_free_list);
	}

	for (i = brq->max_pending; i < (2 * brq->max_pending); i++) {
		bwork = (struct blockrq_work *)(brq->wq_page_va +
						i * sizeof(*bwork));
		bwork->brq = brq;
		INIT_LIST_HEAD(&bwork->head);
		INIT_WORK(&bwork->work, blockrq_work_func);
		bwork->d.w.func = NULL;
		bwork->d.w.priv = NULL;
		bwork->is_rw = FALSE;
		bwork->is_free = TRUE;
		list_add_tail(&bwork->head, &brq->wq_w_free_list);
	}

	brq->wq = vmm_workqueue_create(name, VMM_THREAD_DEF_PRIORITY);
	if (!brq->wq) {
		goto fail_free_pages;
	}

	INIT_REQUEST_QUEUE(&brq->rq,
			   max_pending,
			   blockrq_make_request,
			   blockrq_abort_request,
			   blockrq_flush_cache,
			   brq);

	return brq;

fail_free_pages:
	vmm_host_free_pages(brq->wq_page_va, brq->wq_page_count);
fail_free_brq:
	vmm_free(brq);
fail:
	return NULL;
}
VMM_EXPORT_SYMBOL(vmm_blockrq_create);
