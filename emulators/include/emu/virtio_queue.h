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
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#include <emu/virtio_ring.h>

#define VIRTIO_IRQ_LOW          0
#define VIRTIO_IRQ_HIGH         1

#define VIRTIO_PCI_O_CONFIG     0
#define VIRTIO_PCI_O_MSIX       1

struct virtio_device;

struct virtio_queue {
	physical_size_t pfn;
	/* The last_avail_idx field is an index to ->ring of struct vring_avail.
	   It's where we assume the next request index is at.  */
	u16             last_avail_idx;
	u16             last_used_signalled;

	struct vring    vring;

	void		*guest_vq_mapper;
	physical_addr_t	host_pfn;
	physical_size_t	host_pfn_size;
};

static inline u16 virtio_queue_pop(struct virtio_queue *queue)
{
	return queue->vring.avail->ring[
			umod32(queue->last_avail_idx++,	queue->vring.num)];
}

static inline struct vring_desc *virtio_queue_get_desc(
				struct virtio_queue *queue,
				u16 desc_ndx)
{
	return &queue->vring.desc[desc_ndx];
}

static inline bool virtio_queue_available(struct virtio_queue *vq)
{
	if (!vq->vring.avail)
		return 0;

	vring_avail_event(&vq->vring) = vq->last_avail_idx;
	return vq->vring.avail->idx !=  vq->last_avail_idx;
}

bool virtio_queue_should_signal(struct virtio_queue *vq);

struct vring_used_elem *virtio_queue_set_used_elem(
					struct virtio_queue *queue,
					u32 head, u32 len);

int virtio_queue_map_vq_space(struct vmm_guest *guest,
			      struct virtio_queue *vq,
			      physical_addr_t gphys_addr,
			      physical_size_t gphys_size);

int virtio_queue_unmap_vq_space(struct vmm_guest *guest,
				struct virtio_queue *vq);

u16 virtio_queue_get_iovec(struct virtio_queue *vq,
			   struct virtio_iovec *iov,
			   u32 iov_cnt, u32 *ret_iov_cnt,
			   u32 *ret_total_len);

u32 virtio_iovec_to_buf_read(struct virtio_device *dev,
			     struct virtio_iovec *iov,
			     u32 iov_cnt, void *buf,
			     u32 buf_len);

u32 virtio_buf_to_iovec_write(struct virtio_device *dev,
			      struct virtio_iovec *iov,
			      u32 iov_cnt, void *buf,
			      u32 buf_len);

void virtio_iovec_fill_zeros(struct virtio_device *dev,
			     struct virtio_iovec *iov,
			     u32 iov_cnt);

#endif
