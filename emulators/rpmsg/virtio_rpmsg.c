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
 * @file virtio_rpmsg.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO based rpmsg Emulator.
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_host_aspace.h>
#include <vmm_devemu.h>
#include <vio/vmm_vmsg.h>
#include <vio/vmm_virtio.h>
#include <vio/vmm_virtio_rpmsg.h>
#include <libs/mempool.h>
#include <libs/stringlib.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC			"VirtIO Rpmsg Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VMSG_IPRIORITY + \
					 VMM_VIRTIO_IPRIORITY + 1)
#define MODULE_INIT			virtio_rpmsg_init
#define MODULE_EXIT			virtio_rpmsg_exit

#define VIRTIO_RPMSG_MAX_BUFF_SIZE	512
#define VIRTIO_RPMSG_NODE_MAX_BUFF_SIZE	\
	(VIRTIO_RPMSG_MAX_BUFF_SIZE - sizeof(struct vmm_rpmsg_hdr))

#define VIRTIO_RPMSG_QUEUE_SIZE		256
#define VIRTIO_RPMSG_NUM_QUEUES		2
#define VIRTIO_RPMSG_RX_QUEUE		0
#define VIRTIO_RPMSG_TX_QUEUE		1

#define VIRTIO_RPMSG_BUF_SET_USED_TX	(1 << 0)
#define VIRTIO_RPMSG_BUF_NOTIFY_TX	(1 << 1)

struct virtio_rpmsg_buf {
	u32 flags;
	u16 head;
	u32 total_len;
	struct vmm_vmsg msg;
	char data[VIRTIO_RPMSG_MAX_BUFF_SIZE];
};

struct virtio_rpmsg_dev {
	struct vmm_virtio_device *vdev;

	struct vmm_virtio_queue vqs[VIRTIO_RPMSG_NUM_QUEUES];
	struct vmm_virtio_iovec rx_iov[VIRTIO_RPMSG_QUEUE_SIZE];
	struct vmm_virtio_iovec tx_iov[VIRTIO_RPMSG_QUEUE_SIZE];

	struct mempool *tx_buf_pool;

	u64 features;

	char name[VMM_VIRTIO_DEVICE_MAX_NAME_LEN];
	bool node_ns_name_avail;
	char node_ns_name[VMM_VIRTIO_RPMSG_NS_NAME_SIZE];
	struct vmm_vmsg_node *node;
	struct vmm_vmsg_node_lazy tx_lazy;
};

static u64 virtio_rpmsg_get_host_features(struct vmm_virtio_device *dev)
{
	return 1UL << VMM_VIRTIO_RPMSG_F_NS;
}

static void virtio_rpmsg_set_guest_features(struct vmm_virtio_device *dev,
					    u32 select, u32 features)
{
	struct virtio_rpmsg_dev *rdev = dev->emu_data;

	if (1 < select)
		return;

	rdev->features &= ~((u64)UINT_MAX << (select * 32));
	rdev->features |= ((u64)features << (select * 32));
}

static int virtio_rpmsg_init_vq(struct vmm_virtio_device *dev,
				u32 vq, u32 page_size, u32 align, u32 pfn)
{
	int rc;
	struct virtio_rpmsg_dev *rdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_RPMSG_RX_QUEUE:
	case VIRTIO_RPMSG_TX_QUEUE:
		rc = vmm_virtio_queue_setup(&rdev->vqs[vq], dev->guest,
			pfn, page_size, VIRTIO_RPMSG_QUEUE_SIZE, align);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_rpmsg_get_pfn_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc;
	struct virtio_rpmsg_dev *rdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_RPMSG_RX_QUEUE:
	case VIRTIO_RPMSG_TX_QUEUE:
		rc = vmm_virtio_queue_guest_pfn(&rdev->vqs[vq]);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_rpmsg_get_size_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc;

	switch (vq) {
	case VIRTIO_RPMSG_RX_QUEUE:
	case VIRTIO_RPMSG_TX_QUEUE:
		rc = VIRTIO_RPMSG_QUEUE_SIZE;
		break;
	default:
		rc = 0;
		break;
	};

	return rc;
}

static int virtio_rpmsg_set_size_vq(struct vmm_virtio_device *dev,
				    u32 vq, int size)
{
	/* FIXME: dynamic */
	return size;
}

static void virtio_rpmsg_free_hdr(struct vmm_vmsg *m)
{
	struct virtio_rpmsg_dev *rdev = m->priv;
	struct virtio_rpmsg_buf *buf =
			container_of(m, struct virtio_rpmsg_buf, msg);
	struct vmm_virtio_queue *vq = &rdev->vqs[VIRTIO_RPMSG_TX_QUEUE];
	struct vmm_virtio_device *dev = rdev->vdev;

	if (buf->flags & VIRTIO_RPMSG_BUF_SET_USED_TX) {
		vmm_virtio_queue_set_used_elem(vq, buf->head, buf->total_len);
	}

	if (buf->flags & VIRTIO_RPMSG_BUF_NOTIFY_TX) {
		if (vmm_virtio_queue_should_signal(vq)) {
			dev->tra->notify(dev, VIRTIO_RPMSG_TX_QUEUE);
		}
	}

	mempool_free(rdev->tx_buf_pool, buf);
}

void virtio_rpmsg_tx_msgs(struct vmm_vmsg_node *node, void *data, int budget)
{
	int rc;
	u16 head = 0;
	u32 i, len, iov_cnt = 0, total_len = 0;
	struct virtio_rpmsg_dev *rdev = data;
	struct vmm_virtio_device *dev = rdev->vdev;
	struct vmm_virtio_queue *vq = &rdev->vqs[VIRTIO_RPMSG_TX_QUEUE];
	struct vmm_virtio_iovec *iov = rdev->tx_iov;
	struct vmm_virtio_iovec tiov;
	struct vmm_rpmsg_hdr hdr;
	struct virtio_rpmsg_buf *buf;
	struct vmm_vmsg *msg;

	while ((budget > 0) && vmm_virtio_queue_available(vq)) {
		rc = vmm_virtio_queue_get_iovec(vq, iov,
						&iov_cnt, &total_len, &head);
		if (rc) {
			vmm_printf("%s: failed to get iovec (error %d)\n",
				   __func__, rc);
			continue;
		}

		DPRINTF("%s: node=%s iov_cnt=%d total_len=0x%x\n",
			__func__, rdev->node->name, iov_cnt, total_len);

		for (i = 0; i < iov_cnt; i++) {
			memcpy(&tiov, &iov[i], sizeof(tiov));

			if ((tiov.len < sizeof(hdr)) ||
			    (VIRTIO_RPMSG_MAX_BUFF_SIZE < tiov.len)) {
				continue;
			}

			len = vmm_virtio_iovec_to_buf_read(dev, &tiov,
							1, &hdr, sizeof(hdr));
			if (len != sizeof(hdr)) {
				continue;
			}

			tiov.addr += len;
			tiov.len -= len;

			if (!tiov.len ||
			    (hdr.dst < VMM_VMSG_NODE_ADDR_MIN) ||
			    (hdr.len != tiov.len)) {
				continue;
			}

			buf = mempool_malloc(rdev->tx_buf_pool);
			if (!buf) {
				vmm_printf("%s: node=%s failed to alloc buf\n",
					   __func__, rdev->node->name);
				continue;
			}
			msg = &buf->msg;

			if (i == 0) {
				buf->flags = VIRTIO_RPMSG_BUF_SET_USED_TX;
				buf->head = head;
				buf->total_len = total_len;
			} else {
				buf->flags = 0;
				buf->head = 0;
				buf->total_len = 0;
			}

			INIT_VMSG(msg, hdr.dst, rdev->node->addr,
				  hdr.src, &buf->data[0], hdr.len, rdev,
				  NULL, virtio_rpmsg_free_hdr);

			len = vmm_virtio_iovec_to_buf_read(dev, &tiov,
						1, msg->data, msg->len);
			if (len != msg->len) {
				goto skip_msg;
			}

			DPRINTF("%s: node=%s addr=0x%x src=0x%x dst=0x%x "
				"local=0x%x len=0x%zx\n", __func__,
				rdev->node->name, rdev->node->addr,
				msg->src, msg->dst, msg->local, msg->len);

			vmm_vmsg_node_send_fast(rdev->node, msg);

skip_msg:
			vmm_vmsg_dref(msg);
		}

		budget--;
	}

	if (vmm_virtio_queue_available(vq)) {
		vmm_vmsg_node_start_lazy(&rdev->tx_lazy);
	}

	if (vmm_virtio_queue_should_signal(vq)) {
		dev->tra->notify(dev, VIRTIO_RPMSG_TX_QUEUE);
	}

	return;
}

static int virtio_rpmsg_notify_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc = VMM_OK;
	struct virtio_rpmsg_dev *rdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_RPMSG_TX_QUEUE:
		vmm_vmsg_node_start_lazy(&rdev->tx_lazy);
		break;
	case VIRTIO_RPMSG_RX_QUEUE:
		break;
	default:
		rc = VMM_EINVALID;
		break;
	}

	return rc;
}

static void virtio_rpmsg_status_changed(struct vmm_virtio_device *dev,
					u32 new_status)
{
	struct virtio_rpmsg_dev *rdev = dev->emu_data;

	if (new_status & VMM_VIRTIO_CONFIG_S_DRIVER_OK) {
		vmm_vmsg_node_ready(rdev->node);
	} else {
		vmm_vmsg_node_notready(rdev->node);
	}
}

static int virtio_rpmsg_rx_msg(struct virtio_rpmsg_dev *rdev,
			       u32 src, u32 dst, u32 local,
			       void *msg, u16 len, bool use_local_as_dst)
{
	int rc;
	u16 head = 0;
	u32 pos, iov_cnt = 0, total_len = 0;
	struct vmm_virtio_queue *vq = &rdev->vqs[VIRTIO_RPMSG_RX_QUEUE];
	struct vmm_virtio_iovec *iov = rdev->rx_iov;
	struct vmm_virtio_device *dev = rdev->vdev;
	struct vmm_rpmsg_hdr hdr;

	DPRINTF("%s: node=%s src=0x%x dst=0x%x local=0x%x len=0x%x "
		"use_local_as_dst=%d\n", __func__, rdev->node->name,
		src, dst, local, len, use_local_as_dst);

	if (!vmm_virtio_queue_available(vq)) {
		return VMM_ENOSPC;
	}

	rc = vmm_virtio_queue_get_iovec(vq, iov,
					&iov_cnt, &total_len, &head);
	if (rc) {
		vmm_printf("%s: failed to get iovec (error %d)\n",
			   __func__, rc);
		return rc;
	}
	if (!iov_cnt || (iov->len < (sizeof(hdr) + len))) {
		return VMM_ENOSPC;
	}

	if (use_local_as_dst) {
		dst = local;
	}

	hdr.src = src;
	hdr.dst = dst;
	hdr.reserved = 0;
	hdr.len = len;
	hdr.flags = 0;

	pos = vmm_virtio_buf_to_iovec_write(dev, &iov[0], 1,
					    &hdr, sizeof(hdr));
	iov[0].addr += pos;
	iov[0].len -= pos;

	vmm_virtio_buf_to_iovec_write(dev, &iov[0], 1, msg, len);

	vmm_virtio_queue_set_used_elem(vq, head, 1);

	if (vmm_virtio_queue_should_signal(vq)) {
		dev->tra->notify(dev, VIRTIO_RPMSG_RX_QUEUE);
	}

	return VMM_OK;
}

static void virtio_rpmsg_peer_up(struct vmm_vmsg_node *node,
				 const char *peer_name, u32 peer_addr)
{
	int rc;
	struct vmm_rpmsg_ns_msg nsmsg;
	struct virtio_rpmsg_dev *rdev = vmm_vmsg_node_priv(node);

	if (!(rdev->features & (1UL << VMM_VIRTIO_RPMSG_F_NS))) {
		return;
	}

	memset(&nsmsg, 0, sizeof(nsmsg));
	if (rdev->node_ns_name_avail) {
		strncpy(nsmsg.name, rdev->node_ns_name, sizeof(nsmsg.name));
	} else {
		strncpy(nsmsg.name, peer_name, sizeof(nsmsg.name));
	}
	nsmsg.addr = peer_addr;
	nsmsg.flags = VMM_VIRTIO_RPMSG_NS_CREATE;

	DPRINTF("%s: node=%s peer=%s nsmsg.name=%s nsmsg.addr=0x%x\n",
		__func__, node->name, peer_name, nsmsg.name, nsmsg.addr);

	rc = virtio_rpmsg_rx_msg(rdev,
				 VMM_VIRTIO_RPMSG_NS_ADDR,
				 VMM_VIRTIO_RPMSG_NS_ADDR,
				 VMM_VIRTIO_RPMSG_NS_ADDR,
				 &nsmsg, sizeof(nsmsg), FALSE);
	if (rc) {
		vmm_printf("%s: Failed to rx message (error %d)\n",
			   __func__, rc);
		return;
	}
}

static void virtio_rpmsg_peer_down(struct vmm_vmsg_node *node,
				   const char *peer_name, u32 peer_addr)
{
	int rc;
	struct vmm_rpmsg_ns_msg nsmsg;
	struct virtio_rpmsg_dev *rdev = vmm_vmsg_node_priv(node);

	if (!(rdev->features & (1UL << VMM_VIRTIO_RPMSG_F_NS))) {
		return;
	}

	memset(&nsmsg, 0, sizeof(nsmsg));
	if (rdev->node_ns_name_avail) {
		strncpy(nsmsg.name, rdev->node_ns_name, sizeof(nsmsg.name));
	} else {
		strncpy(nsmsg.name, peer_name, sizeof(nsmsg.name));
	}
	nsmsg.addr = peer_addr;
	nsmsg.flags = VMM_VIRTIO_RPMSG_NS_DESTROY;

	DPRINTF("%s: node=%s peer=%s nsmsg.name=%s nsmsg.addr=0x%x\n",
		__func__, node->name, peer_name, nsmsg.name, nsmsg.addr);

	rc = virtio_rpmsg_rx_msg(rdev,
				 VMM_VIRTIO_RPMSG_NS_ADDR,
				 VMM_VIRTIO_RPMSG_NS_ADDR,
				 VMM_VIRTIO_RPMSG_NS_ADDR,
				 &nsmsg, sizeof(nsmsg), FALSE);
	if (rc) {
		vmm_printf("%s: Failed to rx message (error %d)\n",
			   __func__, rc);
	}
}

static bool virtio_rpmsg_can_recv_msg(struct vmm_vmsg_node *node)
{
	struct virtio_rpmsg_dev *rdev = vmm_vmsg_node_priv(node);
	struct vmm_virtio_queue *vq = &rdev->vqs[VIRTIO_RPMSG_RX_QUEUE];

	return vmm_virtio_queue_available(vq);
}

static int virtio_rpmsg_recv_msg(struct vmm_vmsg_node *node,
				 struct vmm_vmsg *msg)
{
	struct virtio_rpmsg_dev *rdev = vmm_vmsg_node_priv(node);

	return virtio_rpmsg_rx_msg(rdev, msg->src, msg->dst, msg->local,
				   msg->data, (u32)msg->len, TRUE);
}

static int virtio_rpmsg_read_config(struct vmm_virtio_device *dev,
				    u32 offset, void *dst, u32 dst_len)
{
	/* No config reads */

	return VMM_EINVALID;
}

static int virtio_rpmsg_write_config(struct vmm_virtio_device *dev,
				     u32 offset, void *src, u32 src_len)
{
	/* No config writes */

	return VMM_EINVALID;
}

static int virtio_rpmsg_reset(struct vmm_virtio_device *dev)
{
	int rc;
	struct virtio_rpmsg_dev *rdev = dev->emu_data;

	vmm_vmsg_node_stop_lazy(&rdev->tx_lazy);

	vmm_vmsg_node_notready(rdev->node);

	rc = vmm_virtio_queue_cleanup(&rdev->vqs[VIRTIO_RPMSG_RX_QUEUE]);
	if (rc) {
		return rc;
	}

	rc = vmm_virtio_queue_cleanup(&rdev->vqs[VIRTIO_RPMSG_TX_QUEUE]);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static struct vmm_vmsg_node_ops virtio_rpmsg_ops = {
	.peer_up = virtio_rpmsg_peer_up,
	.peer_down = virtio_rpmsg_peer_down,
	.can_recv_msg = virtio_rpmsg_can_recv_msg,
	.recv_msg = virtio_rpmsg_recv_msg,
};

static int virtio_rpmsg_connect(struct vmm_virtio_device *dev,
				struct vmm_virtio_emulator *emu)
{
	u32 addr, page_count;
	const char *dom_name = NULL;
	const char *ns_name = NULL;
	struct vmm_vmsg_domain *dom;
	struct virtio_rpmsg_dev *rdev;

	rdev = vmm_zalloc(sizeof(struct virtio_rpmsg_dev));
	if (!rdev) {
		vmm_printf("%s: Failed to alloc virtio rpmsg device....\n",
			   __func__);
		return VMM_ENOMEM;
	}
	rdev->vdev = dev;

	if (vmm_devtree_read_string(dev->edev->node,
				    VMM_DEVTREE_DOMAIN_ATTR_NAME,
				    &dom_name)) {
		dom_name = NULL;
	}

	if (dom_name) {
		dom = vmm_vmsg_domain_find(dom_name);
	} else {
		dom = NULL;
	}

	if (vmm_devtree_read_string(dev->edev->node,
				    VMM_DEVTREE_NODE_NS_NAME_ATTR_NAME,
				    &ns_name)) {
		rdev->node_ns_name_avail = FALSE;
	} else {
		strncpy(rdev->node_ns_name, ns_name,
			sizeof(rdev->node_ns_name));
		rdev->node_ns_name_avail = TRUE;
	}

	if (vmm_devtree_read_u32(dev->edev->node,
				 VMM_DEVTREE_NODE_ADDR_ATTR_NAME,
				 &addr)) {
		addr = VMM_VMSG_NODE_ADDR_ANY;
	}

	page_count = VMM_SIZE_TO_PAGE(sizeof(struct virtio_rpmsg_buf) *
				      VIRTIO_RPMSG_QUEUE_SIZE);
	rdev->tx_buf_pool = mempool_ram_create(sizeof(struct virtio_rpmsg_buf),
					page_count, VMM_PAGEPOOL_NORMAL);
	if (!rdev->tx_buf_pool) {
		vmm_free(rdev);
		return VMM_ENOMEM;
	}

	rdev->node = vmm_vmsg_node_create(dev->name, addr,
					  VIRTIO_RPMSG_NODE_MAX_BUFF_SIZE,
					  &virtio_rpmsg_ops, dom, rdev);
	if (!rdev->node) {
		mempool_destroy(rdev->tx_buf_pool);
		vmm_free(rdev);
		return VMM_EFAIL;
	}

	INIT_VMM_VMSG_NODE_LAZY(&rdev->tx_lazy, rdev->node,
				VIRTIO_RPMSG_QUEUE_SIZE / 16,
				rdev, virtio_rpmsg_tx_msgs);

	dev->emu_data = rdev;

	return VMM_OK;
}

static void virtio_rpmsg_disconnect(struct vmm_virtio_device *dev)
{
	struct virtio_rpmsg_dev *rdev = dev->emu_data;

	vmm_vmsg_node_destroy(rdev->node);
	mempool_destroy(rdev->tx_buf_pool);
	vmm_free(rdev);
}

struct vmm_virtio_device_id virtio_rpmsg_emu_id[] = {
	{ .type = VMM_VIRTIO_ID_RPMSG },
	{ },
};

struct vmm_virtio_emulator virtio_rpmsg = {
	.name = "virtio_rpmsg",
	.id_table = virtio_rpmsg_emu_id,

	/* VirtIO operations */
	.get_host_features      = virtio_rpmsg_get_host_features,
	.set_guest_features     = virtio_rpmsg_set_guest_features,
	.init_vq                = virtio_rpmsg_init_vq,
	.get_pfn_vq             = virtio_rpmsg_get_pfn_vq,
	.get_size_vq            = virtio_rpmsg_get_size_vq,
	.set_size_vq            = virtio_rpmsg_set_size_vq,
	.notify_vq              = virtio_rpmsg_notify_vq,
	.status_changed         = virtio_rpmsg_status_changed,

	/* Emulator operations */
	.read_config = virtio_rpmsg_read_config,
	.write_config = virtio_rpmsg_write_config,
	.reset = virtio_rpmsg_reset,
	.connect = virtio_rpmsg_connect,
	.disconnect = virtio_rpmsg_disconnect,
};

static int __init virtio_rpmsg_init(void)
{
	return vmm_virtio_register_emulator(&virtio_rpmsg);
}

static void __exit virtio_rpmsg_exit(void)
{
	vmm_virtio_unregister_emulator(&virtio_rpmsg);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
