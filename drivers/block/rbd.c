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
 * @file rbd.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief RAM backed block device driver.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
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
#define MODULE_IPRIORITY		(VMM_BLOCKDEV_CLASS_IPRIORITY+1)
#define	MODULE_INIT			rbd_driver_init
#define	MODULE_EXIT			rbd_driver_exit

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
		vmm_host_memory_read(pa, r->data, sz);
		vmm_blockdev_complete_request(r);
		break;
	case VMM_REQUEST_WRITE:
		vmm_host_memory_write(pa, r->data, sz);
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

	if (!name) {
		return NULL;
	}

	d = vmm_zalloc(sizeof(struct rbd));
	if (!d) {
		goto free_nothing;
	}
	d->addr = pa;
	d->size = sz;

	d->bdev = vmm_blockdev_alloc();
	if (!d->bdev) {
		goto free_rbd;
	}

	/* Setup block device instance */
	strncpy(d->bdev->name, name, VMM_BLOCKDEV_MAX_NAME_SIZE);
	strncpy(d->bdev->desc, "RAM backed block device", 
		VMM_BLOCKDEV_MAX_DESC_SIZE);
	d->bdev->dev = dev;
	d->bdev->flags = VMM_BLOCKDEV_RW;
	d->bdev->start_lba = 0;
	d->bdev->num_blocks = udiv64(d->size, RBD_BLOCK_SIZE);
	d->bdev->block_size = RBD_BLOCK_SIZE;

	/* Setup request queue for block device instance */
	d->bdev->rq->make_request = rbd_make_request;
	d->bdev->rq->abort_request = rbd_abort_request;
	d->bdev->rq->priv = d;

	/* Register block device instance */
	if (vmm_blockdev_register(d->bdev)) {
		goto free_bdev;
	}

	/* Reserve RAM space */
	if (vmm_host_ram_reserve(d->addr, d->size)) {
		goto unreg_bdev;
	}

	return d;

unreg_bdev:
	vmm_blockdev_unregister(d->bdev);
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

void rbd_destroy(struct rbd *d)
{
	vmm_host_ram_reserve(d->addr, d->size);
	vmm_blockdev_unregister(d->bdev);
	vmm_blockdev_free(d->bdev);
	vmm_free(d);
}

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

	dev->priv = __rbd_create(dev, dev->node->name, pa, sz);
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
