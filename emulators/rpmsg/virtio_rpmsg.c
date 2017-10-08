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
#include <vmm_devemu.h>
#include <vio/vmm_vmsg.h>
#include <vio/vmm_virtio.h>
#include <vio/vmm_virtio_rpmsg.h>
#include <libs/fifo.h>
#include <libs/radix-tree.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"VirtIO Rpmsg Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VMSG_IPRIORITY + \
					 VMM_VIRTIO_IPRIORITY + 1)
#define MODULE_INIT			virtio_rpmsg_init
#define MODULE_EXIT			virtio_rpmsg_exit

#define VIRTIO_RPMSG_QUEUE_SIZE		256
#define VIRTIO_RPMSG_NUM_QUEUES		2
#define VIRTIO_RPMSG_RX_QUEUE		0
#define VIRTIO_RPMSG_TX_QUEUE		1

struct virtio_rpmsg_dev {
	struct vmm_virtio_device *vdev;

	struct vmm_virtio_queue vqs[VIRTIO_RPMSG_NUM_QUEUES];
	struct vmm_virtio_iovec rx_iov[VIRTIO_RPMSG_QUEUE_SIZE];
	struct vmm_virtio_iovec tx_iov[VIRTIO_RPMSG_QUEUE_SIZE];
	u32 features;

	char name[VMM_VIRTIO_DEVICE_MAX_NAME_LEN];
	struct vmm_vmsg_node *node;
	struct radix_tree_root global_to_local;
};

static u32 virtio_rpmsg_get_host_features(struct vmm_virtio_device *dev)
{
	return 1UL << VMM_VIRTIO_RPMSG_F_NS;
}

static void virtio_rpmsg_set_guest_features(struct vmm_virtio_device *dev,
					    u32 features)
{
	struct virtio_rpmsg_dev *rdev = dev->emu_data;

	rdev->features = features;
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

static int virtio_rpmsg_tx_msgs(struct vmm_virtio_device *dev,
				struct virtio_rpmsg_dev *rdev)
{
	u16 head = 0;
	void *local_addr;
	u32 i, len, iov_cnt = 0, total_len = 0;
	struct vmm_virtio_queue *vq = &rdev->vqs[VIRTIO_RPMSG_TX_QUEUE];
	struct vmm_virtio_iovec *iov = rdev->tx_iov;
	struct vmm_virtio_iovec tiov;
	struct vmm_rpmsg_hdr hdr;
	struct vmm_vmsg *msg;

	while (vmm_virtio_queue_available(vq)) {
		head = vmm_virtio_queue_get_iovec(vq, iov,
						  &iov_cnt, &total_len);

		for (i = 0; i < iov_cnt; i++) {
			memcpy(&tiov, &iov[i], sizeof(tiov));

			if (tiov.len < sizeof(hdr)) {
				continue;
			}

			len = vmm_virtio_iovec_to_buf_read(dev, &tiov,
						1, &hdr, sizeof(hdr) - 1);
			if (len != (sizeof(hdr) - 1)) {
				continue;
			}

			tiov.addr += len;
			tiov.len -= len;

			if (!tiov.len ||
			    (hdr.dst < VMM_VMSG_NODE_ADDR_MIN) ||
			    (hdr.src < VMM_VMSG_NODE_ADDR_MIN) ||
			    (hdr.len != tiov.len)) {
				continue;
			}

			local_addr = radix_tree_lookup(&rdev->global_to_local,
							hdr.dst);
			if (!local_addr) {
				local_addr = (void *)((unsigned long)hdr.src);
				radix_tree_insert(&rdev->global_to_local,
						  hdr.dst, local_addr);
			}

			msg = vmm_vmsg_alloc(hdr.src, hdr.dst, hdr.len);
			if (!msg) {
				continue;
			}

			len = vmm_virtio_iovec_to_buf_read(dev, &tiov,
						1, msg->data, msg->len);
			if (len != msg->len) {
				goto skip_msg;
			}

			vmm_vmsg_node_send(rdev->node, msg);

skip_msg:
			vmm_vmsg_dref(msg);
		}

		vmm_virtio_queue_set_used_elem(vq, head, total_len);
	}

	if (vmm_virtio_queue_should_signal(vq)) {
		dev->tra->notify(dev, VIRTIO_RPMSG_TX_QUEUE);
	}

	return VMM_OK;
}

static void virtio_rpmsg_tx_work(void *data)
{
	int rc;
	struct virtio_rpmsg_dev *rdev = data;

	rc = virtio_rpmsg_tx_msgs(rdev->vdev, rdev);
	WARN_ON(rc);
}

static int virtio_rpmsg_notify_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc = VMM_OK;
	struct virtio_rpmsg_dev *rdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_RPMSG_TX_QUEUE:
		/* TODO: */
		rc = vmm_vmsg_node_start_work(rdev->node, rdev,
					      virtio_rpmsg_tx_work);
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
			       u32 src, u32 dst, void *msg, u16 len,
			       bool override_dst)
{
	u16 head = 0;
	void *local_addr;
	u32 pos, iov_cnt = 0, total_len = 0;
	struct vmm_virtio_queue *vq = &rdev->vqs[VIRTIO_RPMSG_RX_QUEUE];
	struct vmm_virtio_iovec *iov = rdev->rx_iov;
	struct vmm_virtio_device *dev = rdev->vdev;
	struct vmm_rpmsg_hdr hdr;

	if (!vmm_virtio_queue_available(vq)) {
		return VMM_ENODEV;
	}

	head = vmm_virtio_queue_get_iovec(vq, iov,
					  &iov_cnt, &total_len);
	if (!iov_cnt || (iov->len < (sizeof(hdr) + len))) {
		return VMM_ENOSPC;
	}

	if (override_dst) {
		/* Use global 'src' to determine local 'dst' */
		local_addr = radix_tree_lookup(&rdev->global_to_local, src);
		if (!local_addr) {
			return VMM_ENOTAVAIL;
		}
		dst = (u32)((unsigned long)local_addr);
	}

	hdr.src = src;
	hdr.dst = dst;
	hdr.reserved = 0;
	hdr.len = len;
	hdr.flags = 0;

	pos = vmm_virtio_buf_to_iovec_write(dev, &iov[0], 1,
					    &hdr, sizeof(hdr) - 1);
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

	strncpy(nsmsg.name, peer_name, sizeof(nsmsg.name));
	nsmsg.addr = peer_addr;
	nsmsg.flags = VMM_VIRTIO_RPMSG_NS_CREATE;

	rc = virtio_rpmsg_rx_msg(rdev,
				 VMM_VIRTIO_RPMSG_NS_ADDR,
				 VMM_VIRTIO_RPMSG_NS_ADDR,
				 &nsmsg, sizeof(nsmsg), FALSE);
	if (rc) {
		vmm_printf("%s: Failed to rx message (error %d)\n",
			   __func__, rc);
		return;
	}

	radix_tree_delete(&rdev->global_to_local, peer_addr);
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

	strncpy(nsmsg.name, peer_name, sizeof(nsmsg.name));
	nsmsg.addr = peer_addr;
	nsmsg.flags = VMM_VIRTIO_RPMSG_NS_DESTROY;

	rc = virtio_rpmsg_rx_msg(rdev,
				 VMM_VIRTIO_RPMSG_NS_ADDR,
				 VMM_VIRTIO_RPMSG_NS_ADDR,
				 &nsmsg, sizeof(nsmsg), FALSE);
	if (rc) {
		vmm_printf("%s: Failed to rx message (error %d)\n",
			   __func__, rc);
	}

	radix_tree_delete(&rdev->global_to_local, peer_addr);
}

static void virtio_rpmsg_recv_msg(struct vmm_vmsg_node *node,
				  struct vmm_vmsg *msg)
{
	int rc;
	struct virtio_rpmsg_dev *rdev = vmm_vmsg_node_priv(node);

	rc = virtio_rpmsg_rx_msg(rdev, msg->src, msg->dst,
				 msg->data, (u32)msg->len, TRUE);
	if (rc) {
		vmm_printf("%s: Failed to rx message (error %d)\n",
			   __func__, rc);
	}
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

static int virtio_rpmsg_reset_iter(struct vmm_vmsg_node *node, void *data)
{
	struct virtio_rpmsg_dev *rdev = data;

	radix_tree_delete(&rdev->global_to_local, vmm_vmsg_node_get_addr(node));

	return VMM_OK;
}

static int virtio_rpmsg_reset(struct vmm_virtio_device *dev)
{
	int rc;
	struct virtio_rpmsg_dev *rdev = dev->emu_data;

	vmm_vmsg_node_stop_work(rdev->node, rdev, virtio_rpmsg_tx_work);

	vmm_vmsg_node_notready(rdev->node);

	vmm_vmsg_domain_node_iterate(vmm_vmsg_node_get_domain(rdev->node),
				     NULL, rdev, virtio_rpmsg_reset_iter);

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
	.recv_msg = virtio_rpmsg_recv_msg,
};

static int virtio_rpmsg_connect(struct vmm_virtio_device *dev,
				struct vmm_virtio_emulator *emu)
{
	u32 addr;
	const char *dom_name = NULL;
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

	if (vmm_devtree_read_u32(dev->edev->node,
				 VMM_DEVTREE_NODE_ADDR_ATTR_NAME,
				 &addr)) {
		addr = VMM_VMSG_NODE_ADDR_ANY;
	}

	rdev->node = vmm_vmsg_node_create(dev->name, addr,
					  &virtio_rpmsg_ops,
					  dom, rdev);
	if (!rdev->node) {
		vmm_free(rdev);
		return VMM_EFAIL;
	}

	INIT_RADIX_TREE(&rdev->global_to_local, 0);

	dev->emu_data = rdev;

	return VMM_OK;
}

static void virtio_rpmsg_disconnect(struct vmm_virtio_device *dev)
{
	struct virtio_rpmsg_dev *rdev = dev->emu_data;

	vmm_vmsg_node_destroy(rdev->node);
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
