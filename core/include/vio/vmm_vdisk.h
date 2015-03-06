/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file vmm_vdisk.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for virtual disk framework
 */

/* The virtual disk framework helps disk controller emulators
 * in emulating disk read/write operations irrespective to
 * disk controller type. It also provides a convient way of
 * tracking various virtual disk instances of a guest.
 *
 * Each virtual disk can be attached to a block device. If the
 * block device is unregistered then virtual disk is dettached
 * automatically using block device notifiers.
 */

#ifndef _VMM_VDISK_H__
#define _VMM_VDISK_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_notifier.h>
#include <block/vmm_blockdev.h>
#include <libs/list.h>

#define VMM_VDISK_IPRIORITY		(VMM_BLOCKDEV_CLASS_IPRIORITY + 1)

struct vmm_vdisk_request;
struct vmm_vdisk;

/** Types of block IO request */
enum vmm_vdisk_request_type {
	VMM_VDISK_REQUEST_UNKNOWN=0,
	VMM_VDISK_REQUEST_READ=1,
	VMM_VDISK_REQUEST_WRITE=2
};

/** Representation of a virtual disk request  */
struct vmm_vdisk_request {
	struct vmm_vdisk *vdisk;
	struct vmm_request r;
};

/** Representation of a virtual disk */
struct vmm_vdisk {
	struct dlist head;
	char name[VMM_FIELD_NAME_SIZE];
	u32 block_size;

	void (*completed)(struct vmm_vdisk *, struct vmm_vdisk_request *);
	void (*failed)(struct vmm_vdisk *, struct vmm_vdisk_request *);

	vmm_spinlock_t blk_lock; /* Protect blk pointer */
	struct vmm_blockdev *blk;
	u32 blk_factor;

	void *priv;
};

/* Notifier event when virtual disk is created */
#define VMM_VDISK_EVENT_CREATE		0x01
/* Notifier event when virtual disk is destroyed */
#define VMM_VDISK_EVENT_DESTROY		0x02

/** Representation of virtual disk notifier event */
struct vmm_vdisk_event {
	struct vmm_vdisk *vdisk;
	void *data;
};

/** Register a notifier client to receive virtual disk events */
int vmm_vdisk_register_client(struct vmm_notifier_block *nb);

/** Unregister a notifier client to not receive virtual disk events */
int vmm_vdisk_unregister_client(struct vmm_notifier_block *nb);

/** Retrive private context of virtual disk */
static inline void *vmm_vdisk_priv(struct vmm_vdisk *vdisk)
{
	return (vdisk) ? vdisk->priv : NULL;
}

/** Submit IO request to virtual disk */
int vmm_vdisk_submit_request(struct vmm_vdisk *vdisk,
			     struct vmm_vdisk_request *vreq,
			     enum vmm_vdisk_request_type type,
			     u64 lba, void *data, u32 data_len);

/* Abort IO request from virtual disk */
int vmm_vdisk_abort_request(struct vmm_vdisk *vdisk,
			    struct vmm_vdisk_request *vreq);

/** Flush cached IO from virtual disk */
int vmm_vdisk_flush_cache(struct vmm_vdisk *vdisk);

/** Name of virtual disk */
static inline const char *vmm_vdisk_name(struct vmm_vdisk *vdisk)
{
	return (vdisk) ? vdisk->name : NULL;
}

/** Block size of virtual disk */
static inline u32 vmm_vdisk_block_size(struct vmm_vdisk *vdisk)
{
	return (vdisk) ? vdisk->block_size : 0;
}

/** Block count of virtual disk based on attached block device */
u64 vmm_vdisk_capacity(struct vmm_vdisk *vdisk);

/** Current block device attached to virtual disk */
int vmm_vdisk_current_block_device(struct vmm_vdisk *vdisk,
				   char *buf, u32 buf_len);

/** Attach block device to virtual disk */
void vmm_vdisk_attach_block_device(struct vmm_vdisk *vdisk,
				   const char *bdev_name);

/** Detach block device from virtual disk */
void vmm_vdisk_detach_block_device(struct vmm_vdisk *vdisk);

/** Create a virtual disk */
struct vmm_vdisk *vmm_vdisk_create(const char *name, u32 block_size,
	void (*completed)(struct vmm_vdisk *, struct vmm_vdisk_request *),
	void (*failed)(struct vmm_vdisk *, struct vmm_vdisk_request *),
	const char *bdev_name,
	void *priv);

/** Destroy a virtual disk */
int vmm_vdisk_destroy(struct vmm_vdisk *vdisk);

/** Find a virtual disk with given name */
struct vmm_vdisk *vmm_vdisk_find(const char *name);

/** Iterate over each virtual disk */
int vmm_vdisk_iterate(struct vmm_vdisk *start, void *data,
		      int (*fn)(struct vmm_vdisk *vdisk, void *data));

/** Count of available virtual disks */
u32 vmm_vdisk_count(void);

#endif
