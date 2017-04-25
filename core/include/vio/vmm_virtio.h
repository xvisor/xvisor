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
 * @file vmm_virtio.h
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO Core Framework Interface
 */
#ifndef __VMM_VIRTIO_H__
#define __VMM_VIRTIO_H__

#include <vmm_types.h>
#include <vio/vmm_virtio_ring.h>
#include <libs/list.h>

/** VirtIO module intialization priority */
#define VMM_VIRTIO_IPRIORITY				1

#define VMM_VIRTIO_DEVICE_MAX_NAME_LEN			64

enum vmm_virtio_id {
	VMM_VIRTIO_ID_NET		=  1, /* Network card */
	VMM_VIRTIO_ID_BLOCK		=  2, /* Block device */
	VMM_VIRTIO_ID_CONSOLE		=  3, /* Console */
	VMM_VIRTIO_ID_RNG		=  4, /* Entropy source */
	VMM_VIRTIO_ID_BALLOON		=  5, /* Memory ballooning (traditional) */
	VMM_VIRTIO_ID_IO_MEMORY		=  6, /* ioMemory */
	VMM_VIRTIO_ID_RPMSG		=  7, /* rpmsg (remote processor messaging) */
	VMM_VIRTIO_ID_SCSI		=  8, /* SCSI host */
	VMM_VIRTIO_ID_9P		=  9, /* 9P transport */
	VMM_VIRTIO_ID_MAC_VLAN		= 10, /* mac 802.11 Vlan */
	VMM_VIRTIO_ID_RPROC_SERIAL	= 11, /* rproc serial */
	VMM_VIRTIO_ID_CAIF		= 12, /* virtio CAIF */
	VMM_VIRTIO_ID_BALLOON_NEW	= 13, /* New memory ballooning */
	VMM_VIRTIO_ID_GPU		= 16, /* GPU device */
	VMM_VIRTIO_ID_TIMER		= 17, /* Timer/Clock device */
	VMM_VIRTIO_ID_INPUT		= 18, /* Input device */
};

#define VMM_VIRTIO_IRQ_LOW		0
#define VMM_VIRTIO_IRQ_HIGH		1

struct vmm_guest;
struct vmm_virtio_device;

struct vmm_virtio_iovec {
	/* Address (guest-physical). */
	u64 addr;
	/* Length. */
	u32 len;
	/* The flags as indicated above. */
	u16 flags;
};

struct vmm_virtio_queue {
	/* The last_avail_idx field is an index to ->ring of struct vring_avail.
	   It's where we assume the next request index is at.  */
	u16			last_avail_idx;
	u16			last_used_signalled;

	struct vmm_vring	vring;

	struct vmm_guest	*guest;
	u32			desc_count;
	u32			align;
	physical_addr_t		guest_pfn;
	physical_size_t		guest_page_size;
	physical_addr_t		guest_addr;
	physical_addr_t		host_addr;
	physical_size_t		total_size;
};

struct vmm_virtio_device_id {
	u32 type;
};

struct vmm_virtio_device {
	char name[VMM_VIRTIO_DEVICE_MAX_NAME_LEN];
	struct vmm_emudev *edev;

	struct vmm_virtio_device_id id;

	struct vmm_virtio_transport *tra;
	void *tra_data;

	struct vmm_virtio_emulator *emu;
	void *emu_data;

	struct dlist node;
	struct vmm_guest *guest;
};

struct vmm_virtio_transport {
	const char *name;

	int  (*notify)(struct vmm_virtio_device *, u32 vq);
};

struct vmm_virtio_emulator {
	const char *name;
	const struct vmm_virtio_device_id *id_table;

	/* VirtIO operations */
	u32 (*get_host_features) (struct vmm_virtio_device *dev);
	void (*set_guest_features) (struct vmm_virtio_device *dev,
				    u32 features);
	int (*init_vq) (struct vmm_virtio_device *dev, u32 vq, u32 page_size,
			u32 align, u32 pfn);
	int (*get_pfn_vq) (struct vmm_virtio_device *dev, u32 vq);
	int (*get_size_vq) (struct vmm_virtio_device *dev, u32 vq);
	int (*set_size_vq) (struct vmm_virtio_device *dev, u32 vq, int size);
	int (*notify_vq) (struct vmm_virtio_device *dev , u32 vq);

	/* Emulator operations */
	int (*read_config)(struct vmm_virtio_device *dev,
			   u32 offset, void *dst, u32 dst_len);
	int (*write_config)(struct vmm_virtio_device *dev,
			    u32 offset, void *src, u32 src_len);
	int (*reset)(struct vmm_virtio_device *dev);
	int  (*connect)(struct vmm_virtio_device *dev,
			struct vmm_virtio_emulator *emu);
	void (*disconnect)(struct vmm_virtio_device *dev);

	struct dlist node;
};

/** Get guest to which the queue belongs
 *  Note: only available after queue setup is done
 */
struct vmm_guest *vmm_virtio_queue_guest(struct vmm_virtio_queue *vq);

/** Get maximum number of descriptors in queue
 *  Note: only available after queue setup is done
 */
u32 vmm_virtio_queue_desc_count(struct vmm_virtio_queue *vq);

/** Get queue alignment
 *  Note: only available after queue setup is done
 */
u32 vmm_virtio_queue_align(struct vmm_virtio_queue *vq);

/** Get guest page frame number of queue
 *  Note: only available after queue setup is done
 */
physical_addr_t vmm_virtio_queue_guest_pfn(struct vmm_virtio_queue *vq);

/** Get guest page size for this queue
 *  Note: only available after queue setup is done
 */
physical_size_t vmm_virtio_queue_guest_page_size(struct vmm_virtio_queue *vq);

/** Get guest physical address of this queue
 *  Note: only available after queue setup is done
 */
physical_addr_t vmm_virtio_queue_guest_addr(struct vmm_virtio_queue *vq);

/** Get host physical address of this queue
 *  Note: only available after queue setup is done
 */
physical_addr_t vmm_virtio_queue_host_addr(struct vmm_virtio_queue *vq);

/** Get total physical space required by this queue
 *  Note: only available after queue setup is done
 */
physical_size_t virtio_queue_total_size(struct vmm_virtio_queue *vq);

/** Retrive maximum number of vring descriptors
 *  Note: works only after queue setup is done
 */
u32 vmm_virtio_queue_max_desc(struct vmm_virtio_queue *vq);

/** Retrive vring descriptor at given index
 *  Note: works only after queue setup is done
 */
int vmm_virtio_queue_get_desc(struct vmm_virtio_queue *vq, u16 indx,
			      struct vmm_vring_desc *desc);

/** Pop the index of next available descriptor
 *  Note: works only after queue setup is done
 */
u16 vmm_virtio_queue_pop(struct vmm_virtio_queue *vq);

/** Check whether any descriptor is available or not
 *  Note: works only after queue setup is done
 */
bool vmm_virtio_queue_available(struct vmm_virtio_queue *vq);

/** Check whether queue notification is required
 *  Note: works only after queue setup is done
 */
bool vmm_virtio_queue_should_signal(struct vmm_virtio_queue *vq);

/** Update avail_event in vring
 *  Note: works only after queue setup is done
 */
void vmm_virtio_queue_set_avail_event(struct vmm_virtio_queue *vq);

/** Update used element in vring
 *  Note: works only after queue setup is done
 */
void vmm_virtio_queue_set_used_elem(struct vmm_virtio_queue *vq,
				    u32 head, u32 len);

/** Check whether queue setup is done by guest or not */
bool vmm_virtio_queue_setup_done(struct vmm_virtio_queue *vq);

/** Cleanup or reset the queue 
 *  Note: After cleanup we need to setup queue before reusing it.
 */
int vmm_virtio_queue_cleanup(struct vmm_virtio_queue *vq);

/** Setup or initialize the queue 
 *  Note: If queue was already setup then it will cleanup first.
 */
int vmm_virtio_queue_setup(struct vmm_virtio_queue *vq,
			   struct vmm_guest *guest,
			   physical_addr_t guest_pfn,
			   physical_size_t guest_page_size,
			   u32 desc_count, u32 align);

/** Get guest IO vectors based on given head
 *  Note: works only after queue setup is done
 */
u16 vmm_virtio_queue_get_head_iovec(struct vmm_virtio_queue *vq,
				u16 head, struct vmm_virtio_iovec *iov,
				u32 *ret_iov_cnt, u32 *ret_total_len);

/** Get guest IO vectors based on current head
 *  Note: works only after queue setup is done
 */
u16 vmm_virtio_queue_get_iovec(struct vmm_virtio_queue *vq,
			       struct vmm_virtio_iovec *iov,
			       u32 *ret_iov_cnt, u32 *ret_total_len);

/** Read contents from guest IO vectors to a buffer */
u32 vmm_virtio_iovec_to_buf_read(struct vmm_virtio_device *dev,
				 struct vmm_virtio_iovec *iov,
				 u32 iov_cnt, void *buf,
				 u32 buf_len);

/** Write contents to guest IO vectors from a buffer */
u32 vmm_virtio_buf_to_iovec_write(struct vmm_virtio_device *dev,
				  struct vmm_virtio_iovec *iov,
				  u32 iov_cnt, void *buf,
				  u32 buf_len);

/** Fill guest IO vectors with zeros */
void vmm_virtio_iovec_fill_zeros(struct vmm_virtio_device *dev,
				 struct vmm_virtio_iovec *iov,
				 u32 iov_cnt);

/** Read VirtIO device configuration */
int vmm_virtio_config_read(struct vmm_virtio_device *dev,
			   u32 offset, void *dst, u32 dst_len);

/** Write VirtIO device configuration */
int vmm_virtio_config_write(struct vmm_virtio_device *dev,
			    u32 offset, void *src, u32 src_len);

/** Reset VirtIO device */
int vmm_virtio_reset(struct vmm_virtio_device *dev);

/** Register VirtIO device */
int vmm_virtio_register_device(struct vmm_virtio_device *dev);

/** UnRegister VirtIO device */
void vmm_virtio_unregister_device(struct vmm_virtio_device *dev);

/** Register VirtIO device emulator */
int vmm_virtio_register_emulator(struct vmm_virtio_emulator *emu);

/** UnRegister VirtIO device emulator */
void vmm_virtio_unregister_emulator(struct vmm_virtio_emulator *emu);

#endif /* __VMM_VIRTIO_H__ */
