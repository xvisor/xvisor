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

static BLOCKING_NOTIFIER_CHAIN(bdev_notifier_chain);

int vmm_blockdev_register_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_register(&bdev_notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_blockdev_register_client);

int vmm_blockdev_unregister_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_unregister(&bdev_notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_blockdev_unregister_client);

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
VMM_EXPORT_SYMBOL(vmm_blockdev_complete_request);

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
VMM_EXPORT_SYMBOL(vmm_blockdev_fail_request);

int vmm_blockdev_submit_request(struct vmm_blockdev *bdev,
				struct vmm_request *r)
{
	int rc;
	irq_flags_t flags;

	if (!bdev || !r || !bdev->rq) {
		rc = VMM_EFAIL;
		goto failed;
	}

	if ((r->type == VMM_REQUEST_WRITE) &&
	   !(bdev->flags & VMM_BLOCKDEV_RW)) {
		rc = VMM_EINVALID;
		goto failed;
	}

	if (bdev->num_blocks < r->bcnt) {
		rc = VMM_ERANGE;
		goto failed;
	}
	if ((r->lba < bdev->start_lba) ||
	    ((bdev->start_lba + bdev->num_blocks) <= r->lba)) {
		rc = VMM_ERANGE;
		goto failed;
	}
	if ((bdev->start_lba + bdev->num_blocks) < (r->lba + r->bcnt)) {
		rc = VMM_ERANGE;
		goto failed;
	}

	if (bdev->rq->make_request) {
		r->bdev = bdev;
		vmm_spin_lock_irqsave(&bdev->rq->lock, flags);
		rc = bdev->rq->make_request(bdev->rq, r);
		vmm_spin_unlock_irqrestore(&bdev->rq->lock, flags);
		if (rc) {
			r->bdev = NULL;
			return rc;
		}
	} else {
		rc = VMM_EFAIL;
		goto failed;
	}

	return VMM_OK;

failed:
	vmm_blockdev_fail_request(r);
	return rc;
}
VMM_EXPORT_SYMBOL(vmm_blockdev_submit_request);

int vmm_blockdev_abort_request(struct vmm_request *r)
{
	int rc;
	irq_flags_t flags;
	struct vmm_blockdev *bdev;

	if (!r || !r->bdev || !r->bdev->rq) {
		return VMM_EFAIL;
	}
	bdev = r->bdev;

	if (bdev->rq->abort_request) {
		vmm_spin_lock_irqsave(&bdev->rq->lock, flags);
		rc = bdev->rq->abort_request(bdev->rq, r);
		vmm_spin_unlock_irqrestore(&bdev->rq->lock, flags);
		if (rc) {
			return rc;
		}
	}

	return vmm_blockdev_fail_request(r);
}
VMM_EXPORT_SYMBOL(vmm_blockdev_abort_request);

int vmm_blockdev_flush_cache(struct vmm_blockdev *bdev)
{
	int rc;
	irq_flags_t flags;

	if (!bdev || !bdev->rq) {
		return VMM_EFAIL;
	}

	if (bdev->rq->flush_cache) {
		vmm_spin_lock_irqsave(&bdev->rq->lock, flags);
		rc = bdev->rq->flush_cache(bdev->rq);
		vmm_spin_unlock_irqrestore(&bdev->rq->lock, flags);
		if (rc) {
			return rc;
		}
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_blockdev_flush_cache);

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
	u8 *tbuf = NULL;
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
VMM_EXPORT_SYMBOL(vmm_blockdev_rw);

struct vmm_blockdev *vmm_blockdev_alloc(void)
{
	struct vmm_blockdev *bdev;

	bdev = vmm_zalloc(sizeof(struct vmm_blockdev));
	if (!bdev) {
		return NULL;
	}

	INIT_LIST_HEAD(&bdev->head);
	INIT_MUTEX(&bdev->child_lock);
	bdev->child_count = 0;
	INIT_LIST_HEAD(&bdev->child_list);
	bdev->rq = NULL;

	return bdev;
}
VMM_EXPORT_SYMBOL(vmm_blockdev_alloc);

void vmm_blockdev_free(struct vmm_blockdev *bdev)
{
	vmm_free(bdev);
}
VMM_EXPORT_SYMBOL(vmm_blockdev_free);

static struct vmm_class bdev_class = {
	.name = VMM_BLOCKDEV_CLASS_NAME,
};

int vmm_blockdev_register(struct vmm_blockdev *bdev)
{
	int rc;
	struct vmm_blockdev_event event;

	if (!bdev || !bdev->rq) {
		return VMM_EFAIL;
	}

	if (!(bdev->flags & VMM_BLOCKDEV_RDONLY) &&
	    !(bdev->flags & VMM_BLOCKDEV_RW)) {
		return VMM_EINVALID;
	}

	vmm_devdrv_initialize_device(&bdev->dev);
	if (strlcpy(bdev->dev.name, bdev->name, sizeof(bdev->dev.name)) >=
	    sizeof(bdev->dev.name)) {
		return VMM_EOVERFLOW;
	}
	bdev->dev.class = &bdev_class;
	vmm_devdrv_set_data(&bdev->dev, bdev);

	rc = vmm_devdrv_register_device(&bdev->dev);
	if (rc) {
		return rc;
	}

	/* Broadcast register event */
	event.bdev = bdev;
	event.data = NULL;
	vmm_blocking_notifier_call(&bdev_notifier_chain, 
				   VMM_BLOCKDEV_EVENT_REGISTER, 
				   &event);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_blockdev_register);

int vmm_blockdev_add_child(struct vmm_blockdev *bdev, 
			   u64 start_lba, u64 num_blocks)
{
	int rc;
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

	child_bdev = vmm_blockdev_alloc();
	child_bdev->parent = bdev;
	child_bdev->dev.parent = &bdev->dev;
	vmm_mutex_lock(&bdev->child_lock);
	vmm_snprintf(child_bdev->name, sizeof(child_bdev->name),
			"%sp%d", bdev->name, bdev->child_count);
	if (strlcpy(child_bdev->desc, bdev->desc, sizeof(child_bdev->desc)) >=
	    sizeof(child_bdev->desc)) {
		rc = VMM_EOVERFLOW;
		goto free_blockdev;
	}
	bdev->child_count++;
	list_add_tail(&child_bdev->head, &bdev->child_list);
	vmm_mutex_unlock(&bdev->child_lock);
	child_bdev->flags = bdev->flags;
	child_bdev->start_lba = start_lba;
	child_bdev->num_blocks = num_blocks;
	child_bdev->block_size = bdev->block_size;
	child_bdev->rq = bdev->rq;

	rc = vmm_blockdev_register(child_bdev);
	if (rc) {
		goto remove_from_list;
	}

	return rc;

remove_from_list:
	vmm_mutex_lock(&bdev->child_lock);
	list_del(&child_bdev->head);
	vmm_mutex_unlock(&bdev->child_lock);
free_blockdev:
	vmm_blockdev_free(child_bdev);
	return rc;
}
VMM_EXPORT_SYMBOL(vmm_blockdev_add_child);

int vmm_blockdev_unregister(struct vmm_blockdev *bdev)
{
	int rc;
	struct vmm_blockdev *child_bdev;
	struct vmm_blockdev_event event;

	if (!bdev) {
		return VMM_EFAIL;
	}

	/* Unreg & free child block devices */
	vmm_mutex_lock(&bdev->child_lock);
	while (!list_empty(&bdev->child_list)) {
		child_bdev = list_first_entry(&bdev->child_list, struct vmm_blockdev, head);
		list_del(&child_bdev->head);
		if ((rc = vmm_blockdev_unregister(child_bdev))) {
			vmm_mutex_unlock(&bdev->child_lock);
			return rc;
		}
		vmm_blockdev_free(child_bdev);
	}
	vmm_mutex_unlock(&bdev->child_lock);

	/* Broadcast unregister event */
	event.bdev = bdev;
	event.data = NULL;
	vmm_blocking_notifier_call(&bdev_notifier_chain, 
				   VMM_BLOCKDEV_EVENT_UNREGISTER, 
				   &event);

	return vmm_devdrv_unregister_device(&bdev->dev);
}
VMM_EXPORT_SYMBOL(vmm_blockdev_unregister);

struct vmm_blockdev *vmm_blockdev_find(const char *name)
{
	struct vmm_device *dev;

	dev = vmm_devdrv_class_find_device_by_name(&bdev_class, name);
	if (!dev) {
		return NULL;
	}

	return vmm_devdrv_get_data(dev);
}
VMM_EXPORT_SYMBOL(vmm_blockdev_find);

struct vmm_blockdev *vmm_blockdev_get(int num)
{
	struct vmm_device *dev;

	dev = vmm_devdrv_class_device(&bdev_class, num);
	if (!dev) {
		return NULL;
	}

	return vmm_devdrv_get_data(dev);
}
VMM_EXPORT_SYMBOL(vmm_blockdev_get);

u32 vmm_blockdev_count(void)
{
	return vmm_devdrv_class_device_count(&bdev_class);
}
VMM_EXPORT_SYMBOL(vmm_blockdev_count);

static int __init vmm_blockdev_init(void)
{
	vmm_printf("Initialize Block Device Framework\n");

	return vmm_devdrv_register_class(&bdev_class);
}

static void __exit vmm_blockdev_exit(void)
{
	vmm_devdrv_unregister_class(&bdev_class);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
