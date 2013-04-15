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
 * @file virtio_queue.h
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO Queue Interface
 */

#ifndef __VIRTIO_QUEUE_H__
#define __VIRTIO_QUEUE_H__

#include <vmm_types.h>
#include <emu/virtio_ring.h>

#define VIRTIO_IRQ_LOW          0
#define VIRTIO_IRQ_HIGH         1

#define VIRTIO_PCI_O_CONFIG     0
#define VIRTIO_PCI_O_MSIX       1

struct vmm_guest;
struct virtio_device;

struct virtio_queue {
	/* The last_avail_idx field is an index to ->ring of struct vring_avail.
	   It's where we assume the next request index is at.  */
	u16			last_avail_idx;
	u16			last_used_signalled;

	struct vring		vring;

	void			*addr;
	struct vmm_guest	*guest;
	u32			desc_count;
	u32			align;
	physical_addr_t		guest_pfn;
	physical_size_t		guest_page_size;
	physical_addr_t		guest_addr;
	physical_addr_t		host_addr;
	physical_size_t		total_size;
};

/** Get mapped base address of queue
 *  Note: only available after queue setup is done
 */
void *virtio_queue_base(struct virtio_queue *vq);

/** Get guest to which the queue belongs
 *  Note: only available after queue setup is done
 */
struct vmm_guest *virtio_queue_guest(struct virtio_queue *vq);

/** Get maximum number of descriptors in queue
 *  Note: only available after queue setup is done
 */
u32 virtio_queue_desc_count(struct virtio_queue *vq);

/** Get queue alignment
 *  Note: only available after queue setup is done
 */
u32 virtio_queue_align(struct virtio_queue *vq);

/** Get guest page frame number of queue
 *  Note: only available after queue setup is done
 */
physical_addr_t virtio_queue_guest_pfn(struct virtio_queue *vq);

/** Get guest page size for this queue
 *  Note: only available after queue setup is done
 */
physical_size_t virtio_queue_guest_page_size(struct virtio_queue *vq);

/** Get guest physical address of this queue
 *  Note: only available after queue setup is done
 */
physical_addr_t virtio_queue_guest_addr(struct virtio_queue *vq);

/** Get host physical address of this queue
 *  Note: only available after queue setup is done
 */
physical_addr_t virtio_queue_host_addr(struct virtio_queue *vq);

/** Get total physical space required by this queue
 *  Note: only available after queue setup is done
 */
physical_size_t virtio_queue_total_size(struct virtio_queue *vq);

/** Pop the index of next available descriptor
 *  Note: works only after queue setup is done
 */
u16 virtio_queue_pop(struct virtio_queue *vq);

/** Retrive vring descriptor at given index
 *  Note: works only after queue setup is done
 */
struct vring_desc *virtio_queue_get_desc(struct virtio_queue *vq, u16 indx);

/** Check whether any descriptor is available or not
 *  Note: works only after queue setup is done
 */
bool virtio_queue_available(struct virtio_queue *vq);

/** Check whether queue notification is required
 *  Note: works only after queue setup is done
 */
bool virtio_queue_should_signal(struct virtio_queue *vq);

/** Update used element in vring
 *  Note: works only after queue setup is done
 */
struct vring_used_elem *virtio_queue_set_used_elem(struct virtio_queue *vq,
						   u32 head, u32 len);

/** Cleanup or reset the queue 
 *  Note: After cleanup we need to setup queue before reusing it.
 */
int virtio_queue_cleanup(struct virtio_queue *vq);

/** Setup or initialize the queue 
 *  Note: If queue was already setup then it will cleanup first.
 */
int virtio_queue_setup(struct virtio_queue *vq,
			struct vmm_guest *guest,
			physical_addr_t guest_pfn,
			physical_size_t guest_page_size,
			u32 desc_count, u32 align);

/** Get guest IO vectors based on given head
 *  Note: works only after queue setup is done
 */
u16 virtio_queue_get_head_iovec(struct virtio_queue *vq,
				u16 head, struct virtio_iovec *iov,
				u32 *ret_iov_cnt, u32 *ret_total_len);

/** Get guest IO vectors based on current head
 *  Note: works only after queue setup is done
 */
u16 virtio_queue_get_iovec(struct virtio_queue *vq,
			   struct virtio_iovec *iov,
			   u32 *ret_iov_cnt, u32 *ret_total_len);

/** Read contents from guest IO vectors to a buffer */
u32 virtio_iovec_to_buf_read(struct virtio_device *dev,
			     struct virtio_iovec *iov,
			     u32 iov_cnt, void *buf,
			     u32 buf_len);

/** Write contents to guest IO vectors from a buffer */
u32 virtio_buf_to_iovec_write(struct virtio_device *dev,
			      struct virtio_iovec *iov,
			      u32 iov_cnt, void *buf,
			      u32 buf_len);

/** Fill guest IO vectors with zeros */
void virtio_iovec_fill_zeros(struct virtio_device *dev,
			     struct virtio_iovec *iov,
			     u32 iov_cnt);

#endif
