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
 * @file virtio_console.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO based console Emulator.
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vio/vmm_vserial.h>
#include <vio/vmm_virtio.h>
#include <vio/vmm_virtio_console.h>
#include <libs/fifo.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"VirtIO Console Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VSERIAL_IPRIORITY + \
					 VMM_VIRTIO_IPRIORITY + 1)
#define MODULE_INIT			virtio_console_init
#define MODULE_EXIT			virtio_console_exit

#define VIRTIO_CONSOLE_QUEUE_SIZE	128
#define VIRTIO_CONSOLE_NUM_QUEUES	2
#define VIRTIO_CONSOLE_RX_QUEUE		0
#define VIRTIO_CONSOLE_TX_QUEUE		1

#define VIRTIO_CONSOLE_VSERIAL_FIFO_SZ	1024

struct virtio_console_dev {
	struct vmm_virtio_device *vdev;

	struct vmm_virtio_queue vqs[VIRTIO_CONSOLE_NUM_QUEUES];
	struct vmm_virtio_iovec rx_iov[VIRTIO_CONSOLE_QUEUE_SIZE];
	struct vmm_virtio_iovec tx_iov[VIRTIO_CONSOLE_QUEUE_SIZE];
	struct vmm_virtio_console_config config;
	u32 features;

	char name[VMM_VIRTIO_DEVICE_MAX_NAME_LEN];
	struct vmm_vserial *vser;
	struct fifo *emerg_rd;
};

static u32 virtio_console_get_host_features(struct vmm_virtio_device *dev)
{
	/* We support emergency write. */
	return 1UL << VMM_VIRTIO_CONSOLE_F_EMERG_WRITE;
}

static void virtio_console_set_guest_features(struct vmm_virtio_device *dev,
					      u32 features)
{
	/* No host features so, ignore it. */
}

static int virtio_console_init_vq(struct vmm_virtio_device *dev,
				  u32 vq, u32 page_size, u32 align, u32 pfn)
{
	int rc;
	struct virtio_console_dev *cdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_CONSOLE_RX_QUEUE:
	case VIRTIO_CONSOLE_TX_QUEUE:
		rc = vmm_virtio_queue_setup(&cdev->vqs[vq], dev->guest, 
			pfn, page_size, VIRTIO_CONSOLE_QUEUE_SIZE, align);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_console_get_pfn_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc;
	struct virtio_console_dev *cdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_CONSOLE_RX_QUEUE:
	case VIRTIO_CONSOLE_TX_QUEUE:
		rc = vmm_virtio_queue_guest_pfn(&cdev->vqs[vq]);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_console_get_size_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc;

	switch (vq) {
	case VIRTIO_CONSOLE_RX_QUEUE:
	case VIRTIO_CONSOLE_TX_QUEUE:
		rc = VIRTIO_CONSOLE_QUEUE_SIZE;
		break;
	default:
		rc = 0;
		break;
	};

	return rc;
}

static int virtio_console_set_size_vq(struct vmm_virtio_device *dev,
				      u32 vq, int size)
{
	/* FIXME: dynamic */
	return size;
}

static int virtio_console_do_tx(struct vmm_virtio_device *dev,
				struct virtio_console_dev *cdev)
{
	u8 buf[8];
	u16 head = 0;
	u32 i, len, iov_cnt = 0, total_len = 0;
	struct vmm_virtio_queue *vq = &cdev->vqs[VIRTIO_CONSOLE_TX_QUEUE];
	struct vmm_virtio_iovec *iov = cdev->tx_iov;
	struct vmm_virtio_iovec tiov;

	while (vmm_virtio_queue_available(vq)) {
		head = vmm_virtio_queue_get_iovec(vq, iov,
						  &iov_cnt, &total_len);

		for (i = 0; i < iov_cnt; i++) {
			memcpy(&tiov, &iov[i], sizeof(tiov));
			while (tiov.len) {
				len = vmm_virtio_iovec_to_buf_read(dev, &tiov,
							1, &buf, sizeof(buf));
				vmm_vserial_receive(cdev->vser, buf, len);
				tiov.addr += len;
				tiov.len -= len;
			}
		}

		vmm_virtio_queue_set_used_elem(vq, head, total_len);
	}

	return VMM_OK;
}

static int virtio_console_notify_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc = VMM_OK;
	struct virtio_console_dev *cdev = dev->emu_data;

	switch (vq) {
	case VIRTIO_CONSOLE_TX_QUEUE:
		rc = virtio_console_do_tx(dev, cdev);
		break;
	case VIRTIO_CONSOLE_RX_QUEUE:
		break;
	default:
		rc = VMM_EINVALID;
		break;
	}

	return rc;
}

static bool virtio_console_vserial_can_send(struct vmm_vserial *vser)
{
	/* We always return TRUE because we always queue
	 * send data to emergency read fifo.
	 *
	 * If VirtIO Rx Queue is available then we also queue
	 * the send data to VirtIO Rx Queue.
	 */
	return TRUE;
}

static int virtio_console_vserial_send(struct vmm_vserial *vser, u8 data)
{
	u16 head = 0;
	u32 iov_cnt = 0, total_len = 0;
	struct virtio_console_dev *cdev = vmm_vserial_priv(vser);
	struct vmm_virtio_queue *vq = &cdev->vqs[VIRTIO_CONSOLE_RX_QUEUE];
	struct vmm_virtio_iovec *iov = cdev->rx_iov;
	struct vmm_virtio_device *dev = cdev->vdev;

	fifo_enqueue(cdev->emerg_rd, &data, TRUE);

	if (vmm_virtio_queue_available(vq)) {
		head = vmm_virtio_queue_get_iovec(vq, iov,
						  &iov_cnt, &total_len);
		if (iov_cnt) {
			vmm_virtio_buf_to_iovec_write(dev, &iov[0], 1,
						      &data, 1);

			vmm_virtio_queue_set_used_elem(vq, head, 1);

			if (vmm_virtio_queue_should_signal(vq)) {
				dev->tra->notify(dev, VIRTIO_CONSOLE_RX_QUEUE);
			}
		}
	}

	return VMM_OK;
}

static int virtio_console_read_config(struct vmm_virtio_device *dev, 
				      u32 offset, void *dst, u32 dst_len)
{
	struct virtio_console_dev *cdev = dev->emu_data;
	u8 data8, *src = (u8 *)&cdev->config;
	u32 i, data, src_len = sizeof(cdev->config);

	if (offset == offsetof(struct vmm_virtio_console_config, emerg_wr)) {
		if (fifo_dequeue(cdev->emerg_rd, &data8)) {
			data = (1 << 31) | data8;
		} else {
			data = 0x0;
		}
		switch (dst_len) {
		case 1:
			*(u8 *)dst = (u8)data;
			break;
		case 2:
			*(u16 *)dst = (u16)data;
			break;
		case 4:
			*(u32 *)dst = (u32)data;
			break;
		default:
			data = 0x0;
			break;
		};
	} else {
		for (i = 0; (i < dst_len) && ((offset + i) < src_len); i++) {
			*((u8 *)dst + i) = src[offset + i];
		}
	}

	return VMM_OK;
}

static int virtio_console_write_config(struct vmm_virtio_device *dev,
				       u32 offset, void *src, u32 src_len)
{
	u8 data;
	struct virtio_console_dev *cdev = dev->emu_data;

	if (offset == offsetof(struct vmm_virtio_console_config, emerg_wr)) {
		switch (src_len) {
		case 1:
			data = *(u8 *)src;
			break;
		case 2:
			data = *(u16 *)src;
			break;
		case 4:
			data = *(u32 *)src;
			break;
		default:
			data = 0x0;
			break;
		};
		vmm_vserial_receive(cdev->vser, &data, 1);
	}

	/* Ignore config writes to other parts of console config space */

	return VMM_OK;
}

static int virtio_console_reset(struct vmm_virtio_device *dev)
{
	int rc;
	struct virtio_console_dev *cdev = dev->emu_data;

	if (!fifo_clear(cdev->emerg_rd)) {
		return VMM_EFAIL;
	}

	rc = vmm_virtio_queue_cleanup(&cdev->vqs[VIRTIO_CONSOLE_RX_QUEUE]);
	if (rc) {
		return rc;
	}

	rc = vmm_virtio_queue_cleanup(&cdev->vqs[VIRTIO_CONSOLE_TX_QUEUE]);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int virtio_console_connect(struct vmm_virtio_device *dev, 
				  struct vmm_virtio_emulator *emu)
{
	struct virtio_console_dev *cdev;

	cdev = vmm_zalloc(sizeof(struct virtio_console_dev));
	if (!cdev) {
		vmm_printf("Failed to allocate virtio console device....\n");
		return VMM_ENOMEM;
	}
	cdev->vdev = dev;

	vmm_snprintf(cdev->name, VMM_VIRTIO_DEVICE_MAX_NAME_LEN,
		     "%s", dev->name);
	cdev->vser = vmm_vserial_create(cdev->name, 
					&virtio_console_vserial_can_send,
					&virtio_console_vserial_send,
					VIRTIO_CONSOLE_VSERIAL_FIFO_SZ, cdev);
	if (!cdev->vser) {
		return VMM_EFAIL;
	}

	cdev->emerg_rd = fifo_alloc(1, VIRTIO_CONSOLE_VSERIAL_FIFO_SZ);
	if (!cdev->emerg_rd) {
		vmm_vserial_destroy(cdev->vser);
		return VMM_ENOMEM;
	}
	
	cdev->config.cols = 80;
	cdev->config.rows = 24;
	cdev->config.max_nr_ports = 1;

	dev->emu_data = cdev;

	return VMM_OK;
}

static void virtio_console_disconnect(struct vmm_virtio_device *dev)
{
	struct virtio_console_dev *cdev = dev->emu_data;

	fifo_free(cdev->emerg_rd);
	vmm_vserial_destroy(cdev->vser);
	vmm_free(cdev);
}

struct vmm_virtio_device_id virtio_console_emu_id[] = {
	{ .type = VMM_VIRTIO_ID_CONSOLE },
	{ },
};

struct vmm_virtio_emulator virtio_console = {
	.name = "virtio_console",
	.id_table = virtio_console_emu_id,

	/* VirtIO operations */
	.get_host_features      = virtio_console_get_host_features,
	.set_guest_features     = virtio_console_set_guest_features,
	.init_vq                = virtio_console_init_vq,
	.get_pfn_vq             = virtio_console_get_pfn_vq,
	.get_size_vq            = virtio_console_get_size_vq,
	.set_size_vq            = virtio_console_set_size_vq,
	.notify_vq              = virtio_console_notify_vq,

	/* Emulator operations */
	.read_config = virtio_console_read_config,
	.write_config = virtio_console_write_config,
	.reset = virtio_console_reset,
	.connect = virtio_console_connect,
	.disconnect = virtio_console_disconnect,
};

static int __init virtio_console_init(void)
{
	return vmm_virtio_register_emulator(&virtio_console);
}

static void __exit virtio_console_exit(void)
{
	vmm_virtio_unregister_emulator(&virtio_console);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
