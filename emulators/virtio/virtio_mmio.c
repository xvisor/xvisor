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
 * @file virtio_mmio.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO MMIO Transport Device
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_host_io.h>
#include <emu/virtio.h>
#include <emu/virtio_queue.h>
#include <emu/virtio_mmio.h>

#define MODULE_DESC			"VirtIO MMIO Transport"
#define MODULE_AUTHOR			"Pranav Sawargaonkar"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VIRTIO_IPRIORITY + 1)
#define	MODULE_INIT			virtio_mmio_init
#define	MODULE_EXIT			virtio_mmio_exit

static int virtio_mmio_notify(struct virtio_device *dev, u32 vq)
{
	struct virtio_mmio_dev *s = dev->tra_data;

	s->config.interrupt_state |= VIRTIO_MMIO_INT_VRING;

	vmm_devemu_emulate_irq(s->guest, s->irq, 1);

	return VMM_OK;
}

int virtio_mmio_config_read(struct virtio_mmio_dev *dev,
			    u32 offset, void *dst,
			    u32 dst_len)
{
	int rc = VMM_OK;
	int val = 0;

	switch (offset) {
	case VIRTIO_MMIO_MAGIC_VALUE:
	case VIRTIO_MMIO_VERSION:
	case VIRTIO_MMIO_DEVICE_ID:
	case VIRTIO_MMIO_VENDOR_ID:
	case VIRTIO_MMIO_STATUS:
	case VIRTIO_MMIO_INTERRUPT_STATUS:
		val = (*(u32 *)(((void *)&dev->config) + offset));
		*(u32 *) dst = val;
		break;
	case VIRTIO_MMIO_HOST_FEATURES:
		val = dev->dev.emu->get_host_features(&dev->dev);
		*(u32 *) dst = val;
		break;
	case VIRTIO_MMIO_QUEUE_PFN:
		val = dev->dev.emu->get_pfn_vq(&dev->dev,
						dev->config.queue_sel);
		*(u32 *) dst = val;
		break;
	case VIRTIO_MMIO_QUEUE_NUM_MAX:
		val = dev->dev.emu->get_size_vq(&dev->dev,
						dev->config.queue_sel);
		*(u32 *) dst = val;
		break;
	default:
		break;
	}

	return rc;
}

static int virtio_mmio_read(struct vmm_emudev *edev,
			    physical_addr_t offset,
			    void *dst, u32 dst_len)
{
	struct virtio_mmio_dev *s = edev->priv;

	/* Device specific config write */
	if (offset >= VIRTIO_MMIO_CONFIG) {
		offset -= VIRTIO_MMIO_CONFIG;
		return virtio_config_read(&s->dev, (u32) offset, dst, dst_len);
	}

	return virtio_mmio_config_read(s, (u32)offset, dst, dst_len);
}

static int virtio_mmio_config_write(struct virtio_mmio_dev *dev,
                             physical_addr_t offset,
                             void *src, u32 src_len)
{
	int rc = VMM_OK;
	u32 val = vmm_cpu_to_le32(*(u32 *) (src));

	switch (offset) {
	case VIRTIO_MMIO_HOST_FEATURES_SEL:
	case VIRTIO_MMIO_GUEST_FEATURES_SEL:
	case VIRTIO_MMIO_QUEUE_SEL:
	case VIRTIO_MMIO_STATUS:
		*(u32 *) (((void *)&dev->config) + offset) = val;
		break;
	case VIRTIO_MMIO_GUEST_FEATURES:
		if (dev->config.guest_features_sel == 0)  {
			dev->dev.emu->set_guest_features(&dev->dev, val);
		}
		break;
	case VIRTIO_MMIO_GUEST_PAGE_SIZE:
		dev->config.guest_page_size = val;
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		dev->config.queue_num = val;
		dev->dev.emu->set_size_vq(&dev->dev, dev->config.queue_sel,
						dev->config.queue_num);
		break;
	case VIRTIO_MMIO_QUEUE_ALIGN:
		dev->config.queue_align = val;
		break;
	case VIRTIO_MMIO_QUEUE_PFN:
		dev->dev.emu->init_vq(&dev->dev, dev->config.queue_sel,
					dev->config.guest_page_size,
					dev->config.queue_align,
					val);
		break;
	case VIRTIO_MMIO_QUEUE_NOTIFY:
		dev->dev.emu->notify_vq(&dev->dev, val);
		break;
	case VIRTIO_MMIO_INTERRUPT_ACK:
		dev->config.interrupt_state &= ~val;
		vmm_devemu_emulate_irq(dev->guest, dev->irq, 0);
		break;
	default:
		break;
	};

	return rc;
}

static int virtio_mmio_write(struct vmm_emudev *edev,
			     physical_addr_t offset,
			     void *src, u32 src_len)
{
	struct virtio_mmio_dev *s = edev->priv;

	/* Device specific config write */
	if (offset >= VIRTIO_MMIO_CONFIG) {
		offset -= VIRTIO_MMIO_CONFIG;
		return virtio_config_write(&s->dev, (u32)offset, src, src_len);
	}

	return virtio_mmio_config_write(s, (u32) offset, src, src_len);
}

static int virtio_mmio_reset(struct vmm_emudev *edev)
{
	struct virtio_mmio_dev *s = edev->priv;

	return virtio_reset(&s->dev);
}

static struct virtio_transport mmio_tra = {
	.name = "virtio_mmio",
	.notify = virtio_mmio_notify,
};

static int virtio_mmio_probe(struct vmm_guest *guest,
			     struct vmm_emudev *edev,
			     const struct vmm_emuid *eid)
{
	int rc = VMM_OK;
	const char *attr;
	struct virtio_mmio_dev *s;

	s = vmm_zalloc(sizeof(struct virtio_mmio_dev));
	if (!s) {
		rc = VMM_EFAIL;
		goto virtio_mmio_probe_done;
	}

	s->guest = guest;

	vmm_snprintf(s->dev.name, VIRTIO_DEVICE_MAX_NAME_LEN, 
		     "%s/%s", guest->node->name, edev->node->name); 
	s->dev.edev = edev;
	s->dev.tra = &mmio_tra;
	s->dev.tra_data = s;
	s->dev.guest = guest;

	s->config = (struct virtio_mmio_config) {
		.magic          = {'v', 'i', 'r', 't'},
		.version        = 1,
		.vendor_id      = 0x52535658, /* XVSR */
		.queue_num_max  = 256,
	};

	attr = vmm_devtree_attrval(edev->node, "virtio_type");
	if (attr) {
		s->dev.id.type = *((u32 *)attr);
	} else {
		rc = VMM_EFAIL;
		goto virtio_mmio_probe_freestate_fail;
	}

	s->config.device_id = s->dev.id.type;

	attr = vmm_devtree_attrval(edev->node, "irq");
	if (attr) {
		s->irq = *((u32 *)attr);
	} else {
		rc = VMM_EFAIL;
		goto virtio_mmio_probe_freestate_fail;
	}

	if ((rc = virtio_register_device(&s->dev))) {
		goto virtio_mmio_probe_freestate_fail;
	}

	edev->priv = s;

	goto virtio_mmio_probe_done;

virtio_mmio_probe_freestate_fail:
	vmm_free(s);
virtio_mmio_probe_done:
	return rc;
}

static int virtio_mmio_remove(struct vmm_emudev *edev)
{
	struct virtio_mmio_dev *s = edev->priv;

	if (s) {
		virtio_unregister_device((struct virtio_device *)&s->dev);
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_emuid virtio_mmio_emuid_table[] = {
	{ .type = "virtio", 
	  .compatible = "virtio,mmio", 
	},
	{ /* end of list */ },
};

static struct vmm_emulator virtio_mmio = {
	.name = "virtio_mmio",
	.match_table = virtio_mmio_emuid_table,
	.probe = virtio_mmio_probe,
	.read = virtio_mmio_read,
	.write = virtio_mmio_write,
	.reset = virtio_mmio_reset,
	.remove = virtio_mmio_remove,
};

static int __init virtio_mmio_init(void)
{
	return vmm_devemu_register_emulator(&virtio_mmio);
}

static void __exit virtio_mmio_exit(void)
{
	vmm_devemu_unregister_emulator(&virtio_mmio);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
