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
 * @file virtio_queue.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO Queue Implementation.
 */
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <arch_barrier.h>

#include <emu/virtio.h>

bool virtio_queue_should_signal(struct virtio_queue *vq)
{
	u16 old_idx, new_idx, event_idx;

	old_idx         = vq->last_used_signalled;
	new_idx         = vq->vring.used->idx;
	event_idx       = vring_used_event(&vq->vring);

	if (vring_need_event(event_idx, new_idx, old_idx)) {
		vq->last_used_signalled = new_idx;
		return true;
	}

	return false;
}
VMM_EXPORT_SYMBOL(virtio_queue_should_signal);

struct vring_used_elem *virtio_queue_set_used_elem(
                                        struct virtio_queue *queue,
                                        u32 head, u32 len)
{
	struct vring_used_elem *used_elem;

	used_elem       = &queue->vring.used->ring[
				umod32(queue->vring.used->idx, queue->vring.num)];
	used_elem->id   = head;
	used_elem->len  = len;

	/*
	 * Use wmb to assure that used elem was updated with head and len.
	 * We need a wmb here since we can't advance idx unless we're ready
	 * to pass the used element to the guest.
	 */
	arch_wmb();
	queue->vring.used->idx++;

	/*
	 * Use wmb to assure used idx has been increased before we signal the guest.
	 * Without a wmb here the guest may ignore the queue since it won't see
	 * an updated idx.
	 */
	arch_wmb();

	return used_elem;
}
VMM_EXPORT_SYMBOL(virtio_queue_set_used_elem);

int virtio_queue_map_vq_space(struct vmm_guest *guest,
			      struct virtio_queue *vq,
			      physical_addr_t gphys_addr,
			      physical_size_t gphys_size)
{
	physical_addr_t hphys_addr;
	physical_size_t avail_size;
	u32             reg_flags;
	int rc = 0;

	if (vq->guest_vq_mapper) {
		//unmap previously mapped pages
	}

	if ((rc = vmm_guest_physical_map(guest, gphys_addr, gphys_size,
					&hphys_addr, &avail_size,
					&reg_flags))) {
		vmm_printf("Failed  vmm_guest_physical_map\n");
		return VMM_EFAIL;
	}

	if (!(reg_flags & VMM_REGION_ISRAM)) {
		//Unmap
		return VMM_EFAIL;
	}

	if (avail_size < gphys_size) {
		//unmap
		return VMM_EFAIL;
	}

	vq->guest_vq_mapper = (void *) vmm_host_memmap(hphys_addr, gphys_size,
			VMM_MEMORY_WRITEABLE | VMM_MEMORY_READABLE);


	if (!vq->guest_vq_mapper) {
		// unmap
		return VMM_EFAIL;
	}

	vq->host_pfn = hphys_addr;
	vq->host_pfn_size = gphys_size;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(virtio_queue_map_vq_space);

int virtio_queue_unmap_vq_space(struct vmm_guest *guest,
                                struct virtio_queue *vq)
{
	// TBD::
	vq->host_pfn  = 0;
	vq->host_pfn_size = 0;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(virtio_queue_unmap_vq_space);


/*
 * Each buffer in the virtqueues is actually a chain of descriptors.  This
 * function returns the next descriptor in the chain, or vq->vring.num if we're
 * at the end.
 */
static unsigned next_desc(struct vring_desc *desc, u32 i, u32 max)
{
	u32 next;

	if (!(desc[i].flags & VRING_DESC_F_NEXT))
		return max;

	next = desc[i].next;

	return next;
}

u16 virtio_queue_get_iovec(struct virtio_queue *vq,
                           struct virtio_iovec *iov,
                           u32 iov_cnt, u32 *ret_iov_cnt,
                           u32 *ret_total_len)
{
	struct vring_desc *desc;
	u16 idx, head, max;
	int i = 0;

	idx = head = virtio_queue_pop(vq);

	*ret_iov_cnt = 0;
	max = vq->vring.num;
	desc = vq->vring.desc;

	if (desc[idx].flags & VRING_DESC_F_INDIRECT) {
#if 0
		max = desc[idx].len / sizeof(struct vring_desc);
		desc = guest_flat_to_host(kvm, desc[idx].addr);
		idx = 0;
#endif
	}

	do {
		iov[i].addr = desc[idx].addr;
		iov[i].len = desc[idx].len;

		*ret_total_len += desc[idx].len;

		if (desc[idx].flags & VRING_DESC_F_WRITE)
			iov[i].flags = 1;  // Write
		else
			iov[i].flags = 0; // Read

		i++;

	} while ((idx = next_desc(desc, idx, max)) != max);

	*ret_iov_cnt = i;

	return head;
}
VMM_EXPORT_SYMBOL(virtio_queue_get_iovec);

u32 virtio_iovec_to_buf_read(struct virtio_device *dev,
                             struct virtio_iovec *iov,
                             u32 iov_cnt, void *buf,
                             u32 buf_len)
{
	u32 i = 0, pos = 0, len = 0;

	for (i = 0; i < iov_cnt && pos < buf_len; i++) {
		len = ((buf_len - pos) < iov[i].len) ?
						(buf_len - pos) : iov[i].len;

		len = vmm_guest_physical_read(dev->guest,
					     iov[i].addr, buf + pos, len);
		if (!len) {
			break;
		}

		pos += len;
	}

	return pos;
}
VMM_EXPORT_SYMBOL(virtio_iovec_to_buf_read);

u32 virtio_buf_to_iovec_write(struct virtio_device *dev,
                              struct virtio_iovec *iov,
                              u32 iov_cnt, void *buf,
                              u32 buf_len)
{
	u32 i = 0, pos = 0, len = 0;

	for (i = 0; i < iov_cnt && pos < buf_len; i++) {
		len = ((buf_len - pos) < iov[i].len) ?
					(buf_len - pos) : iov[i].len;

		len = vmm_guest_physical_write(dev->guest,
					       iov[i].addr, buf + pos, len);
		if (!len) {
			break;
		}

		pos += len;
	}

	return pos;
}
VMM_EXPORT_SYMBOL(virtio_buf_to_iovec_write);

void virtio_iovec_fill_zeros(struct virtio_device *dev,
                             struct virtio_iovec *iov,
                             u32 iov_cnt)
{
	u32 i = 0, pos = 0, len = 0;
	u8 zeros[16];

	memset(zeros, 0, sizeof(zeros));

	while (i < iov_cnt) {
		len = (iov[i].len < 16) ? iov[i].len : 16;
		len = vmm_guest_physical_write(dev->guest, iov[i].addr + pos, zeros, len);
		if (!len) {
			break;
		}

		pos += len;
		if (pos == iov[i].len) {
			pos = 0;
			i++;
		}
	}
}
VMM_EXPORT_SYMBOL(virtio_iovec_fill_zeros);


