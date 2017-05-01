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
 * @file virtio_host_mmio.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO host MMIO transport driver.
 *
 * The source has been largely adapted from Linux
 * drivers/virtio/virtio_mmio.c
 *
 * Virtio memory mapped device driver
 *
 * Copyright 2011-2014, ARM Ltd.
 *
 * This module allows virtio devices to be used over a virtual, memory mapped
 * platform device.
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_devres.h>
#include <vio/vmm_virtio_mmio.h>
#include <drv/virtio_host.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(dev, msg...)		vmm_linfo(dev->name, msg)
#else
#define DPRINTF(dev, msg...)
#endif

#define MODULE_DESC			"VirtIO Host MMIO Transport Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(0)
#define	MODULE_INIT			virtio_host_mmio_init
#define	MODULE_EXIT			virtio_host_mmio_exit

/* The alignment to use between consumer and producer parts of vring.
 * Currently hardcoded to the page size. */
#define VIRTIO_HOST_MMIO_VRING_ALIGN	VMM_PAGE_SIZE

#define to_virtio_host_mmio_device(_plat_dev) \
	container_of(_plat_dev, struct virtio_host_mmio_device, vdev)

struct virtio_host_mmio_device {
	struct virtio_host_device vdev;
	struct vmm_device *dev;

	u32 irq;
	void *base;
	unsigned long version;

	/* a list of queues so we can dispatch IRQs */
	vmm_spinlock_t lock;
	struct dlist vqs;
};

struct virtio_host_mmio_vq_info {
	/* the actual VirtIO host queue */
	struct virtio_host_queue *vq;

	/* the list node for the VirtIO host queue list */
	struct dlist node;
};

/* Configuration interface */

static u64 vm_get_features(struct virtio_host_device *vdev)
{
	struct virtio_host_mmio_device *vm_dev =
				to_virtio_host_mmio_device(vdev);
	u64 features;

	vmm_writel(1, vm_dev->base + VMM_VIRTIO_MMIO_DEVICE_FEATURES_SEL);
	features = vmm_readl(vm_dev->base + VMM_VIRTIO_MMIO_DEVICE_FEATURES);
	features <<= 32;

	vmm_writel(0, vm_dev->base + VMM_VIRTIO_MMIO_DEVICE_FEATURES_SEL);
	features |= vmm_readl(vm_dev->base + VMM_VIRTIO_MMIO_DEVICE_FEATURES);

	return features;
}

static int vm_finalize_features(struct virtio_host_device *vdev)
{
	struct virtio_host_mmio_device *vm_dev =
				to_virtio_host_mmio_device(vdev);

	/* Give virtio_ring a chance to accept features. */
	virtio_host_transport_features(vdev);

	/* Make sure there is are no mixed devices */
	if (vm_dev->version == 2 &&
	    !__virtio_host_test_bit(vdev, VMM_VIRTIO_F_VERSION_1)) {
		vmm_lerror(vdev->dev.name, "New virtio-mmio devices "
		"(version 2) must provide VIRTIO_F_VERSION_1 feature!\n");
		return VMM_EINVALID;
	}

	vmm_writel(1, vm_dev->base + VMM_VIRTIO_MMIO_DRIVER_FEATURES_SEL);
	vmm_writel((u32)(vdev->features >> 32),
		   vm_dev->base + VMM_VIRTIO_MMIO_DRIVER_FEATURES);

	vmm_writel(0, vm_dev->base + VMM_VIRTIO_MMIO_DRIVER_FEATURES_SEL);
	vmm_writel((u32)vdev->features,
		   vm_dev->base + VMM_VIRTIO_MMIO_DRIVER_FEATURES);

	return 0;
}

static void vm_get(struct virtio_host_device *vdev, unsigned offset,
		   void *buf, unsigned len)
{
	struct virtio_host_mmio_device *vm_dev =
				to_virtio_host_mmio_device(vdev);
	void *base = vm_dev->base + VMM_VIRTIO_MMIO_CONFIG;
	u8 b;
	u16 w;
	u32 l;

	if (vm_dev->version == 1) {
		u8 *ptr = buf;
		int i;

		for (i = 0; i < len; i++)
			ptr[i] = vmm_readb(base + offset + i);
		return;
	}

	switch (len) {
	case 1:
		b = vmm_readb(base + offset);
		memcpy(buf, &b, sizeof(b));
		break;
	case 2:
		w = vmm_cpu_to_le16(vmm_readw(base + offset));
		memcpy(buf, &w, sizeof(w));
		break;
	case 4:
		l = vmm_cpu_to_le32(vmm_readl(base + offset));
		memcpy(buf, &l, sizeof(l));
		break;
	case 8:
		l = vmm_cpu_to_le32(vmm_readl(base + offset));
		memcpy(buf, &l, sizeof(l));
		l = vmm_cpu_to_le32(vmm_readl(base + offset + sizeof(l)));
		memcpy(buf + sizeof(l), &l, sizeof(l));
		break;
	default:
		BUG();
	}
}

static void vm_set(struct virtio_host_device *vdev, unsigned offset,
		   const void *buf, unsigned len)
{
	struct virtio_host_mmio_device *vm_dev =
				to_virtio_host_mmio_device(vdev);
	void *base = vm_dev->base + VMM_VIRTIO_MMIO_CONFIG;
	u8 b;
	u16 w;
	u32 l;

	if (vm_dev->version == 1) {
		const u8 *ptr = buf;
		int i;

		for (i = 0; i < len; i++)
			vmm_writeb(ptr[i], base + offset + i);

		return;
	}

	switch (len) {
	case 1:
		memcpy(&b, buf, sizeof(b));
		vmm_writeb(b, base + offset);
		break;
	case 2:
		memcpy(&w, buf, sizeof(w));
		vmm_writew(vmm_le16_to_cpu(w), base + offset);
		break;
	case 4:
		memcpy(&l, buf, sizeof(l));
		vmm_writel(vmm_le32_to_cpu(l), base + offset);
		break;
	case 8:
		memcpy(&l, buf, sizeof(l));
		vmm_writel(vmm_le32_to_cpu(l), base + offset);
		memcpy(&l, buf + sizeof(l), sizeof(l));
		vmm_writel(vmm_le32_to_cpu(l), base + offset + sizeof(l));
		break;
	default:
		BUG();
	}
}

static u32 vm_generation(struct virtio_host_device *vdev)
{
	struct virtio_host_mmio_device *vm_dev =
				to_virtio_host_mmio_device(vdev);

	if (vm_dev->version == 1)
		return 0;
	else
		return vmm_readl(vm_dev->base +
				 VMM_VIRTIO_MMIO_CONFIG_GENERATION);
}

static u8 vm_get_status(struct virtio_host_device *vdev)
{
	struct virtio_host_mmio_device *vm_dev =
				to_virtio_host_mmio_device(vdev);

	return vmm_readl(vm_dev->base + VMM_VIRTIO_MMIO_STATUS) & 0xff;
}

static void vm_set_status(struct virtio_host_device *vdev, u8 status)
{
	struct virtio_host_mmio_device *vm_dev =
				to_virtio_host_mmio_device(vdev);

	/* We should never be setting status to 0. */
	BUG_ON(status == 0);

	vmm_writel(status, vm_dev->base + VMM_VIRTIO_MMIO_STATUS);
}

static void vm_reset(struct virtio_host_device *vdev)
{
	struct virtio_host_mmio_device *vm_dev =
				to_virtio_host_mmio_device(vdev);

	/* 0 status means a reset. */
	vmm_writel(0, vm_dev->base + VMM_VIRTIO_MMIO_STATUS);
}

/* Transport interface */

/* The notify function used when creating a virt queue */
static bool vm_notify(struct virtio_host_queue *vq)
{
	struct virtio_host_mmio_device *vm_dev =
				to_virtio_host_mmio_device(vq->vdev);

	/* We write the queue's selector into the notification register to
	 * signal the other end */
	vmm_writel(vq->index, vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_NOTIFY);
	return TRUE;
}

/* Notify all virtqueues on an interrupt. */
static vmm_irq_return_t vm_interrupt(int irq, void *opaque)
{
	struct virtio_host_mmio_device *vm_dev = opaque;
	struct virtio_host_mmio_vq_info *info;
	unsigned long status;
	irq_flags_t flags;
	vmm_irq_return_t ret = VMM_IRQ_NONE;

	/* Read and acknowledge interrupts */
	status = vmm_readl(vm_dev->base + VMM_VIRTIO_MMIO_INTERRUPT_STATUS);
	vmm_writel(status, vm_dev->base + VMM_VIRTIO_MMIO_INTERRUPT_ACK);

	if (unlikely(status & VMM_VIRTIO_MMIO_INT_CONFIG)) {
		virtio_host_config_changed(&vm_dev->vdev);
		ret = VMM_IRQ_HANDLED;
	}

	if (likely(status & VMM_VIRTIO_MMIO_INT_VRING)) {
		vmm_spin_lock_irqsave(&vm_dev->lock, flags);
		list_for_each_entry(info, &vm_dev->vqs, node)
			ret |= virtio_host_queue_interrupt(irq, info->vq);
		vmm_spin_unlock_irqrestore(&vm_dev->lock, flags);
	}

	return ret;
}

static void vm_del_vq(struct virtio_host_queue *vq)
{
	struct virtio_host_mmio_device *vm_dev =
			to_virtio_host_mmio_device(vq->vdev);
	struct virtio_host_mmio_vq_info *info = vq->priv;
	unsigned long flags;
	unsigned int index = vq->index;

	vmm_spin_lock_irqsave(&vm_dev->lock, flags);
	list_del(&info->node);
	vmm_spin_unlock_irqrestore(&vm_dev->lock, flags);

	/* Select and deactivate the queue */
	vmm_writel(index, vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_SEL);
	if (vm_dev->version == 1) {
		vmm_writel(0, vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_PFN);
	} else {
		vmm_writel(0, vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_READY);
		WARN_ON(vmm_readl(vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_READY));
	}

	virtio_host_destroy_queue(vq);

	vmm_free(info);
}

static void vm_del_vqs(struct virtio_host_device *vdev)
{
	struct virtio_host_queue *vq, *n;

	list_for_each_entry_safe(vq, n, &vdev->vqs, head)
		vm_del_vq(vq);
}

static struct virtio_host_queue *vm_setup_vq(struct virtio_host_device *vdev,
					unsigned index,
					virtio_host_queue_callback_t callback,
					const char *name)
{
	struct virtio_host_mmio_device *vm_dev =
				to_virtio_host_mmio_device(vdev);
	struct virtio_host_mmio_vq_info *info;
	struct virtio_host_queue *vq;
	irq_flags_t flags;
	unsigned int num;
	int err;

	if (!name)
		return NULL;

	/* Select the queue we're interested in */
	vmm_writel(index, vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_SEL);

	/* Queue shouldn't already be set up. */
	if (vmm_readl(vm_dev->base + (vm_dev->version == 1 ?
		VMM_VIRTIO_MMIO_QUEUE_PFN : VMM_VIRTIO_MMIO_QUEUE_READY))) {
		err = VMM_ENOENT;
		goto error_available;
	}

	/* Allocate and fill out our active queue description */
	info = vmm_zalloc(sizeof(*info));
	if (!info) {
		err = VMM_ENOMEM;
		goto error_zalloc;
	}

	num = vmm_readl(vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_NUM_MAX);
	if (num == 0) {
		err = VMM_ENOENT;
		goto error_new_vq;
	}

	/* Create the vring */
	vq = virtio_host_create_queue(index, num,
				      VIRTIO_HOST_MMIO_VRING_ALIGN, vdev,
				      TRUE, vm_notify, callback, name);
	if (!vq) {
		err = VMM_ENOMEM;
		goto error_new_vq;
	}

	/* Activate the queue */
	vmm_writel(virtio_host_queue_get_vring_size(vq),
		   vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_NUM);
	if (vm_dev->version == 1) {
		vmm_writel(VMM_PAGE_SIZE,
			   vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_ALIGN);
		vmm_writel(virtio_host_queue_get_desc_addr(vq) >> VMM_PAGE_SHIFT,
			   vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_PFN);
	} else {
		u64 addr = virtio_host_queue_get_desc_addr(vq);

		vmm_writel((u32)addr,
			   vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_DESC_LOW);
		vmm_writel((u32)(addr >> 32),
			   vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_DESC_HIGH);

		addr = virtio_host_queue_get_avail_addr(vq);
		vmm_writel((u32)addr,
			   vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_AVAIL_LOW);
		vmm_writel((u32)(addr >> 32),
			   vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_AVAIL_HIGH);

		addr = virtio_host_queue_get_used_addr(vq);
		vmm_writel((u32)addr,
			   vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_USED_LOW);
		vmm_writel((u32)(addr >> 32),
			   vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_USED_HIGH);

		vmm_writel(1, vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_READY);
	}

	vq->priv = info;
	info->vq = vq;

	vmm_spin_lock_irqsave(&vm_dev->lock, flags);
	list_add(&info->node, &vm_dev->vqs);
	vmm_spin_unlock_irqrestore(&vm_dev->lock, flags);

	return vq;

error_new_vq:
	if (vm_dev->version == 1) {
		vmm_writel(0, vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_PFN);
	} else {
		vmm_writel(0, vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_READY);
		WARN_ON(vmm_readl(vm_dev->base + VMM_VIRTIO_MMIO_QUEUE_READY));
	}
	vmm_free(info);
error_zalloc:
error_available:
	return VMM_ERR_PTR(err);
}

static int vm_find_vqs(struct virtio_host_device *vdev, unsigned nvqs,
		       struct virtio_host_queue *vqs[],
		       virtio_host_queue_callback_t callbacks[],
		       const char * const names[])
{
	int i;

	for (i = 0; i < nvqs; ++i) {
		vqs[i] = vm_setup_vq(vdev, i, callbacks[i], names[i]);
		if (VMM_IS_ERR(vqs[i])) {
			vm_del_vqs(vdev);
			return VMM_PTR_ERR(vqs[i]);
		}
	}

	return 0;
}

static const char *vm_bus_name(struct virtio_host_device *vdev)
{
	struct virtio_host_mmio_device *vm_dev =
				to_virtio_host_mmio_device(vdev);

	return vm_dev->dev->name;
}

static const struct virtio_host_config_ops virtio_host_mmio_config_ops = {
	.get		= vm_get,
	.set		= vm_set,
	.generation	= vm_generation,
	.get_status	= vm_get_status,
	.set_status	= vm_set_status,
	.reset		= vm_reset,
	.find_vqs	= vm_find_vqs,
	.del_vqs	= vm_del_vqs,
	.get_features	= vm_get_features,
	.finalize_features = vm_finalize_features,
	.bus_name	= vm_bus_name,
};

static int virtio_host_mmio_probe(struct vmm_device *dev,
				  const struct vmm_devtree_nodeid *devid)
{
	int ret = 0;
	unsigned long magic;
	virtual_addr_t base;
	struct virtio_host_mmio_device *vm_dev;

	vm_dev = vmm_devm_zalloc(dev, sizeof(*vm_dev));
	if (vm_dev == NULL)
		return VMM_ENOMEM;

	vm_dev->dev = dev;
	INIT_SPIN_LOCK(&vm_dev->lock);
	INIT_LIST_HEAD(&vm_dev->vqs);

	vm_dev->irq = vmm_devtree_irq_parse_map(dev->of_node, 0);
	if (!vm_dev->irq) {
		vmm_lerror(dev->name,
			   "Failed to parse and map IRQ\n");
		ret = VMM_ENODEV;
		goto fail;
	}
	if ((ret = vmm_host_irq_register(vm_dev->irq, dev->name,
					 vm_interrupt, vm_dev))) {
		vmm_lerror(dev->name,
			   "Failed to register IRQ handler: %d\n", ret);
		goto fail;
	}

	ret = vmm_devtree_request_regmap(dev->of_node, &base, 0,
					"VIRTIO_HOST_MMIO");
	if (ret) {
		vmm_lerror(dev->name,
			   "Failed to map regs: %d\n", ret);
		goto fail_unreg_irq;
	}
	vm_dev->base = (void *)base;

	/* Check magic value */
	magic = vmm_readl(vm_dev->base + VMM_VIRTIO_MMIO_MAGIC_VALUE);
	if (magic != ('v' | 'i' << 8 | 'r' << 16 | 't' << 24)) {
		vmm_lerror(dev->name, "Wrong magic value 0x%08lx!\n", magic);
		ret = VMM_ENODEV;
		goto fail_unreg_base;
	}

	/* Check device version */
	vm_dev->version = vmm_readl(vm_dev->base + VMM_VIRTIO_MMIO_VERSION);
	if (vm_dev->version < 1 || vm_dev->version > 2) {
		vmm_lerror(dev->name, "Version %ld not supported!\n",
			   vm_dev->version);
		ret = VMM_ENODEV;
		goto fail_unreg_base;
	}

	vm_dev->vdev.id.device = vmm_readl(vm_dev->base +
					   VMM_VIRTIO_MMIO_DEVICE_ID);
	if (vm_dev->vdev.id.device == 0) {
		/*
		 * virtio-mmio device with an ID 0 is a (dummy) placeholder
		 * with no function. End probing now with no error reported.
		 */
		ret = VMM_ENODEV;
		goto fail_unreg_base;
	}
	vm_dev->vdev.id.vendor = vmm_readl(vm_dev->base +
					   VMM_VIRTIO_MMIO_VENDOR_ID);

	if (vm_dev->version == 1)
		vmm_writel(VMM_PAGE_SIZE, vm_dev->base +
			   VMM_VIRTIO_MMIO_GUEST_PAGE_SIZE);

	vmm_devdrv_set_data(dev, vm_dev);

	ret = virtio_host_add_device(&vm_dev->vdev,
				     &virtio_host_mmio_config_ops, dev);
	if (ret) {
		vmm_lerror(dev->name,
			   "Failed to register VirtIO host device!\n");
		goto fail_unreg_base;
	}

	vmm_linfo(dev->name, "VirtIO host MMIO device v%ld\n",
		  vm_dev->version);

	return VMM_OK;

fail_unreg_base:
	vmm_devtree_regunmap_release(dev->of_node,
				     (virtual_addr_t)vm_dev->base, 0);
fail_unreg_irq:
	vmm_host_irq_unregister(vm_dev->irq, vm_dev);
fail:
	return ret;
}

static int virtio_host_mmio_remove(struct vmm_device *dev)
{
	struct virtio_host_mmio_device *vm_dev = vmm_devdrv_get_data(dev);

	virtio_host_remove_device(&vm_dev->vdev);
	vmm_devtree_regunmap_release(dev->of_node,
				     (virtual_addr_t)vm_dev->base, 0);
	vmm_host_irq_unregister(vm_dev->irq, vm_dev);

	return 0;
}

static struct vmm_devtree_nodeid virtio_host_mmio_devid_table[] = {
	{ .compatible = "virtio,mmio" },
	{ /* end of list */ },
};

static struct vmm_driver virtio_host_mmio_driver = {
	.name = "virtio_host_mmio",
	.match_table = virtio_host_mmio_devid_table,
	.probe = virtio_host_mmio_probe,
	.remove = virtio_host_mmio_remove,
};

static int __init virtio_host_mmio_init(void)
{
	return vmm_devdrv_register_driver(&virtio_host_mmio_driver);
}

static void __exit virtio_host_mmio_exit(void)
{
	vmm_devdrv_unregister_driver(&virtio_host_mmio_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
