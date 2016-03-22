/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file vmm_vdisk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for virtual disk framework
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vio/vmm_vdisk.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC			"Virtual Disk Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VDISK_IPRIORITY)
#define	MODULE_INIT			vmm_vdisk_init
#define	MODULE_EXIT			vmm_vdisk_exit

struct vmm_vdisk_ctrl {
	struct vmm_mutex vdisk_list_lock;
        struct dlist vdisk_list;
	struct vmm_blocking_notifier_chain notifier_chain;
	struct vmm_notifier_block blk_client;
};

static struct vmm_vdisk_ctrl vdctrl;

int vmm_vdisk_register_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_register(&vdctrl.notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_vdisk_register_client);

int vmm_vdisk_unregister_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_unregister(&vdctrl.notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_vdisk_unregister_client);

static void vdisk_req_completed(struct vmm_request *r)
{
	struct vmm_vdisk_request *vreq =
			container_of(r, struct vmm_vdisk_request, r);
	struct vmm_vdisk *vdisk = vreq->vdisk;

	if (vdisk->completed) {
		vdisk->completed(vdisk, vreq);
	}

	DPRINTF("%s: vdisk=%s lba=0x%llx bcnt=%d\n",
		__func__, vdisk->name, (u64)r->lba, r->bcnt);
}

static void vdisk_req_failed(struct vmm_request *r)
{
	struct vmm_vdisk_request *vreq =
			container_of(r, struct vmm_vdisk_request, r);
	struct vmm_vdisk *vdisk = vreq->vdisk;

	if (vdisk->failed) {
		vdisk->failed(vdisk, vreq);
	}

	DPRINTF("%s: vdisk=%s lba=0x%llx bcnt=%d\n",
		__func__, vdisk->name, (u64)r->lba, r->bcnt);
}

void vmm_vdisk_set_request_type(struct vmm_vdisk_request *vreq,
				enum vmm_vdisk_request_type type)
{
	if (!vreq) {
		return;
	}

	switch (type) {
	case VMM_VDISK_REQUEST_READ:
		vreq->r.type = VMM_REQUEST_READ;
		break;
	case VMM_VDISK_REQUEST_WRITE:
		vreq->r.type = VMM_REQUEST_WRITE;
		break;
	default:
		vreq->r.type = VMM_REQUEST_UNKNOWN;
		break;
	};
}

enum vmm_vdisk_request_type vmm_vdisk_get_request_type(
					struct vmm_vdisk_request *vreq)
{
	enum vmm_vdisk_request_type type;

	if (!vreq) {
		return VMM_VDISK_REQUEST_UNKNOWN;
	}

	switch (vreq->r.type) {
	case VMM_REQUEST_READ:
		type = VMM_VDISK_REQUEST_READ;
		break;
	case VMM_REQUEST_WRITE:
		type = VMM_VDISK_REQUEST_WRITE;
		break;
	default:
		type = VMM_VDISK_REQUEST_UNKNOWN;
		break;
	};

	return type;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_get_request_type);

void vmm_vdisk_set_request_len(struct vmm_vdisk_request *vreq, u32 data_len)
{
	irq_flags_t flags;
	struct vmm_vdisk *vdisk;

	if (!vreq || !vreq->vdisk) {
		return;
	}

	vdisk = vreq->vdisk;
	vmm_spin_lock_irqsave_lite(&vdisk->blk_lock, flags);
	vreq->r.bcnt =
		udiv32(data_len, vdisk->block_size) * vdisk->blk_factor;
	vmm_spin_unlock_irqrestore_lite(&vdisk->blk_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vdisk_set_request_len);

u32 vmm_vdisk_get_request_len(struct vmm_vdisk_request *vreq)
{
	u32 ret = 0;
	irq_flags_t flags;
	struct vmm_vdisk *vdisk;

	if (!vreq || !vreq->vdisk) {
		return 0;
	}

	vdisk = vreq->vdisk;
	vmm_spin_lock_irqsave_lite(&vdisk->blk_lock, flags);
	ret = udiv32(vreq->r.bcnt, vdisk->blk_factor) * vdisk->block_size;
	vmm_spin_unlock_irqrestore_lite(&vdisk->blk_lock, flags);

	return ret;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_get_request_len);

int vmm_vdisk_submit_request(struct vmm_vdisk *vdisk,
			     struct vmm_vdisk_request *vreq,
			     enum vmm_vdisk_request_type type,
			     u64 lba, void *data, u32 data_len)
{
	int rc;
	irq_flags_t flags;

	if (!vdisk || !vreq || !data) {
		return VMM_EINVALID;
	}
	if (data_len < vdisk->block_size) {
		return VMM_EINVALID;
	}
	if ((type < VMM_VDISK_REQUEST_READ) ||
	    (VMM_VDISK_REQUEST_WRITE < type)) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave_lite(&vdisk->blk_lock, flags);
	if (vdisk->blk) {
		vreq->vdisk = vdisk;
		vmm_vdisk_set_request_type(vreq, type);
		vreq->r.lba = lba * vdisk->blk_factor;
		vreq->r.bcnt =
			udiv32(data_len, vdisk->block_size) * vdisk->blk_factor;
		vreq->r.data = data;
		vreq->r.completed = vdisk_req_completed;
		vreq->r.failed = vdisk_req_failed;
		vreq->r.priv = NULL;
		rc = vmm_blockdev_submit_request(vdisk->blk, &vreq->r);
	} else {
		vdisk->failed(vdisk, vreq);
		rc = VMM_ENODEV;
	}
	vmm_spin_unlock_irqrestore_lite(&vdisk->blk_lock, flags);

	DPRINTF("%s: vdisk=%s lba=0x%llx bcnt=%d rc=%d\n",
		__func__, vdisk->name, (u64)vreq->r.lba, vreq->r.bcnt, rc);

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_submit_request);

int vmm_vdisk_abort_request(struct vmm_vdisk *vdisk,
			    struct vmm_vdisk_request *vreq)
{
	int rc;
	irq_flags_t flags;

	if (!vdisk || !vreq) {
		return VMM_EINVALID;
	}
	if (vreq->vdisk != vdisk) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave_lite(&vdisk->blk_lock, flags);
	if (vdisk->blk) {
		rc = vmm_blockdev_abort_request(&vreq->r);
	} else {
		rc = VMM_ENODEV;
	}
	vmm_spin_unlock_irqrestore_lite(&vdisk->blk_lock, flags);

	DPRINTF("%s: vdisk=%s lba=0x%llx bcnt=%d rc=%d\n",
		__func__, vdisk->name, (u64)vreq->r.lba, vreq->r.bcnt, rc);

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_abort_request);

int vmm_vdisk_flush_cache(struct vmm_vdisk *vdisk)
{
	int rc;
	irq_flags_t flags;

	if (!vdisk) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave_lite(&vdisk->blk_lock, flags);
	if (vdisk->blk) {
		rc = vmm_blockdev_flush_cache(vdisk->blk);
	} else {
		rc = VMM_ENODEV;
	}
	vmm_spin_unlock_irqrestore_lite(&vdisk->blk_lock, flags);

	DPRINTF("%s: vdisk=%s rc=%d\n",
		__func__, vdisk->name, rc);

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_flush_cache);

u64 vmm_vdisk_capacity(struct vmm_vdisk *vdisk)
{
	u64 ret = 0;
	irq_flags_t flags;

	if (!vdisk) {
		return 0;
	}

	vmm_spin_lock_irqsave_lite(&vdisk->blk_lock, flags);
	if (vdisk->blk) {
		ret = udiv64(vdisk->blk->num_blocks, vdisk->blk_factor);
	} else {
		ret = 0;
	}
	ret = (vdisk->blk) ? vdisk->blk->num_blocks : 0;
	vmm_spin_unlock_irqrestore_lite(&vdisk->blk_lock, flags);

	return ret;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_capacity);

int vmm_vdisk_current_block_device(struct vmm_vdisk *vdisk,
				   char *name, u32 name_len)
{
	int rc;
	irq_flags_t flags;

	if (!vdisk || !name || !name_len) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave_lite(&vdisk->blk_lock, flags);
	if (vdisk->blk) {
		strncpy(name, vdisk->blk->name, name_len);
		rc = VMM_OK;
	} else {
		rc = VMM_ENODEV;
	}
	vmm_spin_unlock_irqrestore_lite(&vdisk->blk_lock, flags);

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_current_block_device);

struct vdisk_attach_priv {
	struct vmm_vdisk *vdisk;
	const char *bdev_name;
};

static int vdisk_attach_iter(struct vmm_blockdev *dev, void *data)
{
	bool attached;
	irq_flags_t flags;
	struct vdisk_attach_priv *ap = data;
	const char *bdev_name = ap->bdev_name;
	struct vmm_vdisk *vdisk = ap->vdisk;

	if (strncmp(dev->name, bdev_name, sizeof(dev->name))==0) {
		attached = FALSE;
		vmm_spin_lock_irqsave_lite(&vdisk->blk_lock, flags);
		if (!vdisk->blk &&
		    (dev->block_size <= vdisk->block_size) &&
		    !umod32(vdisk->block_size, dev->block_size)) {
			vdisk->blk = dev;
			vdisk->blk_factor = udiv32(vdisk->block_size,
					           vdisk->blk->block_size);
			attached = TRUE;
		}
		vmm_spin_unlock_irqrestore_lite(&vdisk->blk_lock, flags);
		if (attached && vdisk->attached) {
			vdisk->attached(vdisk);
		}
	}

	return VMM_OK;
}

void vmm_vdisk_attach_block_device(struct vmm_vdisk *vdisk,
				   const char *bdev_name)
{
	struct vdisk_attach_priv ap;

	if (!vdisk || !bdev_name) {
		return;
	}

	ap.vdisk = vdisk;
	ap.bdev_name = bdev_name;
	vmm_blockdev_iterate(NULL, &ap, vdisk_attach_iter);
}
VMM_EXPORT_SYMBOL(vmm_vdisk_attach_block_device);

void vmm_vdisk_detach_block_device(struct vmm_vdisk *vdisk)
{
	bool detached;
	irq_flags_t flags;

	if (!vdisk) {
		return;
	}

	detached = FALSE;
	vmm_spin_lock_irqsave_lite(&vdisk->blk_lock, flags);
	if (vdisk->blk) {
		vmm_blockdev_flush_cache(vdisk->blk);
		detached = TRUE;
	}
	vdisk->blk = NULL;
	vdisk->blk_factor = 1;
	vmm_spin_unlock_irqrestore_lite(&vdisk->blk_lock, flags);

	if (detached && vdisk->detached) {
		vdisk->detached(vdisk);
	}
}
VMM_EXPORT_SYMBOL(vmm_vdisk_detach_block_device);

struct vmm_vdisk *vmm_vdisk_create(const char *name, u32 block_size,
	void (*attached)(struct vmm_vdisk *),
	void (*detached)(struct vmm_vdisk *),
	void (*completed)(struct vmm_vdisk *, struct vmm_vdisk_request *),
	void (*failed)(struct vmm_vdisk *, struct vmm_vdisk_request *),
	void *priv)
{
	bool found;
	struct vmm_vdisk *vdisk;
	struct vmm_vdisk_event event;

	if (!name || !block_size || !completed || !failed) {
		return NULL;
	}

	vdisk = NULL;
	found = FALSE;

	vmm_mutex_lock(&vdctrl.vdisk_list_lock);

	list_for_each_entry(vdisk, &vdctrl.vdisk_list, head) {
		if (strcmp(name, vdisk->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&vdctrl.vdisk_list_lock);
		return NULL;
	}

	vdisk = vmm_zalloc(sizeof(struct vmm_vdisk));
	if (!vdisk) {
		vmm_mutex_unlock(&vdctrl.vdisk_list_lock);
		return NULL;
	}

	INIT_LIST_HEAD(&vdisk->head);
	if (strlcpy(vdisk->name, name, sizeof(vdisk->name)) >=
	    sizeof(vdisk->name)) {
		vmm_free(vdisk);
		vmm_mutex_unlock(&vdctrl.vdisk_list_lock);
		return NULL;
	}
	vdisk->block_size = block_size;
	vdisk->attached = attached;
	vdisk->detached = detached;
	vdisk->completed = completed;
	vdisk->failed = failed;
	INIT_SPIN_LOCK(&vdisk->blk_lock);
	vdisk->blk = NULL;
	vdisk->blk_factor = 1;
	vdisk->priv = priv;

	list_add_tail(&vdisk->head, &vdctrl.vdisk_list);

	vmm_mutex_unlock(&vdctrl.vdisk_list_lock);

	/* Broadcast create event */
	event.vdisk = vdisk;
	event.data = NULL;
	vmm_blocking_notifier_call(&vdctrl.notifier_chain,
				   VMM_VDISK_EVENT_CREATE,
				   &event);

	return vdisk;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_create);

int vmm_vdisk_destroy(struct vmm_vdisk *vdisk)
{
	bool found;
	struct vmm_vdisk *vd;
	struct vmm_vdisk_event event;

	if (!vdisk) {
		return VMM_EFAIL;
	}

	/* Detach current block device */
	vmm_vdisk_detach_block_device(vdisk);

	/* Broadcast destroy event */
	event.vdisk = vdisk;
	event.data = NULL;
	vmm_blocking_notifier_call(&vdctrl.notifier_chain,
				   VMM_VDISK_EVENT_DESTROY,
				   &event);

	vmm_mutex_lock(&vdctrl.vdisk_list_lock);

	if (list_empty(&vdctrl.vdisk_list)) {
		vmm_mutex_unlock(&vdctrl.vdisk_list_lock);
		return VMM_EFAIL;
	}

	vd = NULL;
	found = FALSE;

	list_for_each_entry(vd, &vdctrl.vdisk_list, head) {
		if (strcmp(vd->name, vdisk->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&vdctrl.vdisk_list_lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&vd->head);

	vmm_free(vd);

	vmm_mutex_unlock(&vdctrl.vdisk_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_destroy);

struct vmm_vdisk *vmm_vdisk_find(const char *name)
{
	bool found;
	struct vmm_vdisk *vdisk;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	vdisk = NULL;

	vmm_mutex_lock(&vdctrl.vdisk_list_lock);

	list_for_each_entry(vdisk, &vdctrl.vdisk_list, head) {
		if (strcmp(vdisk->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&vdctrl.vdisk_list_lock);

	if (!found) {
		return NULL;
	}

	return vdisk;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_find);

int vmm_vdisk_iterate(struct vmm_vdisk *start, void *data,
		      int (*fn)(struct vmm_vdisk *vdisk, void *data))
{
	int rc = VMM_OK;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_vdisk *vd = NULL;

	if (!fn) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&vdctrl.vdisk_list_lock);

	list_for_each_entry(vd, &vdctrl.vdisk_list, head) {
		if (!start_found) {
			if (start && start == vd) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		rc = fn(vd, data);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&vdctrl.vdisk_list_lock);

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_iterate);

u32 vmm_vdisk_count(void)
{
	u32 retval = 0;
	struct vmm_vdisk *vdisk;

	vmm_mutex_lock(&vdctrl.vdisk_list_lock);

	list_for_each_entry(vdisk, &vdctrl.vdisk_list, head) {
		retval++;
	}

	vmm_mutex_unlock(&vdctrl.vdisk_list_lock);

	return retval;
}
VMM_EXPORT_SYMBOL(vmm_vdisk_count);

static int vdisk_blk_notification(struct vmm_notifier_block *nb,
				  unsigned long evt, void *data)
{
	irq_flags_t flags;
	struct vmm_vdisk *vdisk;
	struct vmm_blockdev_event *e = data;

	if (evt != VMM_BLOCKDEV_EVENT_UNREGISTER) {
		/* We are only interested in unregister events so,
		 * don't care about this event.
		 */
		return NOTIFY_DONE;
	}

	/* Lock virtual disk list */
	vmm_mutex_lock(&vdctrl.vdisk_list_lock);

	/* Find virtual disk using block device */
	list_for_each_entry(vdisk, &vdctrl.vdisk_list, head) {
		vmm_spin_lock_irqsave_lite(&vdisk->blk_lock, flags);
		if (vdisk->blk == e->bdev) {
			vdisk->blk = NULL;
			vdisk->blk_factor = 1;
		}
		vmm_spin_unlock_irqrestore_lite(&vdisk->blk_lock, flags);
	}

	/* Unlock virtual disk list */
	vmm_mutex_unlock(&vdctrl.vdisk_list_lock);

	return NOTIFY_OK;
}

static int __init vmm_vdisk_init(void)
{
	memset(&vdctrl, 0, sizeof(vdctrl));

	INIT_MUTEX(&vdctrl.vdisk_list_lock);
	INIT_LIST_HEAD(&vdctrl.vdisk_list);
	BLOCKING_INIT_NOTIFIER_CHAIN(&vdctrl.notifier_chain);

	vdctrl.blk_client.notifier_call = &vdisk_blk_notification;
	vdctrl.blk_client.priority = 0;
	vmm_blockdev_register_client(&vdctrl.blk_client);

	return VMM_OK;
}

static void __exit vmm_vdisk_exit(void)
{
	vmm_blockdev_unregister_client(&vdctrl.blk_client);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
