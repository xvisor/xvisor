/**
 * Copyright (c) 2013 Pranav Sawargaonkar.
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
 * @file virtio_net.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO based Network Device Emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>

#include <net/vmm_protocol.h>
#include <net/vmm_mbuf.h>
#include <net/vmm_net.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_netport.h>
#include <net/vmm_mbuf.h>

#include <emu/virtio.h>
#include <emu/virtio_net.h>

#define MODULE_DESC			"VirtIO Net Emulator"
#define MODULE_AUTHOR			"Pranav Sawargaonkar"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VIRTIO_IPRIORITY + 1)
#define MODULE_INIT			virtio_net_init
#define MODULE_EXIT			virtio_net_exit

#define VIRTIO_NET_QUEUE_SIZE		256
#define VIRTIO_NET_NUM_QUEUES		2
#define VIRTIO_NET_RX_QUEUE		0
#define VIRTIO_NET_TX_QUEUE		1

#define VIRTIO_NET_MTU			1514

#define VIRTIO_NET_TX_LAZY_BUDGET	(VIRTIO_NET_QUEUE_SIZE / 4)

struct virtio_net_dev {
	struct virtio_device *vdev;

	struct virtio_queue vqs[VIRTIO_NET_NUM_QUEUES];
	struct virtio_iovec rx_iov[VIRTIO_NET_QUEUE_SIZE];
	struct virtio_iovec tx_iov[VIRTIO_NET_QUEUE_SIZE];
	struct virtio_net_config config;
	u32 features;

	int mode;
	struct vmm_netport *port;
	char name[VIRTIO_DEVICE_MAX_NAME_LEN];
};

static u32 virtio_net_get_host_features(struct virtio_device *dev)
{
	return 1UL << VIRTIO_NET_F_MAC
#if 0
		| 1UL << VIRTIO_NET_F_CSUM
		| 1UL << VIRTIO_NET_F_HOST_UFO
		| 1UL << VIRTIO_NET_F_HOST_TSO4
		| 1UL << VIRTIO_NET_F_HOST_TSO6
		| 1UL << VIRTIO_NET_F_GUEST_UFO
		| 1UL << VIRTIO_NET_F_GUEST_TSO4
		| 1UL << VIRTIO_NET_F_GUEST_TSO6
#endif
		| 1UL << VIRTIO_RING_F_EVENT_IDX
#if 0
		| 1UL << VIRTIO_RING_F_INDIRECT_DESC
#endif
		;
}

static void virtio_net_set_guest_features(struct virtio_device *dev,
					  u32 features)
{
	struct virtio_net_dev *ndev = dev->emu_data;

	ndev->features = features;
}

static int virtio_net_init_vq(struct virtio_device *dev,
			      u32 vqnum, u32 page_size, u32 align,
			      u32 pfn)
{
	struct virtio_net_dev *ndev = dev->emu_data;

	return virtio_queue_setup(&ndev->vqs[vqnum], dev->guest, 
				pfn, page_size, VIRTIO_NET_QUEUE_SIZE, align);
}

static int virtio_net_get_pfn_vq(struct virtio_device *dev, u32 vq)
{
	struct virtio_net_dev *ndev = dev->emu_data;

	return virtio_queue_guest_pfn(&ndev->vqs[vq]);
}

static int virtio_net_get_size_vq(struct virtio_device *dev, u32 vq)
{
	/* FIXME: dynamic */
	return VIRTIO_NET_QUEUE_SIZE;
}

static int virtio_net_set_size_vq(struct virtio_device *dev, u32 vq, int size)
{
	/* FIXME: dynamic */
	return size;
}

static void virtio_net_tx_lazy(struct vmm_netport *port, void *arg, int budget)
{
	u16 head = 0;
	u32 iov_cnt = 0, pkt_len = 0, total_len = 0;
	struct virtio_net_dev *ndev = arg;
	struct virtio_device *dev = ndev->vdev;
	struct virtio_queue *vq = &ndev->vqs[VIRTIO_NET_TX_QUEUE];
	struct virtio_iovec *iov = ndev->tx_iov;
	struct vmm_mbuf *mb;

	while ((budget > 0) && virtio_queue_available(vq)) {
		head = virtio_queue_get_iovec(vq, iov, 
						VIRTIO_NET_QUEUE_SIZE,
						&iov_cnt, &total_len);

		/* iov[0] is offload info */
		pkt_len = total_len - iov[0].len;

		if (pkt_len <= VIRTIO_NET_MTU) {
			MGETHDR(mb, 0, 0);
			MEXTMALLOC(mb, pkt_len, M_WAIT);
			virtio_iovec_to_buf_read(dev, 
						 &iov[1], iov_cnt - 1,
						 M_BUFADDR(mb), pkt_len);
			mb->m_len = mb->m_pktlen = pkt_len;
			vmm_port2switch_xfer_mbuf(ndev->port, mb);
		}

		virtio_queue_set_used_elem(vq, head, total_len);

		budget--;
	}

	if (virtio_queue_should_signal(vq)) {
		dev->tra->notify(dev, VIRTIO_NET_TX_QUEUE);
	}
}

static int virtio_net_notify_vq(struct virtio_device *dev, u32 vq)
{
	struct virtio_net_dev *ndev = dev->emu_data;

	switch (vq) {
	case VIRTIO_NET_TX_QUEUE:
		vmm_port2switch_xfer_lazy(ndev->port, virtio_net_tx_lazy, 
					  ndev, VIRTIO_NET_TX_LAZY_BUDGET);
		break;
	case VIRTIO_NET_RX_QUEUE:
		break;
	default:
		vmm_printf("Unknown queue index %u", vq);
		break;
	}

	return VMM_OK;
}

static void virtio_net_set_link(struct vmm_netport *p)
{
	/* FIXME: */
}

static int virtio_net_can_receive(struct vmm_netport *p)
{
	return 1;
}

static int virtio_net_switch2port_xfer(struct vmm_netport *p,
				       struct vmm_mbuf *mb)
{
	u16 head = 0;
	u32 iov_cnt = 0, total_len = 0, pkt_len = 0;
	struct virtio_net_dev *ndev = p->priv;
	struct virtio_queue *vq = &ndev->vqs[VIRTIO_NET_RX_QUEUE];
	struct virtio_iovec *iov = ndev->rx_iov;
	struct virtio_device *dev = ndev->vdev;

	pkt_len = min(VIRTIO_NET_MTU, mb->m_pktlen);

	if (virtio_queue_available(vq)) {
		head = virtio_queue_get_iovec(vq, iov, 
						VIRTIO_NET_QUEUE_SIZE,
						&iov_cnt, &total_len);
	}

	if (iov_cnt > 1) {
		virtio_iovec_fill_zeros(dev, &iov[0], 1);
		virtio_buf_to_iovec_write(dev, &iov[1], 1,
						M_BUFADDR(mb), pkt_len);
		virtio_queue_set_used_elem(vq, head, iov[0].len + pkt_len);

		if (virtio_queue_should_signal(vq)) {
			dev->tra->notify(dev, VIRTIO_NET_RX_QUEUE);
		}
	}

	m_freem(mb);

	return VMM_OK;
}

static int virtio_net_read_config(struct virtio_device *dev, 
				  u32 offset, void *dst, u32 dst_len)
{
	int i = 0;
	struct virtio_net_dev *ndev = dev->emu_data;
	u8 *src = (u8 *)&ndev->config;

	for (i = 0; i < dst_len; i++) {
		*((u8 *)dst + i) = src[offset + i];
	}

	return VMM_OK;
}

static int virtio_net_write_config(struct virtio_device *dev,
				   u32 offset, void *src, u32 src_len)
{
	int i = 0;
	struct virtio_net_dev *ndev = dev->emu_data;
	u8 *dst = (u8 *)&ndev->config;

	for (i = 0; i < src_len; i++) {
		dst[i] = *((u8 *) src + i);
	}

	return VMM_OK;
}

static int virtio_net_reset(struct virtio_device *dev)
{
	int rc;
	struct virtio_net_dev *ndev = dev->emu_data;

	rc = virtio_queue_cleanup(&ndev->vqs[VIRTIO_NET_RX_QUEUE]);
	if (rc) {
		return rc;
	}

	rc = virtio_queue_cleanup(&ndev->vqs[VIRTIO_NET_TX_QUEUE]);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int virtio_net_connect(struct virtio_device *dev, 
			      struct virtio_emulator *emu)
{
	int i, rc;
	char *attr;
	struct virtio_net_dev *ndev;
	struct vmm_netswitch *nsw;

	ndev = vmm_zalloc(sizeof(struct virtio_net_dev));
	if (!ndev) {
		vmm_printf("Failed to allocate virtio net device....\n");
		return VMM_EFAIL;
	}

	ndev->vdev = dev;
	vmm_snprintf(ndev->name, VIRTIO_DEVICE_MAX_NAME_LEN, "%s", dev->name);
	ndev->port = vmm_netport_alloc(ndev->name, VMM_NETPORT_DEF_QUEUE_SIZE);
	ndev->port->mtu = VIRTIO_NET_MTU;
	ndev->port->link_changed = virtio_net_set_link;
	ndev->port->can_receive = virtio_net_can_receive;
	ndev->port->switch2port_xfer = virtio_net_switch2port_xfer;
	ndev->port->priv = ndev;

	rc = vmm_netport_register(ndev->port);
	if (rc) {
		vmm_netport_free(ndev->port);
		vmm_free(ndev);
		return rc;
	}

	attr = vmm_devtree_attrval(dev->edev->node, "switch");
	if (attr) {
		nsw = vmm_netswitch_find((char *)attr);
		if (!nsw) {
			vmm_printf("%s: Cannot find netswitch \"%s\"\n",
					__func__, (char *)attr);
		} else {
			vmm_netswitch_port_add(nsw, ndev->port);
		}
	}

	for (i = 0; i < 6; i++) {
		ndev->config.mac[i] = vmm_netport_mac(ndev->port)[i];
	}

	ndev->config.status = VIRTIO_NET_S_LINK_UP;
	dev->emu_data = ndev;

	return VMM_OK;
}

static void virtio_net_disconnect(struct virtio_device *dev)
{
	struct virtio_net_dev *ndev = dev->emu_data;

	vmm_netport_unregister(ndev->port);
	vmm_netport_free(ndev->port);
	vmm_free(ndev);
}

struct virtio_device_id virtio_net_emu_id[] = {
	{.type = VIRTIO_ID_NET},
	{ },
};

struct virtio_emulator virtio_net = {
	.name = "virtio_net",
	.id_table = virtio_net_emu_id,

	/* VirtIO operations */
	.get_host_features      = virtio_net_get_host_features,
	.set_guest_features     = virtio_net_set_guest_features,
	.init_vq                = virtio_net_init_vq,
	.get_pfn_vq             = virtio_net_get_pfn_vq,
	.get_size_vq            = virtio_net_get_size_vq,
	.set_size_vq            = virtio_net_set_size_vq,
	.notify_vq              = virtio_net_notify_vq,

	/* Emulator operations */
	.read_config = virtio_net_read_config,
	.write_config = virtio_net_write_config,
	.reset = virtio_net_reset,
	.connect = virtio_net_connect,
	.disconnect = virtio_net_disconnect,
};

static int __init virtio_net_init(void)
{
	return virtio_register_emulator(&virtio_net);
}

static void __exit virtio_net_exit(void)
{
	virtio_unregister_emulator(&virtio_net);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
