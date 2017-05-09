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
 * @file vmm_blockrq.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for generic blockdev request queue
 */

#ifndef __VMM_BLOCKRQ_H__
#define __VMM_BLOCKRQ_H__

#include <vmm_types.h>
#include <vmm_limits.h>
#include <vmm_workqueue.h>
#include <vmm_spinlocks.h>
#include <block/vmm_blockdev.h>
#include <libs/list.h>

/** Representation of generic request queue */
struct vmm_blockrq {
	char name[VMM_FIELD_NAME_SIZE];
	u32 max_pending;
	bool async_rw;

	int (*read)(struct vmm_blockrq *brq,
		    struct vmm_request *r, void *priv);
	int (*write)(struct vmm_blockrq *brq,
		     struct vmm_request *r, void *priv);
	int (*abort)(struct vmm_blockrq *brq,
		     struct vmm_request *r, void *priv);
	void (*flush)(struct vmm_blockrq *brq, void *priv);
	void *priv;

	u32 wq_page_count;
	virtual_addr_t wq_page_va;
	vmm_spinlock_t wq_lock;
	struct dlist wq_rw_free_list;
	struct dlist wq_w_free_list;
	struct dlist wq_pending_list;

	struct vmm_workqueue *wq;

	struct vmm_request_queue rq;
};
#define vmm_rq_to_blockrq(__rq)	\
	container_of(__rq, struct vmm_blockrq, rq)

/** Get generic blockdev request queue from request queue pointer */
static inline struct vmm_blockrq *vmm_blockrq_from_rq(
					struct vmm_request_queue *rq)
{
	return (struct vmm_blockrq *)rq->priv;
}

/** Get request queue pointer from generic blockdev request queue */
static inline struct vmm_request_queue *vmm_blockrq_to_rq(
					struct vmm_blockrq *brq)
{
	return &brq->rq;
}

/** Mark async request done */
void vmm_blockrq_async_done(struct vmm_blockrq *brq,
			    struct vmm_request *r, int error);

/** Queue custom work on request queue */
int vmm_blockrq_queue_work(struct vmm_blockrq *brq,
			void (*w_func)(struct vmm_blockrq *, void *),
			void *w_priv);

/** Destroy generic blockdev request queue
 *  Note: This function should be called from Orphan (or Thread) context.
 */
int vmm_blockrq_destroy(struct vmm_blockrq *brq);

/** Create generic blockdev request queue
 *  Note: This function should be called from Orphan (or Thread) context.
 */
struct vmm_blockrq *vmm_blockrq_create(
	const char *name, u32 max_pending, bool async_rw,
	int (*read)(struct vmm_blockrq *,struct vmm_request *, void *),
	int (*write)(struct vmm_blockrq *,struct vmm_request *, void *),
	int (*abort)(struct vmm_blockrq *,struct vmm_request *, void *),
	void (*flush)(struct vmm_blockrq *,void *),
	void *priv);

#endif
