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
 * @file vmm_blockdev.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Block Device framework source
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_scheduler.h>
#include <vmm_devdrv.h>
#include <vmm_completion.h>
#include <block/vmm_blockdev.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"Block Device Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		VMM_BLOCKDEV_CLASS_IPRIORITY
#define	MODULE_INIT			vmm_blockdev_init
#define	MODULE_EXIT			vmm_blockdev_exit

int vmm_blockdev_submit_request(struct vmm_blockdev *bdev,
				struct vmm_request *r)
{
	int rc;
	irq_flags_t flags;

	if (!bdev || !r || !bdev->rq) {
		return VMM_EFAIL;
	}

	if ((r->type == VMM_REQUEST_WRITE) &&
	   !(bdev->flags & VMM_BLOCKDEV_RW)) {
		return VMM_EINVALID;
	}

	if (bdev->num_blocks < r->bcnt) {
		return VMM_ERANGE;
	}
	if ((r->lba < bdev->start_lba) ||
	    ((bdev->start_lba + bdev->num_blocks) <= r->lba)) {
		return VMM_ERANGE;
	}
	if ((bdev->start_lba + bdev->num_blocks) < (r->lba + r->bcnt)) {
		return VMM_ERANGE;
	}

	if (bdev->rq->make_request) {
		r->bdev = bdev;
		vmm_spin_lock_irqsave(&r->bdev->rq->lock, flags);
		rc = r->bdev->rq->make_request(r->bdev->rq, r);
		vmm_spin_unlock_irqrestore(&r->bdev->rq->lock, flags);
		if (rc) {
			r->bdev = NULL;
			return rc;
		}
	} else {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

int vmm_blockdev_complete_request(struct vmm_request *r)
{
	if (!r) {
		return VMM_EFAIL;
	}

	if (r->completed) {
		r->completed(r);
	}
	r->bdev = NULL;

	return VMM_OK;
}

int vmm_blockdev_fail_request(struct vmm_request *r)
{
	if (!r) {
		return VMM_EFAIL;
	}

	if (r->failed) {
		r->failed(r);
	}
	r->bdev = NULL;

	return VMM_OK;
}

int vmm_blockdev_abort_request(struct vmm_request *r)
{
	int rc;
	irq_flags_t flags;

	if (!r || !r->bdev || !r->bdev->rq) {
		return VMM_EFAIL;
	}

	if (r->bdev->rq->abort_request) {
		vmm_spin_lock_irqsave(&r->bdev->rq->lock, flags);
		rc = r->bdev->rq->abort_request(r->bdev->rq, r);
		vmm_spin_unlock_irqrestore(&r->bdev->rq->lock, flags);
		if (rc) {
			return rc;
		}
	}

	return vmm_blockdev_fail_request(r);
}

struct blockdev_rw {
	bool failed;
	struct vmm_request req;
	struct vmm_completion done;
};

static void blockdev_rw_completed(struct vmm_request *req)
{
	struct blockdev_rw *rw = req->priv;

	if (!rw) {
		return;
	}

	rw->failed = FALSE;
	vmm_completion_complete(&rw->done);
}

static void blockdev_rw_failed(struct vmm_request *req)
{
	struct blockdev_rw *rw = req->priv;

	if (!rw) {
		return;
	}

	rw->failed = TRUE;
	vmm_completion_complete(&rw->done);
}

static int blockdev_rw_blocks(struct vmm_blockdev *bdev,
				enum vmm_request_type type,
				u8 *buf, u64 lba, u64 bcnt)
{
	int rc;
	struct blockdev_rw rw;

	rw.failed = FALSE;
	rw.req.type = type;
	rw.req.lba = bdev->start_lba + lba;
	rw.req.bcnt = bcnt;
	rw.req.data = buf;
	rw.req.priv = &rw;
	rw.req.completed = blockdev_rw_completed;
	rw.req.failed = blockdev_rw_failed;
	INIT_COMPLETION(&rw.done);

	if ((rc = vmm_blockdev_submit_request(bdev, &rw.req))) {
		return rc;
	}

	vmm_completion_wait(&rw.done);

	if (rw.failed) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

u64 vmm_blockdev_rw(struct vmm_blockdev *bdev, 
			enum vmm_request_type type,
			u8 *buf, u64 off, u64 len)
{
	u8 *tbuf;
	u64 tmp, first_lba, first_off, first_len;
	u64 middle_lba, middle_len;
	u64 last_lba, last_len;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!buf || !bdev || !len) {
		return 0;
	}

	if ((type != VMM_REQUEST_READ) &&
	    (type != VMM_REQUEST_WRITE)) {
		return 0;
	}

	if ((type == VMM_REQUEST_WRITE) &&
	   !(bdev->flags & VMM_BLOCKDEV_RW)) {
		return 0;
	}

	tmp = bdev->num_blocks * bdev->block_size;
	if ((off >= tmp) || ((off + len) > tmp)) {
		return 0;
	}

	first_lba = udiv64(off, bdev->block_size);
	first_off = off - first_lba * bdev->block_size;
	if (first_off) {
		first_len = bdev->block_size - first_off;
		first_len = (first_len < len) ? first_len : len;
	} else {
		if (len < bdev->block_size) {
			first_len = len;
		} else {
			first_len = 0;
		}
	}

	off += first_len;
	len -= first_len;

	middle_lba = udiv64(off, bdev->block_size);
	middle_len = udiv64(len, bdev->block_size) * bdev->block_size;

	off += middle_len;
	len -= middle_len;

	last_lba = udiv64(off, bdev->block_size);
	last_len = len;

	if (first_len || last_len) {
		tbuf = vmm_malloc(bdev->block_size);
		if (!tbuf) {
			return 0;
		}
	}

	tmp = 0;

	if (first_len) {
		if (blockdev_rw_blocks(bdev, VMM_REQUEST_READ,
					tbuf, first_lba, 1)) {
			goto done;
		}

		if (type == VMM_REQUEST_WRITE) {
			memcpy(&tbuf[first_off], buf, first_len);
			if (blockdev_rw_blocks(bdev, VMM_REQUEST_WRITE,
					tbuf, first_lba, 1)) {
				goto done;
			}
		} else {
			memcpy(buf, &tbuf[first_off], first_len);
		}

		buf += first_len;
		tmp += first_len;
	}

	if (middle_len) {
		if (blockdev_rw_blocks(bdev, type,
		buf, middle_lba, udiv64(middle_len, bdev->block_size))) {
			goto done;
		}

		buf += middle_len;
		tmp += middle_len;
	}

	if (last_len) {
		if (blockdev_rw_blocks(bdev, VMM_REQUEST_READ,
					tbuf, last_lba, 1)) {
			goto done;
		}

		if (type == VMM_REQUEST_WRITE) {
			memcpy(&tbuf[0], buf, last_len);
			if (blockdev_rw_blocks(bdev, VMM_REQUEST_WRITE,
					tbuf, last_lba, 1)) {
				goto done;
			}
		} else {
			memcpy(buf, &tbuf[0], last_len);
		}

		tmp += last_len;
	}

done:
	if (first_len || last_len) {
		vmm_free(tbuf);
	}

	return tmp;
}

static struct vmm_blockdev *__blockdev_alloc(bool alloc_rq)
{
	struct vmm_blockdev *bdev;

	bdev = vmm_zalloc(sizeof(struct vmm_blockdev));
	if (!bdev) {
		return NULL;
	}

	INIT_LIST_HEAD(&bdev->head);
	INIT_SPIN_LOCK(&bdev->child_lock);
	bdev->child_count = 0;
	INIT_LIST_HEAD(&bdev->child_list);

	if (alloc_rq) {
		bdev->rq = vmm_zalloc(sizeof(struct vmm_request_queue));
		if (!bdev->rq) {
			vmm_free(bdev);
			return NULL;
		}

		INIT_SPIN_LOCK(&bdev->rq->lock);
	} else {
		bdev->rq = NULL;
	}

	return bdev;
}

struct vmm_blockdev *vmm_blockdev_alloc(void)
{
	return __blockdev_alloc(TRUE);
}

static void __blockdev_free(struct vmm_blockdev *bdev, bool free_rq)
{
	if (free_rq) {
		vmm_free(bdev->rq);
	}
	vmm_free(bdev);
}

void vmm_blockdev_free(struct vmm_blockdev *bdev)
{
	__blockdev_free(bdev, TRUE);
}

int vmm_blockdev_register(struct vmm_blockdev *bdev)
{
	int rc;
	struct vmm_classdev *cd;

	if (!bdev) {
		return VMM_EFAIL;
	}

	if (!(bdev->flags & VMM_BLOCKDEV_RDONLY) &&
	    !(bdev->flags & VMM_BLOCKDEV_RW)) {
		return VMM_EINVALID;
	}

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&cd->head);
	strncpy(cd->name, bdev->name, VMM_BLOCKDEV_MAX_NAME_SIZE);
	cd->dev = bdev->dev;
	cd->priv = bdev;

	rc = vmm_devdrv_register_classdev(VMM_BLOCKDEV_CLASS_NAME, cd);
	if (rc) {
		cd->dev = NULL;
		cd->priv = NULL;
		vmm_free(cd);
		return rc;
	}

	return VMM_OK;
}

int vmm_blockdev_add_child(struct vmm_blockdev *bdev, 
			   u64 start_lba, u64 num_blocks)
{
	int rc;
	irq_flags_t flags;
	struct vmm_blockdev *child_bdev;

	if (!bdev) {
		return VMM_EFAIL;
	}

	if (bdev->num_blocks < num_blocks) {
		return VMM_ERANGE;
	}
	if ((start_lba < bdev->start_lba) ||
	    ((bdev->start_lba + bdev->num_blocks) <= start_lba)) {
		return VMM_ERANGE;
	}
	if ((bdev->start_lba + bdev->num_blocks) < (start_lba + num_blocks)) {
		return VMM_ERANGE;
	}

	child_bdev = __blockdev_alloc(FALSE);
	child_bdev->parent = bdev;
	vmm_spin_lock_irqsave(&bdev->child_lock, flags);
	vmm_snprintf(child_bdev->name, VMM_BLOCKDEV_MAX_NAME_SIZE,
			"%sp%d", bdev->name, bdev->child_count);
	bdev->child_count++;
	list_add_tail(&child_bdev->head, &bdev->child_list);
	vmm_spin_unlock_irqrestore(&bdev->child_lock, flags);
	child_bdev->flags = bdev->flags;
	child_bdev->start_lba = start_lba;
	child_bdev->num_blocks = num_blocks;
	child_bdev->block_size = bdev->block_size;
	child_bdev->rq = bdev->rq;

	rc = vmm_blockdev_register(child_bdev);
	if (rc) {
		vmm_spin_lock_irqsave(&bdev->child_lock, flags);
		list_del(&child_bdev->head);
		vmm_spin_unlock_irqrestore(&bdev->child_lock, flags);
		__blockdev_free(child_bdev, FALSE);
	}

	return rc;
}

int vmm_blockdev_unregister(struct vmm_blockdev *bdev)
{
	int rc;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_blockdev *child_bdev;
	struct vmm_classdev *cd;

	if (!bdev) {
		return VMM_EFAIL;
	}

	/* Unreg & free child block devices */
	vmm_spin_lock_irqsave(&bdev->child_lock, flags);
	while (!list_empty(&bdev->child_list)) {
		l = list_pop(&bdev->child_list);
		child_bdev = list_entry(l, struct vmm_blockdev, head);
		if ((rc = vmm_blockdev_unregister(child_bdev))) {
			vmm_spin_unlock_irqrestore(&bdev->child_lock, flags);
			return rc;
		}
		__blockdev_free(child_bdev, FALSE);
	}
	vmm_spin_unlock_irqrestore(&bdev->child_lock, flags);

	cd = vmm_devdrv_find_classdev(VMM_BLOCKDEV_CLASS_NAME, bdev->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(VMM_BLOCKDEV_CLASS_NAME, cd);
	if (!rc) {
		vmm_free(cd);
	}

	return rc;
}

struct vmm_blockdev *vmm_blockdev_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_BLOCKDEV_CLASS_NAME, name);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

struct vmm_blockdev *vmm_blockdev_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_BLOCKDEV_CLASS_NAME, num);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

u32 vmm_blockdev_count(void)
{
	return vmm_devdrv_classdev_count(VMM_BLOCKDEV_CLASS_NAME);
}

static int __init vmm_blockdev_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize Block Device Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&c->head);
	strcpy(c->name, VMM_BLOCKDEV_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc) {
		vmm_free(c);
		return rc;
	}

	return VMM_OK;
}

static void __exit vmm_blockdev_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(VMM_BLOCKDEV_CLASS_NAME);
	if (!c) {
		return;
	}

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		return;
	}

	vmm_free(c);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
