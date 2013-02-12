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

static void virtio_reset_device(struct virtio_device *dev)
{
	if (dev->emu) {
		dev->emu->reset(dev);
	}
}

static int virtio_connect_emulator(struct virtio_device *dev,
				struct virtio_emulator *emu)
{
	return emu->connect(dev, emu);
}

static void virtio_disconnect_emulator(struct virtio_device *dev)
{
	if (dev->emu) {
		dev->emu->disconnect(dev);
	}
}

/*
 * virtio helper routines
 */

static int virtio_match_device(const struct virtio_device_id *ids,
				struct virtio_device *dev)
{
	while (ids->type) {
		if (ids->type == dev->id.type)
			return 1;
		ids++;
	}
	return 0;
}

static int virtio_bind_emulator(struct virtio_device *dev,
				struct virtio_emulator *emu)
{
	if (virtio_match_device(emu->id_table, dev)) {
		dev->emu = emu;
		if (virtio_connect_emulator(dev, emu)) {
			dev->emu = NULL;
			return VMM_ENODEV;
		}
	}
	return 0;
}

/* Note: Must be called with virtio_mutex held */
static void virtio_find_emulator(struct virtio_device *dev)
{
	struct dlist *l;
	struct virtio_emulator *emu;

	if (!dev || dev->emu) {
		return;
	}

	list_for_each(l, &virtio_emu_list) {
		emu = list_entry(l, struct virtio_emulator, node);
		if (!virtio_bind_emulator(dev, emu)) {
			break;
		}
	}
}

/* Note: Must be called with virtio_mutex held */
static void virtio_attach_emulator(struct virtio_emulator *emu)
{
	struct dlist *l;
	struct virtio_device *dev;

	if (!emu) {
		return;
	}

	list_for_each(l, &virtio_dev_list) {
		dev = list_entry(l, struct virtio_device, node);
		if (!dev->emu) {
			virtio_bind_emulator(dev, emu);
		}
	}
}

/*
 * virtio global APIs
 */

/* FIXME: */
int virtio_config_read(struct virtio_device *dev,
			u32 offset, void *dst, u32 dst_len)
{
	if (dev->emu) {
	                dev->emu->read_config(dev, offset, dst, dst_len);
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(virtio_config_read);

/* FIXME: */
int virtio_config_write(struct virtio_device *dev,
			u32 offset, void *src, u32 src_len)
{
	if (dev->emu) {
	                dev->emu->write_config(dev, offset, src, src_len);
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(virtio_config_write);

/* FIXME: */
int virtio_reset(struct virtio_device *dev)
{
	if (!dev) {
		return VMM_EFAIL;
	}

	virtio_reset_device(dev);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(virtio_reset);

/* FIXME: */
int virtio_register_device(struct virtio_device *dev)
{
	if (!dev || !dev->tra) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&dev->node);
	dev->emu = NULL;
	dev->emu_data = NULL;

	vmm_mutex_lock(&virtio_mutex);

	list_add_tail(&dev->node, &virtio_dev_list);
	virtio_find_emulator(dev);

	vmm_mutex_unlock(&virtio_mutex);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(virtio_register_device);

/* FIXME: */
void virtio_unregister_device(struct virtio_device *dev)
{
	if (!dev) {
		return;
	}

	vmm_mutex_lock(&virtio_mutex);

	virtio_disconnect_emulator(dev);
	list_del(&dev->node);

	vmm_mutex_unlock(&virtio_mutex);
}
VMM_EXPORT_SYMBOL(virtio_unregister_device);

/* FIXME: */
int virtio_register_emulator(struct virtio_emulator *emu)
{
	bool found;
	struct dlist *l;
	struct virtio_emulator *vemu;

	if (!emu) {
		return VMM_EFAIL;
	}

	vemu = NULL;
	found = FALSE;

	vmm_mutex_lock(&virtio_mutex);

	list_for_each(l, &virtio_emu_list) {
		vemu = list_entry(l, struct virtio_emulator, node);
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

	virtio_attach_emulator(emu);

	vmm_mutex_unlock(&virtio_mutex);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(virtio_register_emulator);

/* FIXME: */
void virtio_unregister_emulator(struct virtio_emulator *emu)
{
	struct virtio_device *dev;

	vmm_mutex_lock(&virtio_mutex);

	list_del(&emu->node);

	list_for_each_entry(dev, &virtio_dev_list, node) {
		if (dev->emu == emu) {
			virtio_disconnect_emulator(dev);
			virtio_find_emulator(dev);
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
