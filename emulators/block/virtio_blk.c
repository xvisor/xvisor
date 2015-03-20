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
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vio/vmm_vdisk.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>

#include <emu/virtio.h>
#include <emu/virtio_blk.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

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
	u16				head;
	struct virtio_iovec		*read_iov;
	u32				read_iov_cnt;
	u32				len;
	struct virtio_iovec		status_iov;
	void				*data;
	struct vmm_vdisk_request	r;
};

struct virtio_blk_dev {
	struct virtio_device 		*vdev;

	struct virtio_queue 		vqs[VIRTIO_BLK_NUM_QUEUES];
	struct virtio_iovec		iov[VIRTIO_BLK_QUEUE_SIZE];
	struct virtio_blk_dev_req	reqs[VIRTIO_BLK_QUEUE_SIZE];
	struct virtio_blk_config 	config;
	u32 				features;

	struct vmm_vdisk		*vdisk;
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
	struct virtio_blk_dev *vbdev = dev->emu_data;

	vbdev->features = features;
}

static int virtio_blk_init_vq(struct virtio_device *dev,
			      u32 vq, u32 page_size, u32 align,
			      u32 pfn)
{
	int rc;
	struct virtio_blk_dev *vbdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_BLK_IO_QUEUE:
		rc = virtio_queue_setup(&vbdev->vqs[vq], dev->guest,
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
	struct virtio_blk_dev *vbdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_BLK_IO_QUEUE:
		rc = virtio_queue_guest_pfn(&vbdev->vqs[vq]);
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

static void virtio_blk_req_done(struct virtio_blk_dev *vbdev,
				struct virtio_blk_dev_req *req, u8 status)
{
	struct virtio_device *dev = vbdev->vdev;
	int queueid = req->vq - vbdev->vqs;

	if (req->read_iov && req->len && req->data &&
	    (status == VIRTIO_BLK_S_OK) &&
	    (vmm_vdisk_get_request_type(&req->r) == VMM_VDISK_REQUEST_READ)) {
		virtio_buf_to_iovec_write(dev,
					  req->read_iov,
					  req->read_iov_cnt,
					  req->data,
					  req->len);
	}

	if (req->read_iov) {
		vmm_free(req->read_iov);
		req->read_iov = NULL;
		req->read_iov_cnt = 0;
	}

	vmm_vdisk_set_request_type(&req->r, VMM_VDISK_REQUEST_UNKNOWN);
	if (req->data) {
		vmm_free(req->data);
		req->data = NULL;
	}

	virtio_buf_to_iovec_write(dev, &req->status_iov, 1, &status, 1);

	virtio_queue_set_used_elem(req->vq, req->head, req->len);

	if (virtio_queue_should_signal(req->vq)) {
		dev->tra->notify(dev, queueid);
	}
}

static void virtio_blk_attached(struct vmm_vdisk *vdisk)
{
	struct virtio_blk_dev *vbdev = vmm_vdisk_priv(vdisk);

	DPRINTF("%s: vdisk=%s\n",
		__func__, vmm_vdisk_name(vdisk));

	vbdev->config.capacity = vmm_vdisk_capacity(vbdev->vdisk);
	vbdev->config.seg_max = VIRTIO_BLK_DISK_SEG_MAX,
	vbdev->config.blk_size = vmm_vdisk_block_size(vbdev->vdisk);
}

static void virtio_blk_detached(struct vmm_vdisk *vdisk)
{
	struct virtio_blk_dev *vbdev = vmm_vdisk_priv(vdisk);

	DPRINTF("%s: vdisk=%s\n",
		__func__, vmm_vdisk_name(vdisk));

	vbdev->config.capacity = 0;
	vbdev->config.seg_max = VIRTIO_BLK_DISK_SEG_MAX,
	vbdev->config.blk_size = VIRTIO_BLK_SECTOR_SIZE;
}

static void virtio_blk_req_completed(struct vmm_vdisk *vdisk,
				     struct vmm_vdisk_request *vreq)
{
	DPRINTF("%s: vdisk=%s\n",
		__func__, vmm_vdisk_name(vdisk));

	virtio_blk_req_done(vmm_vdisk_priv(vdisk),
			    container_of(vreq, struct virtio_blk_dev_req, r),
			    VIRTIO_BLK_S_OK);
}

static void virtio_blk_req_failed(struct vmm_vdisk *vdisk,
				  struct vmm_vdisk_request *vreq)
{
	DPRINTF("%s: vdisk=%s\n",
		__func__, vmm_vdisk_name(vdisk));

	virtio_blk_req_done(vmm_vdisk_priv(vdisk),
			    container_of(vreq, struct virtio_blk_dev_req, r),
			    VIRTIO_BLK_S_IOERR);
}

static void virtio_blk_do_io(struct virtio_device *dev,
			     struct virtio_blk_dev *vbdev)
{
	u16 head;
	u32 i, iov_cnt, len;
	struct virtio_queue *vq = &vbdev->vqs[VIRTIO_BLK_IO_QUEUE];
	struct virtio_blk_dev_req *req;
	struct virtio_blk_outhdr hdr;

	while (virtio_queue_available(vq)) {
		head = virtio_queue_pop(vq);
		req = &vbdev->reqs[head];
		head = virtio_queue_get_head_iovec(vq, head, vbdev->iov,
						   &iov_cnt, &len);

		req->vq = vq;
		req->head = head;
		req->read_iov = NULL;
		req->read_iov_cnt = 0;
		req->len = 0;
		for (i = 1; i < (iov_cnt - 1); i++) {
			req->len += vbdev->iov[i].len;
		}
		req->status_iov.addr = vbdev->iov[iov_cnt - 1].addr;
		req->status_iov.len = vbdev->iov[iov_cnt - 1].len;
		vmm_vdisk_set_request_type(&req->r, VMM_VDISK_REQUEST_UNKNOWN);

		len = virtio_iovec_to_buf_read(dev, &vbdev->iov[0], 1,
						&hdr, sizeof(hdr));
		if (len < sizeof(hdr)) {
			virtio_queue_set_used_elem(req->vq, req->head, 0);
			continue;
		}

		switch (hdr.type) {
		case VIRTIO_BLK_T_IN:
			vmm_vdisk_set_request_type(&req->r,
						   VMM_VDISK_REQUEST_READ);
			req->data = vmm_malloc(req->len);
			if (!req->data) {
				virtio_blk_req_done(vbdev, req,
						    VIRTIO_BLK_S_IOERR);
				continue;
			}
			len = sizeof(struct virtio_iovec) * (iov_cnt - 2);
			req->read_iov = vmm_malloc(len);
			if (!req->read_iov) {
				virtio_blk_req_done(vbdev, req,
						    VIRTIO_BLK_S_IOERR);
				continue;
			}
			req->read_iov_cnt = iov_cnt - 2;
			for (i = 0; i < req->read_iov_cnt; i++) {
				req->read_iov[i].addr = vbdev->iov[i + 1].addr;
				req->read_iov[i].len = vbdev->iov[i + 1].len;
			}
			DPRINTF("%s: VIRTIO_BLK_T_IN dev=%s "
				"hdr.sector=%ll req->len=%d\n",
				__func__, dev->name,
				(u64)hdr.sector, req->len);
			/* Note: We will get failed() or complete() callback
			 * even when no block device attached to virtual disk
			 */
			vmm_vdisk_submit_request(vbdev->vdisk, &req->r,
						 VMM_VDISK_REQUEST_READ,
						 hdr.sector, req->data, req->len);
			break;
		case VIRTIO_BLK_T_OUT:
			vmm_vdisk_set_request_type(&req->r,
						   VMM_VDISK_REQUEST_WRITE);
			req->data = vmm_malloc(req->len);
			if (!req->data) {
				virtio_blk_req_done(vbdev, req,
						    VIRTIO_BLK_S_IOERR);
				continue;
			} else {
				virtio_iovec_to_buf_read(dev,
							 &vbdev->iov[1],
							 iov_cnt - 2,
							 req->data,
							 req->len);
			}
			DPRINTF("%s: VIRTIO_BLK_T_OUT dev=%s "
				"hdr.sector=%ll req->len=%d\n",
				__func__, dev->name,
				(u64)hdr.sector, req->len);
			/* Note: We will get failed() or complete() callback
			 * even when no block device attached to virtual disk
			 */
			vmm_vdisk_submit_request(vbdev->vdisk, &req->r,
						 VMM_VDISK_REQUEST_WRITE,
						 hdr.sector, req->data, req->len);
			break;
		case VIRTIO_BLK_T_FLUSH:
			vmm_vdisk_set_request_type(&req->r,
						   VMM_VDISK_REQUEST_WRITE);
			DPRINTF("%s: VIRTIO_BLK_T_FLUSH dev=%s\n",
				__func__, dev->name);
			if (vmm_vdisk_flush_cache(vbdev->vdisk)) {
				virtio_blk_req_done(vbdev, req,
						    VIRTIO_BLK_S_IOERR);
			} else {
				virtio_blk_req_done(vbdev, req,
						    VIRTIO_BLK_S_OK);
			}
			break;
		case VIRTIO_BLK_T_GET_ID:
			vmm_vdisk_set_request_type(&req->r,
						   VMM_VDISK_REQUEST_READ);
			req->len = VIRTIO_BLK_ID_BYTES;
			req->data = vmm_zalloc(req->len);
			if (!req->data) {
				virtio_blk_req_done(vbdev, req,
						    VIRTIO_BLK_S_IOERR);
				continue;
			}
			req->read_iov = vmm_malloc(sizeof(struct virtio_iovec));
			if (!req->read_iov) {
				virtio_blk_req_done(vbdev, req,
						    VIRTIO_BLK_S_IOERR);
				continue;
			}
			req->read_iov_cnt = 1;
			req->read_iov[0].addr = vbdev->iov[1].addr;
			req->read_iov[0].len = vbdev->iov[1].len;
			DPRINTF("%s: VIRTIO_BLK_T_GET_ID dev=%s req->len=%d\n",
				__func__, dev->name, req->len);
			if (vmm_vdisk_current_block_device(vbdev->vdisk,
							req->data, req->len)) {
				virtio_blk_req_done(vbdev, req,
						    VIRTIO_BLK_S_IOERR);
			} else {
				virtio_blk_req_done(vbdev, req,
						    VIRTIO_BLK_S_OK);
			}
			break;
		default:
			break;
		};
	}
}

static int virtio_blk_notify_vq(struct virtio_device *dev, u32 vq)
{
	int rc = VMM_OK;
	struct virtio_blk_dev *vbdev = dev->emu_data;

	DPRINTF("%s: dev=%s vq=%d\n", __func__, dev->name, vq);

	switch (vq) {
	case VIRTIO_BLK_IO_QUEUE:
		virtio_blk_do_io(dev, vbdev);
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
	u32 i;
	struct virtio_blk_dev *vbdev = dev->emu_data;
	u8 *src = (u8 *)&vbdev->config;

	DPRINTF("%s: dev=%s offset=%d dst=%p dst_len=%d\n",
		__func__, dev->name, offset, dst, dst_len);

	for (i=0; (i<dst_len) && ((offset+i) < sizeof(vbdev->config)); i++) {
		((u8 *)dst)[i] = src[offset + i];
	}

	return VMM_OK;
}

static int virtio_blk_write_config(struct virtio_device *dev,
				   u32 offset, void *src, u32 src_len)
{
	u32 i;
	struct virtio_blk_dev *vbdev = dev->emu_data;
	u8 *dst = (u8 *)&vbdev->config;

	DPRINTF("%s: dev=%s offset=%d src=%p src_len=%d\n",
		__func__, dev->name, offset, src, src_len);

	for (i=0; (i<src_len) && ((offset+i) < sizeof(vbdev->config)); i++) {
		dst[offset + i] = ((u8 *)src)[i];
	}

	return VMM_OK;
}

static int virtio_blk_reset(struct virtio_device *dev)
{
	int i, rc;
	struct virtio_blk_dev_req *req;
	struct virtio_blk_dev *vbdev = dev->emu_data;

	DPRINTF("%s: dev=%s\n", __func__, dev->name);

	for (i = 0; i < VIRTIO_BLK_QUEUE_SIZE; i++) {
		req = &vbdev->reqs[i];
		if (vmm_vdisk_get_request_type(&req->r) !=
					VMM_VDISK_REQUEST_UNKNOWN) {
			vmm_vdisk_abort_request(vbdev->vdisk, &req->r);
		}
		memset(req, 0, sizeof(*req));
		vmm_vdisk_set_request_type(&req->r,
					   VMM_VDISK_REQUEST_UNKNOWN);
	}

	rc = virtio_queue_cleanup(&vbdev->vqs[VIRTIO_BLK_IO_QUEUE]);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int virtio_blk_connect(struct virtio_device *dev,
			      struct virtio_emulator *emu)
{
	const char *attr;
	struct virtio_blk_dev *vbdev;

	DPRINTF("%s: dev=%s emu=%s\n", __func__, dev->name, emu->name);

	vbdev = vmm_zalloc(sizeof(struct virtio_blk_dev));
	if (!vbdev) {
		vmm_printf("Failed to allocate virtio block device....\n");
		return VMM_ENOMEM;
	}
	vbdev->vdev = dev;

	vbdev->config.capacity = 0;
	vbdev->config.seg_max = VIRTIO_BLK_DISK_SEG_MAX,
	vbdev->config.blk_size = VIRTIO_BLK_SECTOR_SIZE;

	vbdev->vdisk = vmm_vdisk_create(dev->name, VIRTIO_BLK_SECTOR_SIZE,
					virtio_blk_attached,
					virtio_blk_detached,
					virtio_blk_req_completed,
					virtio_blk_req_failed,
					vbdev);
	if (!vbdev->vdisk) {
		vmm_free(vbdev);
		return VMM_EFAIL;
	}

	/* Attach block device */
	if (vmm_devtree_read_string(dev->edev->node,
				    "blkdev", &attr) != VMM_OK) {
		attr = NULL;
	}
	vmm_vdisk_attach_block_device(vbdev->vdisk, attr);

	dev->emu_data = vbdev;

	return VMM_OK;
}

static void virtio_blk_disconnect(struct virtio_device *dev)
{
	struct virtio_blk_dev *vbdev = dev->emu_data;

	DPRINTF("%s: dev=%s\n", __func__, dev->name);

	vmm_vdisk_destroy(vbdev->vdisk);
	vmm_free(vbdev);
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
