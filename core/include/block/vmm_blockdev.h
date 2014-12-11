/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file vmm_blockdev.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Block Device framework header
 */

#ifndef __VMM_BLOCKDEV_H_
#define __VMM_BLOCKDEV_H_

#include <vmm_limits.h>
#include <vmm_types.h>
#include <vmm_devdrv.h>
#include <vmm_spinlocks.h>
#include <vmm_mutex.h>
#include <vmm_notifier.h>

#define VMM_BLOCKDEV_CLASS_NAME				"block"
#define VMM_BLOCKDEV_CLASS_IPRIORITY			1

/** Types of block IO request */
enum vmm_request_type {
	VMM_REQUEST_UNKNOWN=0,
	VMM_REQUEST_READ=1,
	VMM_REQUEST_WRITE=2
};

/** Representation of a block IO request */
struct vmm_request {
	struct vmm_blockdev *bdev; /* No need to set this field. 
				    * submit_request() will set this field.
				    */

	enum vmm_request_type type;
	u64 lba;
	u32 bcnt;
	void *data;

	void (*completed)(struct vmm_request *);
	void (*failed)(struct vmm_request *);
	void *priv;
};

/** Representation of a block IO request queue */
struct vmm_request_queue {
	/* Lock to protect the request queue operations */
	vmm_spinlock_t lock;

	/* Note: make_request must ensure that it calls
	 *
	 * vmm_blockdev_complete_request()
	 * OR
	 * vmm_blockdev_fail_request()
	 *
	 * for every request that it gets
	 */
	int (*make_request)(struct vmm_request_queue *rq, 
			    struct vmm_request *r);

	/* Note: abort_request will be called for successfully
	 * submited request only
	 */
	int (*abort_request)(struct vmm_request_queue *rq, 
			    struct vmm_request *r);

	/* Note: This is an optional callback only required
	 * if request queue does block caching 
	 */
	int (*flush_cache)(struct vmm_request_queue *rq);

	void *priv;
};

#define INIT_REQUEST_QUEUE(rq) \
		do { \
			INIT_SPIN_LOCK(&(rq)->lock); \
			(rq)->make_request = NULL; \
			(rq)->abort_request = NULL; \
			(rq)->flush_cache = NULL; \
			(rq)->priv = NULL; \
		} while (0)

/* Block device flags */
#define VMM_BLOCKDEV_RDONLY				0x00000001
#define VMM_BLOCKDEV_RW					0x00000002

/** Block device */
struct vmm_blockdev {
	struct dlist head;
	struct vmm_blockdev *parent;
	
	struct vmm_mutex child_lock;
	u32 child_count;
	struct dlist child_list;

	char name[VMM_FIELD_NAME_SIZE];
	char desc[VMM_FIELD_DESC_SIZE];
	struct vmm_device dev;

	u32 flags;
	u64 start_lba;
	u64 num_blocks;
	u32 block_size;

	struct vmm_request_queue *rq;

	/* NOTE: partition managment uses part_manager_sign and
	 * part_manager_priv for its own use.
	 * NOTE: part_manager_sign will be unique to partition style
	 * NOTE: part_manager_sign=0x0 is reserved and means unknown 
	 * partition style
	 */
	u32 part_manager_sign; /* To be used for partition managment */
	void *part_manager_priv; /* To be used for partition managment */
};

/* Notifier event when block device is registered */
#define VMM_BLOCKDEV_EVENT_REGISTER		0x01
/* Notifier event when block device is unregistered */
#define VMM_BLOCKDEV_EVENT_UNREGISTER		0x02

/** Representation of block device notifier event */
struct vmm_blockdev_event {
	struct vmm_blockdev *bdev;
	void *data;
};

/** Register a notifier client to receive block device events */
int vmm_blockdev_register_client(struct vmm_notifier_block *nb);

/** Unregister a notifier client to not receive block device events */
int vmm_blockdev_unregister_client(struct vmm_notifier_block *nb);

/** Size of block device in bytes */
static inline u64 vmm_blockdev_total_size(struct vmm_blockdev *bdev)
{
	return (bdev) ? bdev->num_blocks * bdev->block_size : 0;
}

/** Generic block IO complete request */
int vmm_blockdev_complete_request(struct vmm_request *r);

/** Generic block IO fail request */
int vmm_blockdev_fail_request(struct vmm_request *r);

/** Generic block IO submit request */
int vmm_blockdev_submit_request(struct vmm_blockdev *bdev,
				struct vmm_request *r);

/** Generic block IO abort request */
int vmm_blockdev_abort_request(struct vmm_request *r);

/** Generic block IO flush cached data 
 *  Note: block device request queue might cache blocks for 
 *  better performance. This API is a hint to request queue
 *  that dirty cached blocks need to written back.
 */
int vmm_blockdev_flush_cache(struct vmm_blockdev *bdev);

/** Generic block IO read/write
 *  Note: This is a blocking API hence must be 
 *  called from Orphan (or Thread) Context
 */
u64 vmm_blockdev_rw(struct vmm_blockdev *bdev, 
			enum vmm_request_type type,
			u8 *buf, u64 off, u64 len);

/** Generic block IO read */
#define vmm_blockdev_read(bdev, dst, off, len) \
	vmm_blockdev_rw((bdev), VMM_REQUEST_READ, (dst), (off), (len))

/** Generic block IO write */
#define vmm_blockdev_write(bdev, src, off, len) \
	vmm_blockdev_rw((bdev), VMM_REQUEST_WRITE, (src), (off), (len))

/** Allocate block device */
struct vmm_blockdev *vmm_blockdev_alloc(void);

/** Free block device */
void vmm_blockdev_free(struct vmm_blockdev *bdev);

/** Register block device to device driver framework 
 *  Note: Block device must have RDONLY or RW flag set. 
 */
int vmm_blockdev_register(struct vmm_blockdev *bdev);

/** Add child block device and register it. */
int vmm_blockdev_add_child(struct vmm_blockdev *bdev, 
			   u64 start_lba, u64 num_blocks);

/** Unregister block device from device driver framework */
int vmm_blockdev_unregister(struct vmm_blockdev *bdev);

/** Find a block device in device driver framework */
struct vmm_blockdev *vmm_blockdev_find(const char *name);

/** Get block device with given number */
struct vmm_blockdev *vmm_blockdev_get(int num);

/** Count number of block devices */
u32 vmm_blockdev_count(void);

#endif /* __VMM_BLOCKDEV_H_ */
