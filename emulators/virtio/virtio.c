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
 * @file virtio.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO Core Framework Implementation
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <emu/virtio.h>

#define MODULE_DESC			"VirtIO Core"
#define MODULE_AUTHOR			"Pranav Sawargaonkar"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		VIRTIO_IPRIORITY
#define	MODULE_INIT			virtio_core_init
#define	MODULE_EXIT			virtio_core_exit

/*
 * virtio_mutex protects entire virtio subsystem and is taken every time
 * virtio device or emulator is registered or unregistered.
 */

static DEFINE_MUTEX(virtio_mutex);

static LIST_HEAD(virtio_dev_list);

static LIST_HEAD(virtio_emu_list);

/*
 * virtio_device & virtio_emulator operations
 */

static int __virtio_reset_emulator(struct virtio_device *dev)
{
	if (dev && dev->emu && dev->emu->reset) {
		return dev->emu->reset(dev);
	}

	return VMM_OK;
}

static int __virtio_connect_emulator(struct virtio_device *dev,
				     struct virtio_emulator *emu)
{
	if (dev && emu && emu->connect) {
		return emu->connect(dev, emu);
	}

	return VMM_OK;
}

static void __virtio_disconnect_emulator(struct virtio_device *dev)
{
	if (dev && dev->emu && dev->emu->disconnect) {
		dev->emu->disconnect(dev);
	}
}

static int __virtio_config_read_emulator(struct virtio_device *dev,
					 u32 offset, void *dst, u32 dst_len)
{
	if (dev && dev->emu && dev->emu->read_config) {
		return dev->emu->read_config(dev, offset, dst, dst_len);
	}

	return VMM_OK;
}

static int __virtio_config_write_emulator(struct virtio_device *dev,
					  u32 offset, void *src, u32 src_len)
{
	if (dev && dev->emu && dev->emu->write_config) {
		return dev->emu->write_config(dev, offset, src, src_len);
	}

	return VMM_OK;
}

/*
 * virtio helper routines
 */

static bool __virtio_match_device(const struct virtio_device_id *ids,
				  struct virtio_device *dev)
{
	while (ids->type) {
		if (ids->type == dev->id.type)
			return TRUE;
		ids++;
	}
	return FALSE;
}

static int __virtio_bind_emulator(struct virtio_device *dev,
				  struct virtio_emulator *emu)
{
	int rc = VMM_EINVALID;
	if (__virtio_match_device(emu->id_table, dev)) {
		dev->emu = emu;
		if ((rc = __virtio_connect_emulator(dev, emu))) {
			dev->emu = NULL;
		}
	}
	return rc;
}

static int __virtio_find_emulator(struct virtio_device *dev)
{
	struct virtio_emulator *emu;

	if (!dev || dev->emu) {
		return VMM_EINVALID;
	}

	list_for_each_entry(emu, &virtio_emu_list, node) {
		if (!__virtio_bind_emulator(dev, emu)) {
			return VMM_OK;
		}
	}

	return VMM_EFAIL;
}

static void __virtio_attach_emulator(struct virtio_emulator *emu)
{
	struct virtio_device *dev;

	if (!emu) {
		return;
	}

	list_for_each_entry(dev, &virtio_dev_list, node) {
		if (!dev->emu) {
			__virtio_bind_emulator(dev, emu);
		}
	}
}

/*
 * virtio global APIs
 */

int virtio_config_read(struct virtio_device *dev,
			u32 offset, void *dst, u32 dst_len)
{
	return __virtio_config_read_emulator(dev, offset, dst, dst_len);
}
VMM_EXPORT_SYMBOL(virtio_config_read);

int virtio_config_write(struct virtio_device *dev,
			u32 offset, void *src, u32 src_len)
{
	return __virtio_config_write_emulator(dev, offset, src, src_len);
}
VMM_EXPORT_SYMBOL(virtio_config_write);

int virtio_reset(struct virtio_device *dev)
{
	return __virtio_reset_emulator(dev);
}
VMM_EXPORT_SYMBOL(virtio_reset);

int virtio_register_device(struct virtio_device *dev)
{
	int rc = VMM_OK;

	if (!dev || !dev->tra) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&dev->node);
	dev->emu = NULL;
	dev->emu_data = NULL;

	vmm_mutex_lock(&virtio_mutex);

	list_add_tail(&dev->node, &virtio_dev_list);
	rc = __virtio_find_emulator(dev);

	vmm_mutex_unlock(&virtio_mutex);

	return rc;
}
VMM_EXPORT_SYMBOL(virtio_register_device);

void virtio_unregister_device(struct virtio_device *dev)
{
	if (!dev) {
		return;
	}

	vmm_mutex_lock(&virtio_mutex);

	__virtio_disconnect_emulator(dev);
	list_del(&dev->node);

	vmm_mutex_unlock(&virtio_mutex);
}
VMM_EXPORT_SYMBOL(virtio_unregister_device);

int virtio_register_emulator(struct virtio_emulator *emu)
{
	bool found;
	struct virtio_emulator *vemu;

	if (!emu) {
		return VMM_EFAIL;
	}

	vemu = NULL;
	found = FALSE;

	vmm_mutex_lock(&virtio_mutex);

	list_for_each_entry(vemu, &virtio_emu_list, node) {
		if (strcmp(vemu->name, emu->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&virtio_mutex);
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&emu->node);
	list_add_tail(&emu->node, &virtio_emu_list);

	__virtio_attach_emulator(emu);

	vmm_mutex_unlock(&virtio_mutex);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(virtio_register_emulator);

void virtio_unregister_emulator(struct virtio_emulator *emu)
{
	struct virtio_device *dev;

	vmm_mutex_lock(&virtio_mutex);

	list_del(&emu->node);

	list_for_each_entry(dev, &virtio_dev_list, node) {
		if (dev->emu == emu) {
			__virtio_disconnect_emulator(dev);
			__virtio_find_emulator(dev);
		}
	}

	vmm_mutex_unlock(&virtio_mutex);
}
VMM_EXPORT_SYMBOL(virtio_unregister_emulator);

static int __init virtio_core_init(void)
{
	/* Nothing to be done */
	return VMM_OK;
}

static void __exit virtio_core_exit(void)
{
	/* Nothing to be done */
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
