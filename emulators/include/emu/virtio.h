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
 * @file virtio.h
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO Core Framework Interface
 */
#ifndef __VIRTIO_H__
#define __VIRTIO_H__

#include <vmm_types.h>
#include <libs/list.h>

#include <emu/virtio_queue.h>

/** VirtIO module intialization priority */
#define VIRTIO_IPRIORITY				1

#define VIRTIO_DEVICE_MAX_NAME_LEN			32

enum virtio_id {
	VIRTIO_ID_NET = 1,
	VIRTIO_ID_BLOCK,
	VIRTIO_ID_CONSOLE,
	VIRTIO_ID_ENTROPY,
	VIRTIO_ID_BALLON,
	VIRTIO_ID_IO_MEMORY,
	VIRTIO_ID_RPMSG,
	VIRTIO_ID_SCSI_HOST,
	VIRTIO_ID_9P
};

struct virtio_device_id {
	u32 type;
};

struct virtio_device {
	char name[VIRTIO_DEVICE_MAX_NAME_LEN];
	struct vmm_emudev *edev;

	struct virtio_device_id id;

	struct virtio_transport *tra;
	void *tra_data;

	struct virtio_emulator *emu;
	void *emu_data;

	struct dlist node;
	struct vmm_guest *guest;
};

struct virtio_transport {
	const char *name;

	int  (*notify)(struct virtio_device *, u32 vq);
};

struct virtio_emulator {
	const char *name;
	const struct virtio_device_id *id_table;

	/* VirtIO operations */
	u32 (*get_host_features) (struct virtio_device *dev);
	void (*set_guest_features) (struct virtio_device *dev, u32 features);
	int (*init_vq) (struct virtio_device *dev, u32 vq, u32 page_size,
				u32 align, u32 pfn);
	int (*get_pfn_vq) (struct virtio_device *dev, u32 vq);
	int (*get_size_vq) (struct virtio_device *dev, u32 vq);
	int (*set_size_vq) (struct virtio_device *dev, u32 vq, int size);
	int (*notify_vq) (struct virtio_device *dev , u32 vq);

	/* Emulator operations */
	int (*read_config)(struct virtio_device *dev, u32 offset, void *dst,
				u32 dst_len);
	int (*write_config)(struct virtio_device *dev,u32 offset, void *src,
				u32 src_len);
	int (*reset)(struct virtio_device *dev);
	int  (*connect)(struct virtio_device *dev, struct virtio_emulator *emu);
	void (*disconnect)(struct virtio_device *dev);

	struct dlist node;
};

int virtio_config_read(struct virtio_device *dev,
			u32 offset, void *dst, u32 dst_len);

int virtio_config_write(struct virtio_device *dev,
			u32 offset, void *src, u32 src_len);

int virtio_reset(struct virtio_device *dev);

int virtio_register_device(struct virtio_device *dev);

void virtio_unregister_device(struct virtio_device *dev);

int virtio_register_emulator(struct virtio_emulator *emu);

void virtio_unregister_emulator(struct virtio_emulator *emu);

#endif /* __VIRTIO_H__ */
