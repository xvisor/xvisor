/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file virtio_host_blk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO host block device driver.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_spinlocks.h>
#include <vmm_completion.h>
#include <vmm_modules.h>
#include <block/vmm_blockrq.h>
#include <block/vmm_blockdev.h>
#include <vio/vmm_virtio_blk.h>
#include <drv/virtio_host.h>
#include <libs/fifo.h>
#include <libs/idr.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(vblk, ...)		vmm_linfo((vblk)->vdev->dev.name, \
						  __VA_ARGS__)
#else
#define DPRINTF(vblk, ...)
#endif

#define MODULE_DESC			"VirtIO Host Block Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VIRTIO_HOST_IPRIORITY + 1)
#define	MODULE_INIT			virtio_host_blk_init
#define	MODULE_EXIT			virtio_host_blk_exit

struct virtio_host_blk_req {
	struct vmm_request *r;
	struct vmm_completion *cmpl;
	struct virtio_host_blk *vblk;
	struct vmm_virtio_blk_outhdr hdr;
	struct virtio_host_iovec iovec[2];
	struct virtio_host_iovec *ivs[2];
};

struct virtio_host_blk {
	int index;
	struct virtio_host_device *vdev;

	bool read_only;
	u64 num_blocks;
	u32 block_size;
	u32 seg_size;

	u16 num_vqs;
	struct virtio_host_queue **vqs;

	u32 max_reqs;
	struct virtio_host_blk_req *reqs;
	struct fifo *reqs_fifo;

	u8 raw_serial[VMM_VIRTIO_BLK_ID_BYTES];
	char serial[VMM_VIRTIO_BLK_ID_BYTES*2 + 1];

	struct vmm_blockrq *brq;
	struct vmm_blockdev *bdev;
};

static DEFINE_IDA(vd_index_ida);

static int virtio_host_blk_read(struct vmm_blockrq *brq,
				struct vmm_request *r, void *priv)
{
	int rc;
	struct virtio_host_blk *vblk = priv;
	struct virtio_host_blk_req *req;

	if (!fifo_dequeue(vblk->reqs_fifo, &req)) {
		vmm_lerror(vblk->vdev->dev.name,
			   "Failed to dequeue free request\n");
		return VMM_EIO;
	}

	req->r = r;
	req->cmpl = NULL;
	req->hdr.type = cpu_to_virtio32(vblk->vdev, VMM_VIRTIO_BLK_T_IN);
	req->hdr.ioprio = 0;
	req->hdr.sector = cpu_to_virtio64(vblk->vdev, r->lba);
	req->iovec[1].buf = r->data;
	req->iovec[1].buf_len = r->bcnt * vblk->block_size;

	DPRINTF(vblk, "%s: req=0x%p lba=%"PRIu64" bcnt=%d data=0x%p\n",
		__func__, req, req->r->lba, req->r->bcnt, req->r->data);

	rc = virtio_host_queue_add_iovecs(vblk->vqs[0], req->ivs, 1, 1, req);
	if (rc) {
		vmm_lerror(vblk->vdev->dev.name,
			   "Failed to add iovecs to VirtIO host queue\n");
		req->r = NULL;
		req->cmpl = NULL;
		fifo_enqueue(vblk->reqs_fifo, &req, TRUE);
		return rc;
	}

	virtio_host_queue_kick(vblk->vqs[0]);

	return VMM_OK;
}

static int virtio_host_blk_write(struct vmm_blockrq *brq,
				 struct vmm_request *r, void *priv)
{
	int rc;
	struct virtio_host_blk *vblk = priv;
	struct virtio_host_blk_req *req;

	if (!fifo_dequeue(vblk->reqs_fifo, &req)) {
		vmm_lerror(vblk->vdev->dev.name,
			   "Failed to dequeue free request\n");
		return VMM_EIO;
	}

	req->r = r;
	req->cmpl = NULL;
	req->hdr.type = cpu_to_virtio32(vblk->vdev, VMM_VIRTIO_BLK_T_OUT);
	req->hdr.ioprio = 0;
	req->hdr.sector = cpu_to_virtio64(vblk->vdev, r->lba);
	req->iovec[1].buf = r->data;
	req->iovec[1].buf_len = r->bcnt * vblk->block_size;

	DPRINTF(vblk, "%s: req=0x%p lba=%"PRIu64" bcnt=%d data=0x%p\n",
		__func__, req, req->r->lba, req->r->bcnt, req->r->data);

	rc = virtio_host_queue_add_iovecs(vblk->vqs[0], req->ivs, 1, 1, req);
	if (rc) {
		vmm_lerror(vblk->vdev->dev.name,
			   "Failed to add iovecs to VirtIO host queue\n");
		req->r = NULL;
		req->cmpl = NULL;
		fifo_enqueue(vblk->reqs_fifo, &req, TRUE);
		return rc;
	}

	virtio_host_queue_kick(vblk->vqs[0]);

	return VMM_OK;
}

static void virtio_host_blk_flush(struct vmm_blockrq *brq, void *priv)
{
	/* TODO: */
}

#define VIRTIO_HOST_BLK_DONE_BUDGET	8

static void virtio_host_blk_done_work(struct vmm_blockrq *brq, void *priv)
{
	int err;
	unsigned int i, len, exp;
	struct virtio_host_blk *vblk = priv;
	struct virtio_host_blk_req *req;

	i = 0;
	do {
		req = virtio_host_queue_get_buf(vblk->vqs[0], &len);
		if (!req) {
			break;
		}

		if (req->r) {
			DPRINTF(vblk, "%s: req=0x%p lba=%"PRIu64" "
				"bcnt=%d data=0x%p\n", __func__,
				req, req->r->lba, req->r->bcnt, req->r->data);

			exp = sizeof(req->hdr);
			exp += req->r->bcnt * vblk->block_size;

			DPRINTF(vblk, "%s: req=0x%p exp=%d len=%d\n",
				__func__, req, exp, len);

			err = (len == exp) ? VMM_OK : VMM_EIO;
			vmm_blockrq_async_done(vblk->brq, req->r, err);
		} else if (req->cmpl) {
			DPRINTF(vblk, "%s: req=0x%p cmpl=0x%p len=%d\n",
				__func__, req, req->cmpl, len);

			vmm_completion_complete(req->cmpl);
		} else {
			DPRINTF(vblk, "%s: req=0x%p len=%d\n",
				__func__, req, req->cmpl, len);
		}

		req->r = NULL;
		req->cmpl = NULL;
		fifo_enqueue(vblk->reqs_fifo, &req, TRUE);
	} while (i < VIRTIO_HOST_BLK_DONE_BUDGET);

	if (virtio_host_queue_have_buf(vblk->vqs[0]))
		vmm_blockrq_queue_work(vblk->brq,
				       virtio_host_blk_done_work, vblk);
}

static void virtio_host_blk_done(struct virtio_host_queue *vq)
{
	struct virtio_host_blk *vblk = vq->vdev->priv;

	vmm_blockrq_queue_work(vblk->brq, virtio_host_blk_done_work, vblk);
}

static void virtio_host_blk_read_serial(struct virtio_host_blk *vblk)
{
	u8 sc;
	int i, rc;
	struct vmm_completion cmpl;
	struct virtio_host_blk_req *req;
	const char hexchar[] = "0123456789abcdef";

	if (!fifo_dequeue(vblk->reqs_fifo, &req)) {
		vmm_lerror(vblk->vdev->dev.name,
			   "Failed to dequeue free request\n");
		return;
	}

	INIT_COMPLETION(&cmpl);

	DPRINTF(vblk, "%s: req=0x%p cmpl=0x%p\n",
		__func__, req, &cmpl);

	req->r = NULL;
	req->cmpl = &cmpl;
	req->hdr.type = cpu_to_virtio32(vblk->vdev, VMM_VIRTIO_BLK_T_GET_ID);
	req->hdr.ioprio = 0;
	req->hdr.sector = 0;
	req->iovec[1].buf = vblk->raw_serial;
	req->iovec[1].buf_len = VMM_VIRTIO_BLK_ID_BYTES;

	rc = virtio_host_queue_add_iovecs(vblk->vqs[0], req->ivs, 1, 1, req);
	if (rc) {
		vmm_lerror(vblk->vdev->dev.name,
			   "Failed to add iovecs to VirtIO host queue\n");
		req->r = NULL;
		req->cmpl = NULL;
		fifo_enqueue(vblk->reqs_fifo, &req, TRUE);
	}

	virtio_host_queue_kick(vblk->vqs[0]);

	vmm_completion_wait(&cmpl);

	for (i = 0; i < VMM_VIRTIO_BLK_ID_BYTES; i++) {
		sc = vblk->raw_serial[VMM_VIRTIO_BLK_ID_BYTES - i - 1];
		vblk->serial[2*i] = hexchar[sc & 0xf];
		vblk->serial[2*i + 1] = hexchar[(sc >> 16) & 0xf];
	}
}

static int virtio_host_blk_init_pool(struct virtio_host_blk *vblk)
{
	int i;
	struct virtio_host_blk_req *req;

	/* Setup max requests count
	 * Note: We don't use indirect descriptors so
	 * max requests count is half available descriptors
	 */
	vblk->max_reqs = vblk->vqs[0]->num_free / 2;

	vblk->reqs = vmm_zalloc(vblk->max_reqs * sizeof(*vblk->reqs));
	if (!vblk->reqs)
		return VMM_ENOMEM;

	vblk->reqs_fifo = fifo_alloc(sizeof(void *), vblk->max_reqs);
	if (!vblk->reqs_fifo) {
		vmm_free(vblk->reqs);
		return VMM_ENOMEM;
	}

	for (i = 0; i < vblk->max_reqs; i++) {
		req = &vblk->reqs[i];
		req->r = NULL;
		req->cmpl = NULL;
		req->vblk = vblk;
		req->iovec[0].buf = &req->hdr;
		req->iovec[0].buf_len = sizeof(req->hdr);
		req->iovec[1].buf = NULL;
		req->iovec[1].buf_len = 0;
		req->ivs[0] = &req->iovec[0];
		req->ivs[1] = &req->iovec[1];
		fifo_enqueue(vblk->reqs_fifo, &req, TRUE);
	}

	return VMM_OK;
}

static void virtio_host_blk_cleanup_pool(struct virtio_host_blk *vblk)
{
	fifo_free(vblk->reqs_fifo);
	vmm_free(vblk->reqs);
}

static int virtio_host_blk_init_vqs(struct virtio_host_blk *vblk)
{
	int i, rc = VMM_OK;
	virtio_host_queue_callback_t *callbacks = NULL;
	char **names;

	rc = virtio_cread_feature(vblk->vdev, VMM_VIRTIO_BLK_F_MQ,
				  struct vmm_virtio_blk_config, num_queues,
				  &vblk->num_vqs);
	if (rc)
		vblk->num_vqs = 1;

	vblk->vqs = vmm_zalloc(vblk->num_vqs * sizeof(*vblk->vqs));
	if (!vblk->vqs) {
		rc = VMM_ENOMEM;
		goto fail;
	}

	callbacks = vmm_zalloc(vblk->num_vqs * sizeof(*callbacks));
	if (!callbacks) {
		rc = VMM_ENOMEM;
		goto fail_free_vqs;
	}

	names = vmm_zalloc(vblk->num_vqs * sizeof(*names));
	if (!names) {
		rc = VMM_ENOMEM;
		goto fail_free_callbacks;
	}

	for (i = 0; i < vblk->num_vqs; i++) {
		vblk->vqs[i] = NULL;
		callbacks[i] = virtio_host_blk_done;
		names[i] = vmm_zalloc(VMM_FIELD_NAME_SIZE);
		if (!names[i]) {
			rc = VMM_ENOMEM;
			goto fail_free_names;
		}
		vmm_snprintf(names[i], VMM_FIELD_NAME_SIZE, "vblk.%d", i);
	}

	rc = virtio_host_find_vqs(vblk->vdev, vblk->num_vqs,
				  vblk->vqs, callbacks, names);
	if (rc) {
		goto fail_free_names;
	}

	for (i = 0; i < vblk->num_vqs; i++) {
		if (!names[i]) {
			continue;
		}
		vmm_free(names[i]);
		names[i] = NULL;
	}
	vmm_free(names);
	vmm_free(callbacks);

	return VMM_OK;

fail_free_names:
	for (i = 0; i < vblk->num_vqs; i++) {
		if (!names[i]) {
			continue;
		}
		vmm_free(names[i]);
		names[i] = NULL;
	}
	vmm_free(names);
fail_free_callbacks:
	vmm_free(callbacks);
fail_free_vqs:
	vmm_free(vblk->vqs);
fail:
	return rc;
}

static void virtio_host_blk_cleanup_vqs(struct virtio_host_blk *vblk)
{
	virtio_host_del_vqs(vblk->vdev);
	vmm_free(vblk->vqs);
}

static int virtio_host_blk_name_format(char *prefix, int index,
				       char *buf, int buflen)
{
	const int base = 'z' - 'a' + 1;
	char *begin = buf + strlen(prefix);
	char *end = buf + buflen;
	char *p;
	int unit;

	p = end - 1;
	*p = '\0';
	unit = base;
	do {
		if (p == begin)
			return VMM_EINVALID;
		*--p = 'a' + umod32(index, unit);
		index = udiv32(index, unit) - 1;
	} while (index >= 0);

	memmove(begin, p, end - p);
	memcpy(buf, prefix, strlen(prefix));

	return 0;
}

static int virtio_host_blk_probe(struct virtio_host_device *vdev)
{
	int rc = VMM_OK;
	struct virtio_host_blk *vblk;

	/* Allocate virtio host block device */
	vblk = vmm_zalloc(sizeof(*vblk));
	if (!vblk) {
		vmm_lerror(vdev->dev.name,
			   "failed to alloc virtio_host_blk\n");
		return VMM_ENOMEM;
	}

	/* Assign an unique index and hence name. */
	rc = ida_simple_get(&vd_index_ida, 0, 0, 0);
	if (rc < 0) {
		vmm_lerror(vdev->dev.name,
			   "failed to alloc virtio_host_blk index\n");
		goto fail_free_vblk;
	}
	vblk->index = rc;
	vblk->vdev = vdev;

	/* If disk is read-only in the host, then we should obey */
	if (virtio_host_has_feature(vdev, VMM_VIRTIO_BLK_F_RO)) {
		vblk->read_only = true;
	} else {
		vblk->read_only = false;
	}

	/* Host must always specify the capacity. */
	virtio_cread(vdev, struct vmm_virtio_blk_config, capacity,
		     &vblk->num_blocks);
	if (!vblk->num_blocks) {
		vmm_linfo(vdev->dev.name,
			  "zero capacity hence no block device\n");
		rc = VMM_ENODEV;
		goto fail_free_index;
	}

	/* Host can optionally specify maximum segment size and number of
	 * segments. */
	rc = virtio_cread_feature(vdev, VMM_VIRTIO_BLK_F_SIZE_MAX,
				  struct vmm_virtio_blk_config, size_max,
				  &vblk->seg_size);
	if (rc) {
		vblk->seg_size = U32_MAX;
	}

	/* Host can optionally specify the block size of the device */
	rc = virtio_cread_feature(vdev, VMM_VIRTIO_BLK_F_BLK_SIZE,
				  struct vmm_virtio_blk_config, blk_size,
				  &vblk->block_size);
	if (rc) {
		vblk->block_size = 512;
	}
	if (vblk->block_size != 512) {
		vblk->num_blocks = udiv64((vblk->num_blocks * 512),
					  vblk->block_size);
	}

	/* Setup VirtIO host queues */
	rc = virtio_host_blk_init_vqs(vblk);
	if (rc) {
		vmm_lerror(vdev->dev.name,
			   "failed to setup virtio_host queues\n");
		goto fail_free_index;
	}

	/* Setup requests pool */
	rc = virtio_host_blk_init_pool(vblk);
	if (rc) {
		vmm_lerror(vdev->dev.name,
			   "failed to setup requests pool\n");
		goto fail_free_vqs;
	}

	/* Allocate block device instance */
	vblk->bdev = vmm_blockdev_alloc();
	if (!vblk->bdev) {
		vmm_lerror(vdev->dev.name,
			   "failed to alloc block device\n");
		rc = VMM_ENOMEM;
		goto fail_free_pool;
	}

	/* Setup block device instance */
	rc = virtio_host_blk_name_format("vd", vblk->index,
					 vblk->bdev->name,
					 sizeof(vblk->bdev->name));
	if (rc) {
		vmm_lerror(vdev->dev.name,
			   "failed to generate block device name\n");
		goto fail_free_bdev;
	}
	vmm_snprintf(vblk->bdev->desc, sizeof(vblk->bdev->desc),
		     "VirtIO host block device");
	vblk->bdev->dev.parent = &vblk->vdev->dev;
	vblk->bdev->flags = (vblk->read_only) ?
			 VMM_BLOCKDEV_RDONLY : VMM_BLOCKDEV_RW;
	vblk->bdev->start_lba = 0;
	vblk->bdev->num_blocks = vblk->num_blocks;
	vblk->bdev->block_size = vblk->block_size;

	/* Setup request queue for block device instance */
	vblk->brq = vmm_blockrq_create(vblk->bdev->name,
				       vblk->max_reqs, TRUE,
				       virtio_host_blk_read,
				       virtio_host_blk_write,
				       NULL,
				       virtio_host_blk_flush,
				       vblk);
	if (!vblk->brq) {
		vmm_lerror(vdev->dev.name,
			   "failed to create block device request queue\n");
		goto fail_free_bdev;
	}
	vblk->bdev->rq = vmm_blockrq_to_rq(vblk->brq);

	/* Register block device instance */
	rc = vmm_blockdev_register(vblk->bdev);
	if (rc) {
		vmm_lerror(vdev->dev.name,
			   "failed to register block device\n");
		goto fail_free_brq;
	}

	/* Save VirtIO host block pointer in VirtIO device */
	vblk->vdev->priv = vblk;

	/* Make VirtIO device ready */
	virtio_host_device_ready(vblk->vdev);

	/* Read serial number */
	virtio_host_blk_read_serial(vblk);
	DPRINTF(vblk, "max_reqs=%d serial=%s\n",
		vblk->max_reqs, vblk->serial);

	/* Announce presence of VirtIO host block device */
	vmm_linfo(vdev->dev.name,
		  "blockdev=%s num_blocks=%"PRIu64" blk_size=%d\n",
		  vblk->bdev->name, vblk->num_blocks, vblk->block_size);

	return VMM_OK;

fail_free_brq:
	vmm_blockrq_destroy(vblk->brq);
fail_free_bdev:
	vmm_blockdev_free(vblk->bdev);
fail_free_pool:
	virtio_host_blk_cleanup_pool(vblk);
fail_free_vqs:
	virtio_host_blk_cleanup_vqs(vblk);
fail_free_index:
	ida_simple_remove(&vd_index_ida, vblk->index);
fail_free_vblk:
	vmm_free(vblk);
	return rc;
}

static void virtio_host_blk_remove(struct virtio_host_device *vdev)
{
	struct virtio_host_blk *vblk = vdev->priv;

	virtio_host_device_reset(vblk->vdev);
	vmm_blockdev_unregister(vblk->bdev);
	vmm_blockrq_destroy(vblk->brq);
	vmm_blockdev_free(vblk->bdev);
	virtio_host_blk_cleanup_pool(vblk);
	virtio_host_blk_cleanup_vqs(vblk);
	ida_simple_remove(&vd_index_ida, vblk->index);
	vmm_free(vblk);
	vdev->priv = NULL;
}

static struct virtio_host_device_id virtio_host_blk_devid_table[] = {
	{ VMM_VIRTIO_ID_BLOCK, VMM_VIRTIO_ID_ANY },
	{ 0 },
};

static unsigned int features_legacy[] = {
	VMM_VIRTIO_BLK_F_SEG_MAX,
	VMM_VIRTIO_BLK_F_SIZE_MAX,
	VMM_VIRTIO_BLK_F_GEOMETRY,
	VMM_VIRTIO_BLK_F_RO,
	VMM_VIRTIO_BLK_F_BLK_SIZE,
	VMM_VIRTIO_BLK_F_FLUSH,
}
;
static unsigned int features[] = {
	VMM_VIRTIO_BLK_F_SEG_MAX,
	VMM_VIRTIO_BLK_F_SIZE_MAX,
	VMM_VIRTIO_BLK_F_GEOMETRY,
	VMM_VIRTIO_BLK_F_RO,
	VMM_VIRTIO_BLK_F_BLK_SIZE,
	VMM_VIRTIO_BLK_F_FLUSH,
};

static struct virtio_host_driver virtio_host_blk_driver = {
	.name = "virtio_host_blk",
	.id_table = virtio_host_blk_devid_table,
	.feature_table = features,
	.feature_table_size = array_size(features),
	.feature_table_legacy = features_legacy,
	.feature_table_size_legacy = array_size(features_legacy),
	.probe = virtio_host_blk_probe,
	.remove = virtio_host_blk_remove,
};

static int __init virtio_host_blk_init(void)
{
	return virtio_host_register_driver(&virtio_host_blk_driver);
}

static void __exit virtio_host_blk_exit(void)
{
	virtio_host_unregister_driver(&virtio_host_blk_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
