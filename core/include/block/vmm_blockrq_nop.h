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
 * @file vmm_blockrq_nop.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for NOP strategy based request queue
 */

#ifndef __VMM_BLOCKRQ_NOP_H_
#define __VMM_BLOCKRQ_NOP_H_

#include <vmm_types.h>
#include <vmm_limits.h>
#include <vmm_workqueue.h>
#include <vmm_spinlocks.h>
#include <block/vmm_blockdev.h>
#include <libs/list.h>

/** Representation of NOP strategy based request queue */
struct vmm_blockrq_nop {
	char name[VMM_FIELD_NAME_SIZE];
	u32 max_pending;

	int (*read)(struct vmm_blockrq_nop *rqnop,
		    struct vmm_request *r, void *priv);
	int (*write)(struct vmm_blockrq_nop *rqnop,
		     struct vmm_request *r, void *priv);
	void (*flush)(struct vmm_blockrq_nop *rqnop, void *priv);
	void *priv;

	u32 wq_page_count;
	virtual_addr_t wq_page_va;
	vmm_spinlock_t wq_lock;
	struct dlist wq_free_list;
	struct dlist wq_pending_list;

	struct vmm_workqueue *wq;

	struct vmm_request_queue rq;
};
#define vmm_rq_to_blockrq_nop(__rq)	\
	container_of(__rq, struct vmm_blockrq_nop, rq)

/** Get NOP strategy based request queue from request queue pointer */
static inline struct vmm_blockrq_nop *vmm_blockrq_nop_from_rq(
					struct vmm_request_queue *rq)
{
	return (struct vmm_blockrq_nop *)rq->priv;
}

/** Get request queue pointer from NOP strategy based request queue */
static inline struct vmm_request_queue *vmm_blockrq_nop_to_rq(
					struct vmm_blockrq_nop *rqnop)
{
	return &rqnop->rq;
}

/** Destroy NOP strategy based request queue
 *  Note: This function should be called from Orphan (or Thread) context.
 */
int vmm_blockrq_nop_destroy(struct vmm_blockrq_nop *rqnop);

/** Create NOP strategy based request queue
 *  Note: This function should be called from Orphan (or Thread) context.
 */
struct vmm_blockrq_nop *vmm_blockrq_nop_create(
	const char *name, u32 max_pending,
	int (*read)(struct vmm_blockrq_nop *,struct vmm_request *, void *),
	int (*write)(struct vmm_blockrq_nop *,struct vmm_request *, void *),
	void (*flush)(struct vmm_blockrq_nop *,void *),
	void *priv);

#endif
