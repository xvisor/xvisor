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
 * @file virtio_host.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO host device driver framework.
 *
 * The source has been largely adapted from Linux
 * drivers/virtio/virtio.c
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_modules.h>
#include <vmm_mutex.h>
#include <drv/virtio_host.h>
#include <libs/idr.h>

#define MODULE_DESC			"VirtIO Host Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		VIRTIO_HOST_IPRIORITY
#define	MODULE_INIT			virtio_host_init
#define	MODULE_EXIT			virtio_host_exit

static DEFINE_IDA(virtio_index_ida);

/* ========== VirtIO host queue APIs ========== */

/* TODO: */

/* ========== VirtIO host device driver APIs ========== */

static void add_status(struct virtio_host_device *vdev, unsigned s)
{
	vdev->config->set_status(vdev, vdev->config->get_status(vdev) | s);
}

static void __virtio_host_config_changed(struct virtio_host_device *vdev)
{
	struct virtio_host_driver *vdrv =
			to_virtio_host_driver(vdev->dev.driver);

	if (!vdev->config_enabled)
		vdev->config_change_pending = TRUE;
	else if (vdrv && vdrv->config_changed)
		vdrv->config_changed(vdev);
}

void virtio_host_config_changed(struct virtio_host_device *vdev)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&vdev->config_lock, flags);
	__virtio_host_config_changed(vdev);
	vmm_spin_unlock_irqrestore(&vdev->config_lock, flags);
}
VMM_EXPORT_SYMBOL(virtio_host_config_changed);

static void virtio_host_config_disable(struct virtio_host_device *vdev)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&vdev->config_lock, flags);
	vdev->config_enabled = FALSE;
	vmm_spin_unlock_irqrestore(&vdev->config_lock, flags);
}

static void virtio_host_config_enable(struct virtio_host_device *vdev)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&vdev->config_lock, flags);
	vdev->config_enabled = TRUE;
	if (vdev->config_change_pending)
		__virtio_host_config_changed(vdev);
	vdev->config_change_pending = FALSE;
	vmm_spin_unlock_irqrestore(&vdev->config_lock, flags);
}

static int virtio_host_finalize_features(struct virtio_host_device *vdev)
{
	unsigned status;
	int ret = vdev->config->finalize_features(vdev);

	if (ret)
		return ret;

	if (!virtio_host_has_feature(vdev, VMM_VIRTIO_F_VERSION_1))
		return 0;

	add_status(vdev, VMM_VIRTIO_CONFIG_S_FEATURES_OK);
	status = vdev->config->get_status(vdev);
	if (!(status & VMM_VIRTIO_CONFIG_S_FEATURES_OK)) {
		vmm_lerror(vdev->dev.name,
			   "virtio: device refuses features: %x\n",
			   status);
		return VMM_ENODEV;
	}

	return 0;
}

static int virtio_host_match_device(const struct virtio_host_device_id *ids,
				    struct virtio_host_device *vdev)
{
	while (ids->device) {
		if ((ids->device == VMM_VIRTIO_ID_ANY ||
		     ids->device == vdev->id.device) &&
		    (ids->vendor == VMM_VIRTIO_ID_ANY ||
		     ids->vendor == vdev->id.vendor))
			return 1;
		ids++;
	}
	return 0;
}

static int virtio_host_bus_match(struct vmm_device *dev,
				 struct vmm_driver *drv)
{
	struct virtio_host_device *vdev = to_virtio_host_device(dev);
	struct virtio_host_driver *vdrv = to_virtio_host_driver(drv);

	return virtio_host_match_device(vdrv->id_table, vdev);
}

static int virtio_host_driver_probe(struct vmm_device *dev)
{
	int err, i;
	struct virtio_host_device *vdev = to_virtio_host_device(dev);
	struct virtio_host_driver *vdrv = to_virtio_host_driver(dev->driver);
	u64 device_features;
	u64 driver_features;
	u64 driver_features_legacy;

	/* We have a driver! */
	add_status(vdev, VMM_VIRTIO_CONFIG_S_DRIVER);

	/* Figure out what features the device supports. */
	device_features = vdev->config->get_features(vdev);

	/* Figure out what features the driver supports. */
	driver_features = 0;
	for (i = 0; i < vdrv->feature_table_size; i++) {
		unsigned int f = vdrv->feature_table[i];
		BUG_ON(f >= 64);
		driver_features |= (1ULL << f);
	}

	/* Some drivers have a separate feature table for virtio v1.0 */
	if (vdrv->feature_table_legacy) {
		driver_features_legacy = 0;
		for (i = 0; i < vdrv->feature_table_size_legacy; i++) {
			unsigned int f = vdrv->feature_table_legacy[i];
			BUG_ON(f >= 64);
			driver_features_legacy |= (1ULL << f);
		}
	} else {
		driver_features_legacy = driver_features;
	}

	if (device_features & (1ULL << VMM_VIRTIO_F_VERSION_1))
		vdev->features = driver_features & device_features;
	else
		vdev->features = driver_features_legacy & device_features;

	/* Transport features always preserved to pass to finalize_features. */
	for (i = VMM_VIRTIO_TRANSPORT_F_START;
	     i < VMM_VIRTIO_TRANSPORT_F_END; i++)
		if (device_features & (1ULL << i))
			__virtio_host_set_bit(vdev, i);

	err = virtio_host_finalize_features(vdev);
	if (err)
		goto err;

	err = vdrv->probe(vdev);
	if (err)
		goto err;

	/* If probe didn't do it, mark device DRIVER_OK ourselves. */
	if (!(vdev->config->get_status(vdev) & VMM_VIRTIO_CONFIG_S_DRIVER_OK))
		virtio_host_device_ready(vdev);

	if (vdrv->scan)
		vdrv->scan(vdev);

	virtio_host_config_enable(vdev);

	return 0;
err:
	add_status(vdev, VMM_VIRTIO_CONFIG_S_FAILED);
	return err;
}

static int virtio_host_driver_remove(struct vmm_device *dev)
{
	struct virtio_host_device *vdev = to_virtio_host_device(dev);
	struct virtio_host_driver *vdrv = to_virtio_host_driver(dev->driver);

	virtio_host_config_disable(vdev);

	vdrv->remove(vdev);

	/* Driver should have reset device. */
	WARN_ON(vdev->config->get_status(vdev));

	/* Acknowledge the device's existence again. */
	add_status(vdev, VMM_VIRTIO_CONFIG_S_ACKNOWLEDGE);

	return 0;
}

static struct vmm_bus virtio_host_bus = {
	.name		= "virtio_host",
	.match		= virtio_host_bus_match,
	.probe		= virtio_host_driver_probe,
	.remove		= virtio_host_driver_remove,
};

int virtio_host_add_device(struct virtio_host_device *vdev,
			   struct vmm_device *parent)
{
	int err;

	if (!vdev) {
		return VMM_EINVALID;
	}

	/* Assign a unique device index and hence name. */
	err = ida_simple_get(&virtio_index_ida, 0, 0, 0);
	if (err < 0)
		goto out;

	vdev->index = err;

	vmm_devdrv_initialize_device(&vdev->dev);
	vdev->dev.parent = parent;
	vmm_snprintf(vdev->dev.name, sizeof(vdev->dev.name),
		     "virtio%u", vdev->index);

	INIT_SPIN_LOCK(&vdev->config_lock);
	vdev->config_enabled = FALSE;
	vdev->config_change_pending = FALSE;

	/* We always start by resetting the device, in case a previous
	 * driver messed it up.  This also tests that code path a little. */
	vdev->config->reset(vdev);

	/* Acknowledge that we've seen the device. */
	add_status(vdev, VMM_VIRTIO_CONFIG_S_ACKNOWLEDGE);

	INIT_LIST_HEAD(&vdev->vqs);

	err = vmm_devdrv_register_device(&vdev->dev);
out:
	if (err)
		add_status(vdev, VMM_VIRTIO_CONFIG_S_FAILED);
	return err;
}
VMM_EXPORT_SYMBOL(virtio_host_add_device);

void virtio_host_remove_device(struct virtio_host_device *vdev)
{
	int index = vdev->index;

	if (!vdev) {
		return;
	}

	vmm_devdrv_unregister_device(&vdev->dev);
	ida_simple_remove(&virtio_index_ida, index);
}
VMM_EXPORT_SYMBOL(virtio_host_remove_device);

int virtio_host_register_driver(struct virtio_host_driver *vdrv)
{
	if (!vdrv) {
		return VMM_EINVALID;
	}

	vdrv->drv.bus = &virtio_host_bus;
	if (strlcpy(vdrv->drv.name, vdrv->name, 
		sizeof(vdrv->drv.name)) >= sizeof(vdrv->drv.name)) {
		return VMM_EOVERFLOW;
	}

	return vmm_devdrv_register_driver(&vdrv->drv);
}
VMM_EXPORT_SYMBOL(virtio_host_register_driver);

void virtio_host_unregister_driver(struct virtio_host_driver *vdrv)
{
	if (!vdrv) {
		return;
	}

	vmm_devdrv_unregister_driver(&vdrv->drv);
}
VMM_EXPORT_SYMBOL(virtio_host_unregister_driver);

static int __init virtio_host_init(void)
{
	return vmm_devdrv_register_bus(&virtio_host_bus);
}

static void __exit virtio_host_exit(void)
{
	vmm_devdrv_unregister_bus(&virtio_host_bus);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
