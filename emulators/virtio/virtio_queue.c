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
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <vmm_modules.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <arch_barrier.h>

#include <emu/virtio.h>

void *virtio_queue_base(struct virtio_queue *vq)
{
	return vq->addr;
}
VMM_EXPORT_SYMBOL(virtio_queue_base);

struct vmm_guest *virtio_queue_guest(struct virtio_queue *vq)
{
	return vq->guest;
}
VMM_EXPORT_SYMBOL(virtio_queue_guest);

u32 virtio_queue_desc_count(struct virtio_queue *vq)
{
	return vq->desc_count;
}
VMM_EXPORT_SYMBOL(virtio_queue_desc_count);

u32 virtio_queue_align(struct virtio_queue *vq)
{
	return vq->align;
}
VMM_EXPORT_SYMBOL(virtio_queue_align);

physical_addr_t virtio_queue_guest_pfn(struct virtio_queue *vq)
{
	return vq->guest_pfn;
}
VMM_EXPORT_SYMBOL(virtio_queue_guest_pfn);

physical_size_t virtio_queue_guest_page_size(struct virtio_queue *vq)
{
	return vq->guest_page_size;
}
VMM_EXPORT_SYMBOL(virtio_queue_guest_page_size);

physical_addr_t virtio_queue_guest_addr(struct virtio_queue *vq)
{
	return vq->guest_addr;
}
VMM_EXPORT_SYMBOL(virtio_queue_guest_addr);

physical_addr_t virtio_queue_host_addr(struct virtio_queue *vq)
{
	return vq->host_addr;
}
VMM_EXPORT_SYMBOL(virtio_queue_host_addr);

physical_size_t virtio_queue_total_size(struct virtio_queue *vq)
{
	return vq->total_size;
}
VMM_EXPORT_SYMBOL(virtio_queue_total_size);

u16 virtio_queue_pop(struct virtio_queue *vq)
{
	if (!vq->addr) {
		return 0;
	}

	return vq->vring.avail->ring[
			umod32(vq->last_avail_idx++, vq->vring.num)];
}
VMM_EXPORT_SYMBOL(virtio_queue_pop);

struct vring_desc *virtio_queue_get_desc(struct virtio_queue *vq, u16 indx)
{
	if (!vq->addr) {
		return NULL;
	}

	return &vq->vring.desc[indx];
}
VMM_EXPORT_SYMBOL(virtio_queue_get_desc);

bool virtio_queue_available(struct virtio_queue *vq)
{
	if (!vq->addr || !vq->vring.avail) {
		return FALSE;
	}

	vring_avail_event(&vq->vring) = vq->last_avail_idx;

	return vq->vring.avail->idx !=  vq->last_avail_idx;
}
VMM_EXPORT_SYMBOL(virtio_queue_available);

bool virtio_queue_should_signal(struct virtio_queue *vq)
{
	u16 old_idx, new_idx, event_idx;

	if (!vq->addr) {
		return FALSE;
	}

	old_idx         = vq->last_used_signalled;
	new_idx         = vq->vring.used->idx;
	event_idx       = vring_used_event(&vq->vring);

	if (vring_need_event(event_idx, new_idx, old_idx)) {
		vq->last_used_signalled = new_idx;
		return TRUE;
	}

	return FALSE;
}
VMM_EXPORT_SYMBOL(virtio_queue_should_signal);

struct vring_used_elem *virtio_queue_set_used_elem(struct virtio_queue *vq,
						   u32 head, u32 len)
{
	struct vring_used_elem *used_elem;

	if (!vq->addr) {
		return NULL;
	}

	used_elem       = &vq->vring.used->ring[
				umod32(vq->vring.used->idx, vq->vring.num)];
	used_elem->id   = head;
	used_elem->len  = len;

	/*
	 * Use wmb to assure that used elem was updated with head and len.
	 * We need a wmb here since we can't advance idx unless we're ready
	 * to pass the used element to the guest.
	 */
	arch_wmb();
	vq->vring.used->idx++;

	/*
	 * Use wmb to assure used idx has been increased before we signal the guest.
	 * Without a wmb here the guest may ignore the queue since it won't see
	 * an updated idx.
	 */
	arch_wmb();

	return used_elem;
}
VMM_EXPORT_SYMBOL(virtio_queue_set_used_elem);

int virtio_queue_cleanup(struct virtio_queue *vq)
{
	int rc = VMM_OK;

	if (!vq->addr) {
		goto done;
	}

	rc = vmm_host_memunmap((virtual_addr_t)vq->addr, 
			       (virtual_size_t)vq->total_size);

done:
	vq->last_avail_idx = 0;
	vq->last_used_signalled = 0;

	vq->addr = NULL;
	vq->guest = NULL;

	vq->desc_count = 0;
	vq->align = 0;
	vq->guest_pfn = 0;
	vq->guest_page_size = 0;

	vq->guest_addr = 0;
	vq->host_addr = 0;
	vq->total_size = 0;

	return rc;
}
VMM_EXPORT_SYMBOL(virtio_queue_cleanup);

int virtio_queue_setup(struct virtio_queue *vq,
			struct vmm_guest *guest,
			physical_addr_t guest_pfn,
			physical_size_t guest_page_size,
			u32 desc_count, u32 align)
{
	int rc = 0;
	u32 reg_flags;
	physical_addr_t gphys_addr, hphys_addr;
	physical_size_t gphys_size, avail_size;

	if ((rc = virtio_queue_cleanup(vq))) {
		return rc;
	}

	gphys_addr = guest_pfn * guest_page_size;
	gphys_size = vring_size(desc_count, align);

	if ((rc = vmm_guest_physical_map(guest, gphys_addr, gphys_size,
					 &hphys_addr, &avail_size,
					 &reg_flags))) {
		vmm_printf("Failed vmm_guest_physical_map\n");
		return VMM_EFAIL;
	}

	if (!(reg_flags & VMM_REGION_ISRAM)) {
		return VMM_EINVALID;
	}

	if (avail_size < gphys_size) {
		return VMM_EINVALID;
	}

	vq->addr = (void *)vmm_host_memmap(hphys_addr, gphys_size, 
					   VMM_MEMORY_FLAGS_NORMAL);
	if (!vq->addr) {
		return VMM_ENOMEM;
	}

	vring_init(&vq->vring, desc_count, vq->addr, align);

	vq->guest = guest;
	vq->desc_count = desc_count;
	vq->align = align;
	vq->guest_pfn = guest_pfn;
	vq->guest_page_size = guest_page_size;

	vq->guest_addr = gphys_addr;
	vq->host_addr = hphys_addr;
	vq->total_size = gphys_size;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(virtio_queue_setup);

/*
 * Each buffer in the virtqueues is actually a chain of descriptors.  This
 * function returns the next descriptor in the chain, or vq->vring.num if we're
 * at the end.
 */
static unsigned next_desc(struct vring_desc *desc, u32 i, u32 max)
{
	u32 next;

	if (!(desc[i].flags & VRING_DESC_F_NEXT)) {
		return max;
	}

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

	if (!vq->addr) {
		*ret_iov_cnt = 0;
		*ret_total_len = 0;
		return 0;
	}

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

		if (desc[idx].flags & VRING_DESC_F_WRITE) {
			iov[i].flags = 1;  /* Write */
		} else {
			iov[i].flags = 0; /* Read */
		}

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
		len = vmm_guest_physical_write(dev->guest, 
						iov[i].addr + pos, zeros, len);
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

