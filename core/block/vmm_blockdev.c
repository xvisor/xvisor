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
#include <vmm_devdrv.h>
#include <stringlib.h>
#include <block/vmm_blockdev.h>

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

	if (r->bdev->rq->make_request) {
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
