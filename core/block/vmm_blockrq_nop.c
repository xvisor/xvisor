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
 * @file vmm_blockrq_nop.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for NOP strategy based request queue
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_limits.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_host_aspace.h>
#include <block/vmm_blockrq_nop.h>

struct blockrq_nop_work {
	struct vmm_blockrq_nop *rqnop;
	struct dlist head;
	struct vmm_work work;
	struct vmm_request *r;
	void *r_priv;
	bool is_free;
};

static int blockrq_nop_queue_work(struct vmm_blockrq_nop *rqnop,
				  struct vmm_request *r)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct blockrq_nop_work *nopwork;

	vmm_spin_lock_irqsave(&rqnop->wq_lock, flags);

	if (list_empty(&rqnop->wq_free_list)) {
		rc = VMM_ENOMEM;
		goto done;
	}

	nopwork = list_first_entry(&rqnop->wq_free_list,
				   struct blockrq_nop_work, head);
	list_del(&nopwork->head);
	nopwork->r = r;
	if (r) {
		nopwork->r_priv = r->priv;
		r->priv = nopwork;
	} else {
		nopwork->r_priv = NULL;
	}
	nopwork->is_free = FALSE;
	list_add_tail(&nopwork->head, &rqnop->wq_pending_list);

	vmm_workqueue_schedule_work(rqnop->wq, &nopwork->work);

done:
	vmm_spin_unlock_irqrestore(&rqnop->wq_lock, flags);

	return rc;
}

static void blockrq_nop_dequeue_work(struct blockrq_nop_work *nopwork)
{
	irq_flags_t flags;
	struct vmm_blockrq_nop *rqnop = nopwork->rqnop;

	vmm_spin_lock_irqsave(&rqnop->wq_lock, flags);

	list_del(&nopwork->head);
	nopwork->is_free = TRUE;
	if (nopwork->r) {
		nopwork->r->priv = nopwork->r_priv;
		nopwork->r = NULL;
		nopwork->r_priv = NULL;
	}
	list_add_tail(&nopwork->head, &rqnop->wq_free_list);

	vmm_spin_unlock_irqrestore(&rqnop->wq_lock, flags);
}

static int blockrq_nop_abort_work(struct vmm_blockrq_nop *rqnop,
				  struct vmm_request *r)
{
	int rc;
	struct blockrq_nop_work *nopwork;

	if (!r || !r->priv) {
		return VMM_EINVALID;
	}
	nopwork = r->priv;

	rc = vmm_workqueue_stop_work(&nopwork->work);
	if (rc) {
		return rc;
	}

	if (!nopwork->is_free) {
		blockrq_nop_dequeue_work(nopwork);
	}

	return VMM_OK;
}

void blockrq_nop_rw_done(struct blockrq_nop_work *nopwork, int error)
{
	struct vmm_request *r;

	if (!nopwork || !nopwork->r || !nopwork->r->priv) {
		return;
	}
	r = nopwork->r;

	blockrq_nop_dequeue_work(nopwork);
	if (error) {
		vmm_blockdev_fail_request(r);
	} else {
		vmm_blockdev_complete_request(r);
	}
}

static void blockrq_nop_work_func(struct vmm_work *work)
{
	int rc;
	struct blockrq_nop_work *nopwork =
		container_of(work, struct blockrq_nop_work, work);
	struct vmm_blockrq_nop *rqnop = nopwork->rqnop;

	if (nopwork->r) {
		switch (nopwork->r->type) {
		case VMM_REQUEST_READ:
			if (rqnop->read) {
				rc = rqnop->read(rqnop,
						 nopwork->r, rqnop->priv);
			} else {
				rc = VMM_EIO;
			}
			break;
		case VMM_REQUEST_WRITE:
			if (rqnop->write) {
				rc = rqnop->write(rqnop,
						  nopwork->r, rqnop->priv);
			} else {
				rc = VMM_EIO;
			}
			break;
		default:
			rc = VMM_EINVALID;
			break;
		};
		if (!rqnop->async_rw) {
			blockrq_nop_rw_done(nopwork, rc);
		}
	} else {
		blockrq_nop_dequeue_work(nopwork);
		if (rqnop->flush) {
			rqnop->flush(rqnop, rqnop->priv);
		}
	}
}

static int blockrq_nop_make_request(struct vmm_request_queue *rq, 
				    struct vmm_request *r)
{
	return blockrq_nop_queue_work(vmm_blockrq_nop_from_rq(rq), r);
}

static int blockrq_nop_abort_request(struct vmm_request_queue *rq, 
				     struct vmm_request *r)
{
	return blockrq_nop_abort_work(vmm_blockrq_nop_from_rq(rq), r);
}

static int blockrq_nop_flush_cache(struct vmm_request_queue *rq)
{
	return blockrq_nop_queue_work(vmm_blockrq_nop_from_rq(rq), NULL);
}

void vmm_blockrq_nop_async_done(struct vmm_blockrq_nop *rqnop,
				struct vmm_request *r, int error)
{
	struct blockrq_nop_work *nopwork;

	if (!rqnop || !rqnop->async_rw || !r || !r->priv) {
		return;
	}
	nopwork = r->priv;

	blockrq_nop_rw_done(nopwork, error);
}
VMM_EXPORT_SYMBOL(vmm_blockrq_nop_async_done);

int vmm_blockrq_nop_destroy(struct vmm_blockrq_nop *rqnop)
{
	int rc;

	if (!rqnop) {
		return VMM_EINVALID;
	}

	rc = vmm_workqueue_destroy(rqnop->wq);
	if (rc) {
		return rc;
	}

	vmm_host_free_pages(rqnop->wq_page_va, rqnop->wq_page_count);

	vmm_free(rqnop);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_blockrq_nop_destroy);

struct vmm_blockrq_nop *vmm_blockrq_nop_create(
	const char *name, u32 max_pending, bool async_rw,
	int (*read)(struct vmm_blockrq_nop *,struct vmm_request *, void *),
	int (*write)(struct vmm_blockrq_nop *,struct vmm_request *, void *),
	void (*flush)(struct vmm_blockrq_nop *,void *),
	void *priv)
{
	u32 i;
	struct vmm_blockrq_nop *rqnop;
	struct blockrq_nop_work *nopwork;

	if (!name || !max_pending) {
		goto fail;
	}

	rqnop = vmm_zalloc(sizeof(*rqnop));
	if (!rqnop) {
		goto fail;
	}

	if (strlcpy(rqnop->name, name, sizeof(rqnop->name)) >=
	    sizeof(rqnop->name)) {
		goto fail_free_rqnop;
	}
	rqnop->max_pending = max_pending;
	rqnop->async_rw = async_rw;
	rqnop->read = read;
	rqnop->write = write;
	rqnop->flush = flush;
	rqnop->priv = priv;

	rqnop->wq_page_count =
			VMM_SIZE_TO_PAGE(max_pending * sizeof(*nopwork));
	rqnop->wq_page_va = vmm_host_alloc_pages(rqnop->wq_page_count,
						 VMM_MEMORY_FLAGS_NORMAL);
	INIT_SPIN_LOCK(&rqnop->wq_lock);
	INIT_LIST_HEAD(&rqnop->wq_free_list);
	INIT_LIST_HEAD(&rqnop->wq_pending_list);

	for (i = 0; i < rqnop->max_pending; i++) {
		nopwork = (struct blockrq_nop_work *)(rqnop->wq_page_va +
						      i*sizeof(*nopwork));
		nopwork->rqnop = rqnop;
		INIT_LIST_HEAD(&nopwork->head);
		INIT_WORK(&nopwork->work, blockrq_nop_work_func);
		nopwork->r = NULL;
		nopwork->is_free = TRUE;
		list_add_tail(&nopwork->head, &rqnop->wq_free_list);
	}

	rqnop->wq = vmm_workqueue_create(name, VMM_THREAD_DEF_PRIORITY);
	if (!rqnop->wq) {
		goto fail_free_pages;
	}

	/*
	 * Note: we set max_pending for underlying vmm_request_queue to be
	 * one less than vmm_blockrq_nop so that we always have atleast one
	 * blockrq_nop_work when clearing backlog via
	 * vmm_blockdev_complete_request() or vmm_blockdev_fail_request()
	 */
	INIT_REQUEST_QUEUE(&rqnop->rq,
			   max_pending,
			   blockrq_nop_make_request,
			   blockrq_nop_abort_request,
			   blockrq_nop_flush_cache,
			   rqnop);

	return rqnop;

fail_free_pages:
	vmm_host_free_pages(rqnop->wq_page_va, rqnop->wq_page_count);
fail_free_rqnop:
	vmm_free(rqnop);
fail:
	return NULL;
}
VMM_EXPORT_SYMBOL(vmm_blockrq_nop_create);

