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
#include <vmm_stdio.h>
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
	struct virtio_mmio_dev *m = dev->tra_data;

	m->config.interrupt_state |= VIRTIO_MMIO_INT_VRING;

	vmm_devemu_emulate_irq(m->guest, m->irq, 1);

	return VMM_OK;
}

int virtio_mmio_config_read(struct virtio_mmio_dev *m,
			    u32 offset, void *dst,
			    u32 dst_len)
{
	int rc = VMM_OK;

	switch (offset) {
	case VIRTIO_MMIO_MAGIC_VALUE:
	case VIRTIO_MMIO_VERSION:
	case VIRTIO_MMIO_DEVICE_ID:
	case VIRTIO_MMIO_VENDOR_ID:
	case VIRTIO_MMIO_STATUS:
	case VIRTIO_MMIO_INTERRUPT_STATUS:
		*(u32 *)dst = (*(u32 *)(((void *)&m->config) + offset));
		break;
	case VIRTIO_MMIO_HOST_FEATURES:
		*(u32 *)dst = m->dev.emu->get_host_features(&m->dev);
		break;
	case VIRTIO_MMIO_QUEUE_PFN:
		*(u32 *)dst = m->dev.emu->get_pfn_vq(&m->dev,
					     m->config.queue_sel);
		break;
	case VIRTIO_MMIO_QUEUE_NUM_MAX:
		*(u32 *)dst = m->dev.emu->get_size_vq(&m->dev,
					      m->config.queue_sel);
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
	struct virtio_mmio_dev *m = edev->priv;

	/* Device specific config write */
	if (offset >= VIRTIO_MMIO_CONFIG) {
		offset -= VIRTIO_MMIO_CONFIG;
		return virtio_config_read(&m->dev, (u32)offset, dst, dst_len);
	}

	return virtio_mmio_config_read(m, (u32)offset, dst, dst_len);
}

static int virtio_mmio_config_write(struct virtio_mmio_dev *m,
				    physical_addr_t offset,
				    void *src, u32 src_len)
{
	int rc = VMM_OK;
	u32 val = vmm_cpu_to_le32(*(u32 *)(src));

	switch (offset) {
	case VIRTIO_MMIO_HOST_FEATURES_SEL:
	case VIRTIO_MMIO_GUEST_FEATURES_SEL:
	case VIRTIO_MMIO_QUEUE_SEL:
	case VIRTIO_MMIO_STATUS:
		*(u32 *)(((void *)&m->config) + offset) = val;
		break;
	case VIRTIO_MMIO_GUEST_FEATURES:
		if (m->config.guest_features_sel == 0)  {
			m->dev.emu->set_guest_features(&m->dev, val);
		}
		break;
	case VIRTIO_MMIO_GUEST_PAGE_SIZE:
		m->config.guest_page_size = val;
		break;
	case VIRTIO_MMIO_QUEUE_NUM:
		m->config.queue_num = val;
		m->dev.emu->set_size_vq(&m->dev, 
					m->config.queue_sel,
					m->config.queue_num);
		break;
	case VIRTIO_MMIO_QUEUE_ALIGN:
		m->config.queue_align = val;
		break;
	case VIRTIO_MMIO_QUEUE_PFN:
		m->dev.emu->init_vq(&m->dev, 
				    m->config.queue_sel,
				    m->config.guest_page_size,
				    m->config.queue_align,
				    val);
		break;
	case VIRTIO_MMIO_QUEUE_NOTIFY:
		m->dev.emu->notify_vq(&m->dev, val);
		break;
	case VIRTIO_MMIO_INTERRUPT_ACK:
		m->config.interrupt_state &= ~val;
		vmm_devemu_emulate_irq(m->guest, m->irq, 0);
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
	struct virtio_mmio_dev *m = edev->priv;

	/* Device specific config write */
	if (offset >= VIRTIO_MMIO_CONFIG) {
		offset -= VIRTIO_MMIO_CONFIG;
		return virtio_config_write(&m->dev, (u32)offset, src, src_len);
	}

	return virtio_mmio_config_write(m, (u32)offset, src, src_len);
}

static int virtio_mmio_reset(struct vmm_emudev *edev)
{
	struct virtio_mmio_dev *m = edev->priv;

	m->config.interrupt_state = 0x0;
	vmm_devemu_emulate_irq(m->guest, m->irq, 0);

	return virtio_reset(&m->dev);
}

static struct virtio_transport mmio_tra = {
	.name = "virtio_mmio",
	.notify = virtio_mmio_notify,
};

static int virtio_mmio_probe(struct vmm_guest *guest,
			     struct vmm_emudev *edev,
			     const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	const char *attr;
	struct virtio_mmio_dev *m;

	m = vmm_zalloc(sizeof(struct virtio_mmio_dev));
	if (!m) {
		rc = VMM_EFAIL;
		goto virtio_mmio_probe_done;
	}

	m->guest = guest;

	vmm_snprintf(m->dev.name, VIRTIO_DEVICE_MAX_NAME_LEN, 
		     "%s/%s", guest->name, edev->node->name); 
	m->dev.edev = edev;
	m->dev.tra = &mmio_tra;
	m->dev.tra_data = m;
	m->dev.guest = guest;

	m->config = (struct virtio_mmio_config) {
		     .magic          = {'v', 'i', 'r', 't'},
		     .version        = 1,
		     .vendor_id      = 0x52535658, /* XVSR */
		     .queue_num_max  = 256,
	};

	attr = vmm_devtree_attrval(edev->node, "virtio_type");
	if (attr) {
		m->config.device_id = *((u32 *)attr);
	} else {
		rc = VMM_EFAIL;
		goto virtio_mmio_probe_freestate_fail;
	}

	m->dev.id.type = m->config.device_id;

	rc = vmm_devtree_irq_get(edev->node, &m->irq, 0);
	if (rc) {
		goto virtio_mmio_probe_freestate_fail;
	}

	if ((rc = virtio_register_device(&m->dev))) {
		goto virtio_mmio_probe_freestate_fail;
	}

	edev->priv = m;

	goto virtio_mmio_probe_done;

virtio_mmio_probe_freestate_fail:
	vmm_free(m);
virtio_mmio_probe_done:
	return rc;
}

static int virtio_mmio_remove(struct vmm_emudev *edev)
{
	struct virtio_mmio_dev *m = edev->priv;

	if (m) {
		virtio_unregister_device((struct virtio_device *)&m->dev);
		vmm_free(m);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid virtio_mmio_emuid_table[] = {
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
