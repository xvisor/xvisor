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
 * @file rbd.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RAM backed block device driver.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_spinlocks.h>
#include <vmm_host_ram.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <drv/rbd.h>

#define MODULE_DESC			"RAM Backed Block Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(RBD_IPRIORITY)
#define	MODULE_INIT			rbd_driver_init
#define	MODULE_EXIT			rbd_driver_exit

static LIST_HEAD(rbd_list);
static DEFINE_SPINLOCK(rbd_list_lock);

static int rbd_make_request(struct vmm_request_queue *rq,
			    struct vmm_request *r)
{
	struct rbd *d = rq->priv;
	physical_addr_t pa;
	physical_size_t sz;

	pa = d->addr + r->lba * RBD_BLOCK_SIZE;
	sz = r->bcnt * RBD_BLOCK_SIZE;

	switch (r->type) {
	case VMM_REQUEST_READ:
		vmm_host_memory_read(pa, r->data, sz, TRUE);
		vmm_blockdev_complete_request(r);
		break;
	case VMM_REQUEST_WRITE:
		vmm_host_memory_write(pa, r->data, sz, TRUE);
		vmm_blockdev_complete_request(r);
		break;
	default:
		vmm_blockdev_fail_request(r);
		break;
	};

	return VMM_OK;
}

static int rbd_abort_request(struct vmm_request_queue *rq,
			     struct vmm_request *r)
{
	/* Do nothing to abort */
	return VMM_OK;
}

static struct rbd *__rbd_create(struct vmm_device *dev,
				const char *name,
				physical_addr_t pa,
				physical_size_t sz)
{
	struct rbd *d;
	irq_flags_t flags;
	physical_addr_t check_pa;

	if (!name) {
		return NULL;
	}

	d = vmm_zalloc(sizeof(struct rbd));
	if (!d) {
		goto free_nothing;
	}
	INIT_LIST_HEAD(&d->head);
	d->addr = pa;
	d->size = sz;

	d->bdev = vmm_blockdev_alloc();
	if (!d->bdev) {
		goto free_rbd;
	}

	/* Setup block device instance */
	strncpy(d->bdev->name, name, VMM_FIELD_NAME_SIZE);
	strncpy(d->bdev->desc, "RAM backed block device",
		VMM_FIELD_DESC_SIZE);
	d->bdev->dev.parent = dev;
	d->bdev->flags = VMM_BLOCKDEV_RW;
	d->bdev->start_lba = 0;
	d->bdev->num_blocks = udiv64(d->size, RBD_BLOCK_SIZE);
	d->bdev->block_size = RBD_BLOCK_SIZE;

	/* Setup request queue for block device instance */
	d->bdev->rq = vmm_zalloc(sizeof(struct vmm_request_queue));
	if (!d->bdev->rq) {
		goto free_bdev;
	}
	INIT_REQUEST_QUEUE(d->bdev->rq);
	d->bdev->rq->make_request = rbd_make_request;
	d->bdev->rq->abort_request = rbd_abort_request;
	d->bdev->rq->priv = d;

	/* Register block device instance */
	if (vmm_blockdev_register(d->bdev)) {
		goto free_bdev_rq;
	}

	/* Reserve RAM space If required */
	d->reserve_ram = TRUE;
	check_pa = d->addr;
	while (check_pa < (d->addr + d->size)) {
		if (!vmm_host_ram_frame_isfree(check_pa)) {
			d->reserve_ram = FALSE;
			break;
		}
		check_pa += VMM_PAGE_SIZE;
	}
	if (d->reserve_ram) {
		if (vmm_host_ram_reserve(d->addr, d->size)) {
			goto unreg_bdev;
		}
	}

	/* Add to list of RBD instances */
	vmm_spin_lock_irqsave(&rbd_list_lock, flags);
	list_add_tail(&d->head, &rbd_list);
	vmm_spin_unlock_irqrestore(&rbd_list_lock, flags);

	return d;

unreg_bdev:
	vmm_blockdev_unregister(d->bdev);
free_bdev_rq:
	vmm_free(d->bdev->rq);
free_bdev:
	vmm_blockdev_free(d->bdev);
free_rbd:
	vmm_free(d);
free_nothing:
	return NULL;
}

struct rbd *rbd_create(const char *name,
			physical_addr_t pa,
			physical_size_t sz)
{
	return __rbd_create(NULL, name, pa, sz);
}
VMM_EXPORT_SYMBOL(rbd_create);

void rbd_destroy(struct rbd *d)
{
	irq_flags_t flags;

	/* Sanity check */
	if (!d) {
		return;
	}

	/* Remove from list of RBD instances */
	vmm_spin_lock_irqsave(&rbd_list_lock, flags);
	list_del(&d->head);
	vmm_spin_unlock_irqrestore(&rbd_list_lock, flags);

	/* Unreserver RAM space */
	if (d->reserve_ram) {
		vmm_host_ram_free(d->addr, d->size);
		d->reserve_ram = FALSE;
	}

	/* Unregister block device */
	vmm_blockdev_unregister(d->bdev);

	/* Free block device request queue */
	vmm_free(d->bdev->rq);

	/* Free block device */
	vmm_blockdev_free(d->bdev);

	/* Free RBD instance */
	vmm_free(d);
}
VMM_EXPORT_SYMBOL(rbd_destroy);

struct rbd *rbd_find(const char *name)
{
	bool found;
	struct dlist *l;
	struct rbd *d;
	irq_flags_t flags;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	d = NULL;

	vmm_spin_lock_irqsave(&rbd_list_lock, flags);

	list_for_each(l, &rbd_list) {
		d = list_entry(l, struct rbd, head);
		if (strcmp(d->bdev->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_spin_unlock_irqrestore(&rbd_list_lock, flags);

	if (!found) {
		return NULL;
	}

	return d;
}
VMM_EXPORT_SYMBOL(rbd_find);

struct rbd *rbd_get(int index)
{
	bool found;
	struct dlist *l;
	struct rbd *retval;
	irq_flags_t flags;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	vmm_spin_lock_irqsave(&rbd_list_lock, flags);

	list_for_each(l, &rbd_list) {
		retval = list_entry(l, struct rbd, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_spin_unlock_irqrestore(&rbd_list_lock, flags);

	if (!found) {
		return NULL;
	}

	return retval;
}
VMM_EXPORT_SYMBOL(rbd_get);

u32 rbd_count(void)
{
	u32 retval = 0;
	struct dlist *l;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&rbd_list_lock, flags);

	list_for_each(l, &rbd_list) {
		retval++;
	}

	vmm_spin_unlock_irqrestore(&rbd_list_lock, flags);

	return retval;
}
VMM_EXPORT_SYMBOL(rbd_count);

static int rbd_driver_probe(struct vmm_device *dev,
			    const struct vmm_devtree_nodeid *devid)
{
	int rc;
	physical_addr_t pa;
	physical_size_t sz;

	rc = vmm_devtree_regaddr(dev->node, &pa, 0);
	if (rc) {
		return rc;
	}

	rc = vmm_devtree_regsize(dev->node, &sz, 0);
	if (rc) {
		return rc;
	}

	dev->priv = __rbd_create(dev, dev->name, pa, sz);
	if (!dev->priv) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

static int rbd_driver_remove(struct vmm_device *dev)
{
	rbd_destroy(dev->priv);

	return VMM_OK;
}

static struct vmm_devtree_nodeid rbd_devid_table[] = {
	{.type = "block",.compatible = "rbd"},
	{ /* end of list */ },
};

static struct vmm_driver rbd_driver = {
	.name = "rbd",
	.match_table = rbd_devid_table,
	.probe = rbd_driver_probe,
	.remove = rbd_driver_remove,
};

static int __init rbd_driver_init(void)
{
	return vmm_devdrv_register_driver(&rbd_driver);
}

static void __exit rbd_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&rbd_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
