/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file virtio_blk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO based block device Emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_spinlocks.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <block/vmm_blockdev.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>

#include <emu/virtio.h>
#include <emu/virtio_blk.h>

#define MODULE_DESC			"VirtIO Block Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VIRTIO_IPRIORITY + 1)
#define MODULE_INIT			virtio_blk_init
#define MODULE_EXIT			virtio_blk_exit

#define VIRTIO_BLK_QUEUE_SIZE		128
#define VIRTIO_BLK_IO_QUEUE		0
#define VIRTIO_BLK_NUM_QUEUES		1
#define VIRTIO_BLK_SECTOR_SIZE		512
#define VIRTIO_BLK_DISK_SEG_MAX		(VIRTIO_BLK_QUEUE_SIZE - 2)

struct virtio_blk_dev_req {
	struct virtio_queue		*vq;
	struct virtio_blk_dev 		*bdev;
	u16				head;
	struct virtio_iovec		*read_iov;
	u32				read_iov_cnt;
	u32				len;
	struct virtio_iovec		status_iov;
	struct vmm_request		r;
};

struct virtio_blk_dev {
	struct virtio_device 		*vdev;

	struct virtio_queue 		vqs[VIRTIO_BLK_NUM_QUEUES];
	struct virtio_iovec		iov[VIRTIO_BLK_QUEUE_SIZE];
	struct virtio_blk_dev_req	reqs[VIRTIO_BLK_QUEUE_SIZE];
	struct virtio_blk_config 	config;
	u32 				features;

	char				blk_name[VMM_FIELD_NAME_SIZE];
	struct vmm_notifier_block	blk_client;
	vmm_spinlock_t			blk_lock; /* Protect blk pointer */
	struct vmm_blockdev 		*blk;
};

static u32 virtio_blk_get_host_features(struct virtio_device *dev)
{
	return	1UL << VIRTIO_BLK_F_SEG_MAX
		| 1UL << VIRTIO_BLK_F_BLK_SIZE
		| 1UL << VIRTIO_BLK_F_FLUSH
		| 1UL << VIRTIO_RING_F_EVENT_IDX;
#if 0
		| 1UL << VIRTIO_RING_F_INDIRECT_DESC;
#endif
}

static void virtio_blk_set_guest_features(struct virtio_device *dev,
					  u32 features)
{
	struct virtio_blk_dev *bdev = dev->emu_data;

	bdev->features = features;
}

static int virtio_blk_init_vq(struct virtio_device *dev,
			      u32 vq, u32 page_size, u32 align,
			      u32 pfn)
{
	int rc;
	struct virtio_blk_dev *bdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_BLK_IO_QUEUE:
		rc = virtio_queue_setup(&bdev->vqs[vq], dev->guest, 
				pfn, page_size, VIRTIO_BLK_QUEUE_SIZE, align);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_blk_get_pfn_vq(struct virtio_device *dev, u32 vq)
{
	int rc;
	struct virtio_blk_dev *bdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_BLK_IO_QUEUE:
		rc = virtio_queue_guest_pfn(&bdev->vqs[vq]);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_blk_get_size_vq(struct virtio_device *dev, u32 vq)
{
	int rc;

	switch (vq) {
	case VIRTIO_BLK_IO_QUEUE:
		rc = VIRTIO_BLK_QUEUE_SIZE;
		break;
	default:
		rc = 0;
		break;
	};

	return rc;
}

static int virtio_blk_set_size_vq(struct virtio_device *dev, u32 vq, int size)
{
	/* FIXME: dynamic */
	return size;
}

static void virtio_blk_req_done(struct virtio_blk_dev_req *req, u8 status)
{
	struct virtio_device *dev = req->bdev->vdev;
	int queueid = req->vq - req->bdev->vqs;

	if (req->read_iov && req->len && req->r.data &&
	    (status == VIRTIO_BLK_S_OK) &&
	    (req->r.type == VMM_REQUEST_READ)) {
		virtio_buf_to_iovec_write(dev, 
					  req->read_iov, 
					  req->read_iov_cnt, 
					  req->r.data, 
					  req->len);
	}

	if (req->read_iov) {
		vmm_free(req->read_iov);
		req->read_iov = NULL;
		req->read_iov_cnt = 0;
	}

	if (req->r.data) {
		vmm_free(req->r.data);
		req->r.data = NULL;
	}

	virtio_buf_to_iovec_write(dev, &req->status_iov, 1, &status, 1);

	virtio_queue_set_used_elem(req->vq, req->head, req->len);

	if (virtio_queue_should_signal(req->vq)) {
		dev->tra->notify(dev, queueid);
	}
}

static void virtio_blk_req_completed(struct vmm_request *r)
{
	virtio_blk_req_done(r->priv, VIRTIO_BLK_S_OK);
}

static void virtio_blk_req_failed(struct vmm_request *r)
{
	virtio_blk_req_done(r->priv, VIRTIO_BLK_S_IOERR);
}

static void virtio_blk_do_io(struct virtio_device *dev,
			     struct virtio_blk_dev *bdev)
{
	u16 head; 
	u32 i, iov_cnt, len;
	irq_flags_t flags;
	struct virtio_queue *vq = &bdev->vqs[VIRTIO_BLK_IO_QUEUE];
	struct virtio_blk_dev_req *req;
	struct virtio_blk_outhdr hdr;
	struct vmm_blockdev *blk;

	while (virtio_queue_available(vq)) {
		head = virtio_queue_pop(vq);
		req = &bdev->reqs[head];
		head = virtio_queue_get_head_iovec(vq, head, bdev->iov, 
						   &iov_cnt, &len);

		req->vq = vq;
		req->bdev = bdev;
		req->head = head;
		req->read_iov = NULL;
		req->read_iov_cnt = 0;
		req->len = 0;
		for (i = 1; i < (iov_cnt - 1); i++) {
			req->len += bdev->iov[i].len;
		}
		req->status_iov.addr = bdev->iov[iov_cnt - 1].addr;
		req->status_iov.len = bdev->iov[iov_cnt - 1].len;
		req->r.type = VMM_REQUEST_UNKNOWN;
		req->r.lba = 0;
		req->r.bcnt = 0;
		req->r.data = NULL;
		req->r.completed = virtio_blk_req_completed;
		req->r.failed = virtio_blk_req_failed;
		req->r.priv = req;

		len = virtio_iovec_to_buf_read(dev, &bdev->iov[0], 1, 	
						&hdr, sizeof(hdr));
		if (len < sizeof(hdr)) {
			virtio_queue_set_used_elem(req->vq, req->head, 0);
			continue;
		}

		switch (hdr.type) {
		case VIRTIO_BLK_T_IN:
			req->r.type = VMM_REQUEST_READ;
			req->r.lba  = hdr.sector;
			req->r.bcnt = udiv32(req->len, bdev->config.blk_size);
			req->r.data = vmm_malloc(req->len);
			if (!req->r.data) {
				virtio_blk_req_done(req, VIRTIO_BLK_S_IOERR);
				continue;
			}
			len = sizeof(struct virtio_iovec) * (iov_cnt - 2);
			req->read_iov = vmm_malloc(len);
			if (!req->read_iov) {
				virtio_blk_req_done(req, VIRTIO_BLK_S_IOERR);
				continue;
			}
			req->read_iov_cnt = iov_cnt - 2;
			for (i = 0; i < req->read_iov_cnt; i++) {
				req->read_iov[i].addr = bdev->iov[i + 1].addr;
				req->read_iov[i].len = bdev->iov[i + 1].len;
			}
			vmm_spin_lock_irqsave(&bdev->blk_lock, flags);
			blk = bdev->blk;
			vmm_spin_unlock_irqrestore(&bdev->blk_lock, flags);
			/* Note: We will get failed() or complete() callback
			 * even if blk == NULL
			 */
			vmm_blockdev_submit_request(blk, &req->r);
			break;
		case VIRTIO_BLK_T_OUT:
			req->r.type = VMM_REQUEST_WRITE;
			req->r.lba  = hdr.sector;
			req->r.bcnt = udiv32(req->len, bdev->config.blk_size);
			req->r.data = vmm_malloc(req->len);
			if (!req->r.data) {
				virtio_blk_req_done(req, VIRTIO_BLK_S_IOERR);
				continue;
			} else {
				virtio_iovec_to_buf_read(dev, 
							 &bdev->iov[1], 
							 iov_cnt - 2,
							 req->r.data, 
							 req->len);
			}
			vmm_spin_lock_irqsave(&bdev->blk_lock, flags);
			blk = bdev->blk;
			vmm_spin_unlock_irqrestore(&bdev->blk_lock, flags);
			/* Note: We will get failed() or complete() callback
			 * even if blk == NULL
			 */
			vmm_blockdev_submit_request(blk, &req->r);
			break;
		case VIRTIO_BLK_T_FLUSH:
			req->r.type = VMM_REQUEST_WRITE;
			req->r.lba  = 0;
			req->r.bcnt = 0;
			req->r.data = NULL;
			vmm_spin_lock_irqsave(&bdev->blk_lock, flags);
			blk = bdev->blk;
			vmm_spin_unlock_irqrestore(&bdev->blk_lock, flags);
			if (vmm_blockdev_flush_cache(blk)) {
				virtio_blk_req_done(req, VIRTIO_BLK_S_IOERR);
			} else {
				virtio_blk_req_done(req, VIRTIO_BLK_S_OK);
			}
			break;
		case VIRTIO_BLK_T_GET_ID:
			req->len = VIRTIO_BLK_ID_BYTES;
			req->r.type = VMM_REQUEST_READ;
			req->r.lba = 0;
			req->r.bcnt = 0;
			req->r.data = vmm_zalloc(req->len);
			if (!req->r.data) {
				virtio_blk_req_done(req, VIRTIO_BLK_S_IOERR);
				continue;
			}
			req->read_iov = vmm_malloc(sizeof(struct virtio_iovec));
			if (!req->read_iov) {
				virtio_blk_req_done(req, VIRTIO_BLK_S_IOERR);
				continue;
			}
			req->read_iov_cnt = 1;
			req->read_iov[0].addr = bdev->iov[1].addr;
			req->read_iov[0].len = bdev->iov[1].len;
			strlcpy(req->r.data, bdev->blk_name, req->len);
			virtio_blk_req_done(req, VIRTIO_BLK_S_OK);
			break;
		default:
			break;
		};
	}
}

static int virtio_blk_notify_vq(struct virtio_device *dev, u32 vq)
{
	int rc = VMM_OK;
	struct virtio_blk_dev *bdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_BLK_IO_QUEUE:
		virtio_blk_do_io(dev, bdev);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_blk_read_config(struct virtio_device *dev, 
				  u32 offset, void *dst, u32 dst_len)
{
	struct virtio_blk_dev *bdev = dev->emu_data;
	u8 *src = (u8 *)&bdev->config;
	u32 i, src_len = sizeof(bdev->config);

	for (i = 0; (i < dst_len) && ((offset + i) < src_len); i++) {
		((u8 *)dst)[i] = src[offset + i];
	}

	return VMM_OK;
}

static int virtio_blk_write_config(struct virtio_device *dev,
				   u32 offset, void *src, u32 src_len)
{
	struct virtio_blk_dev *bdev = dev->emu_data;
	u8 *dst = (u8 *)&bdev->config;
	u32 i, dst_len = sizeof(bdev->config);

	for (i = 0; (i < src_len) && ((offset + i) < dst_len); i++) {
		dst[offset + i] = ((u8 *)src)[i];
	}

	return VMM_OK;
}

static int virtio_blk_reset(struct virtio_device *dev)
{
	int rc;
	struct virtio_blk_dev *bdev = dev->emu_data;

	rc = virtio_queue_cleanup(&bdev->vqs[VIRTIO_BLK_IO_QUEUE]);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int virtio_blk_notification(struct vmm_notifier_block *nb,
				   unsigned long evt, void *data)
{
	int ret = NOTIFY_DONE;
	irq_flags_t flags;
	struct virtio_blk_dev *bdev = 
			container_of(nb, struct virtio_blk_dev, blk_client);
	struct vmm_blockdev_event *e = data;

	vmm_spin_lock_irqsave(&bdev->blk_lock, flags);

	switch (evt) {
	case VMM_BLOCKDEV_EVENT_REGISTER:
		if (!bdev->blk && !strcmp(e->bdev->name, bdev->blk_name)) {
			bdev->blk = e->bdev;
			bdev->config.capacity = bdev->blk->num_blocks;
			bdev->config.blk_size = bdev->blk->block_size;
			ret = NOTIFY_OK;
		}
		break;
	case VMM_BLOCKDEV_EVENT_UNREGISTER:
		if (bdev->blk == e->bdev) {
			bdev->blk = NULL;
			bdev->config.capacity = 0;
			bdev->config.blk_size = VIRTIO_BLK_SECTOR_SIZE;
			ret = NOTIFY_OK;
		}
		break;
	default:
		break;
	}

	vmm_spin_unlock_irqrestore(&bdev->blk_lock, flags);

	return ret;
}

static int virtio_blk_connect(struct virtio_device *dev, 
			      struct virtio_emulator *emu)
{
	int rc;
	char *attr;
	struct virtio_blk_dev *bdev;

	bdev = vmm_zalloc(sizeof(struct virtio_blk_dev));
	if (!bdev) {
		vmm_printf("Failed to allocate virtio block device....\n");
		return VMM_ENOMEM;
	}
	bdev->vdev = dev;

	bdev->blk_client.notifier_call = &virtio_blk_notification;
	bdev->blk_client.priority = 0;
	rc = vmm_blockdev_register_client(&bdev->blk_client);
	if (rc) {
		vmm_free(bdev);
		return rc;
	}

	INIT_SPIN_LOCK(&bdev->blk_lock);

	attr = vmm_devtree_attrval(dev->edev->node, "blkdev");
	if (attr) {
		if (strlcpy(bdev->blk_name,attr, sizeof(bdev->blk_name)) >=
		    sizeof(bdev->blk_name)) {
			vmm_free(bdev);
			return VMM_EOVERFLOW;
		}
		bdev->blk = vmm_blockdev_find(bdev->blk_name);
	} else {
		bdev->blk_name[0] = 0;
		bdev->blk = NULL;
	}

	bdev->config.capacity = (bdev->blk) ? bdev->blk->num_blocks : 0;
	bdev->config.seg_max = VIRTIO_BLK_DISK_SEG_MAX,
	bdev->config.blk_size = 
		(bdev->blk) ? bdev->blk->block_size : VIRTIO_BLK_SECTOR_SIZE;

	dev->emu_data = bdev;

	return VMM_OK;
}

static void virtio_blk_disconnect(struct virtio_device *dev)
{
	struct virtio_blk_dev *bdev = dev->emu_data;

	vmm_blockdev_unregister_client(&bdev->blk_client);
	vmm_free(bdev);
}

struct virtio_device_id virtio_blk_emu_id[] = {
	{.type = VIRTIO_ID_BLOCK},
	{ },
};

struct virtio_emulator virtio_blk = {
	.name = "virtio_blk",
	.id_table = virtio_blk_emu_id,

	/* VirtIO operations */
	.get_host_features      = virtio_blk_get_host_features,
	.set_guest_features     = virtio_blk_set_guest_features,
	.init_vq                = virtio_blk_init_vq,
	.get_pfn_vq             = virtio_blk_get_pfn_vq,
	.get_size_vq            = virtio_blk_get_size_vq,
	.set_size_vq            = virtio_blk_set_size_vq,
	.notify_vq              = virtio_blk_notify_vq,

	/* Emulator operations */
	.read_config = virtio_blk_read_config,
	.write_config = virtio_blk_write_config,
	.reset = virtio_blk_reset,
	.connect = virtio_blk_connect,
	.disconnect = virtio_blk_disconnect,
};

static int __init virtio_blk_init(void)
{
	return virtio_register_emulator(&virtio_blk);
}

static void __exit virtio_blk_exit(void)
{
	virtio_unregister_emulator(&virtio_blk);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
