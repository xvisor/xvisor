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
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_guest_aspace.h>
#include <vmm_modules.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>

#include <emu/virtio.h>

struct vmm_guest *virtio_queue_guest(struct virtio_queue *vq)
{
	return (vq) ? vq->guest : NULL;
}
VMM_EXPORT_SYMBOL(virtio_queue_guest);

u32 virtio_queue_desc_count(struct virtio_queue *vq)
{
	return (vq) ? vq->desc_count : 0;
}
VMM_EXPORT_SYMBOL(virtio_queue_desc_count);

u32 virtio_queue_align(struct virtio_queue *vq)
{
	return (vq) ? vq->align : 0;
}
VMM_EXPORT_SYMBOL(virtio_queue_align);

physical_addr_t virtio_queue_guest_pfn(struct virtio_queue *vq)
{
	return (vq) ? vq->guest_pfn : 0;
}
VMM_EXPORT_SYMBOL(virtio_queue_guest_pfn);

physical_size_t virtio_queue_guest_page_size(struct virtio_queue *vq)
{
	return (vq) ? vq->guest_page_size : 0;
}
VMM_EXPORT_SYMBOL(virtio_queue_guest_page_size);

physical_addr_t virtio_queue_guest_addr(struct virtio_queue *vq)
{
	return (vq) ? vq->guest_addr : 0;
}
VMM_EXPORT_SYMBOL(virtio_queue_guest_addr);

physical_addr_t virtio_queue_host_addr(struct virtio_queue *vq)
{
	return (vq) ? vq->host_addr : 0;
}
VMM_EXPORT_SYMBOL(virtio_queue_host_addr);

physical_size_t virtio_queue_total_size(struct virtio_queue *vq)
{
	return (vq) ? vq->total_size : 0;
}
VMM_EXPORT_SYMBOL(virtio_queue_total_size);

u32 virtio_queue_max_desc(struct virtio_queue *vq)
{
	if (!vq || !vq->guest) {
		return 0;
	}

	return vq->desc_count;
}
VMM_EXPORT_SYMBOL(VMM_EXPORT_SYMBOL);

int virtio_queue_get_desc(struct virtio_queue *vq, u16 indx,
			  struct vring_desc *desc)
{
	u32 ret;
	physical_addr_t desc_pa;

	if (!vq || !vq->guest || !desc) {
		return VMM_EINVALID;
	}

	desc_pa = vq->vring.desc_pa + indx * sizeof(*desc);
	ret = vmm_guest_memory_read(vq->guest, desc_pa,
				    desc, sizeof(*desc), TRUE);
	if (ret != sizeof(*desc)) {
		return VMM_EIO;
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(virtio_queue_get_desc);

u16 virtio_queue_pop(struct virtio_queue *vq)
{
	u16 val;
	u32 ret;
	physical_addr_t avail_pa;

	if (!vq || !vq->guest) {
		return 0;
	}

	ret = umod32(vq->last_avail_idx++, vq->desc_count);

	avail_pa = vq->vring.avail_pa +
		   offsetof(struct vring_avail, ring[ret]);
	ret = vmm_guest_memory_read(vq->guest, avail_pa,
				    &val, sizeof(val), TRUE);
	if (ret != sizeof(val)) {
		vmm_printf("%s: read failed at avail_pa=0x%"PRIPADDR"\n",
			   __func__, avail_pa);
		return 0;
	}

	return val;
}
VMM_EXPORT_SYMBOL(virtio_queue_pop);

bool virtio_queue_available(struct virtio_queue *vq)
{
	u16 val;
	u32 ret;
	physical_addr_t used_pa, avail_pa;

	if (!vq || !vq->guest) {
		return FALSE;
	}

	val = vq->last_avail_idx;
	used_pa = vq->vring.used_pa +
		  offsetof(struct vring_used, ring[vq->vring.num]);
	ret = vmm_guest_memory_write(vq->guest, used_pa,
				     &val, sizeof(val), TRUE);
	if (ret != sizeof(val)) {
		vmm_printf("%s: write failed at used_pa=0x%"PRIPADDR"\n",
			   __func__, used_pa);
		return FALSE;
	}

	avail_pa = vq->vring.avail_pa +
		   offsetof(struct vring_avail, idx);
	ret = vmm_guest_memory_read(vq->guest, avail_pa,
				    &val, sizeof(val), TRUE);
	if (ret != sizeof(val)) {
		vmm_printf("%s: read failed at avail_pa=0x%"PRIPADDR"\n",
			   __func__, avail_pa);
		return FALSE;
	}

	return val != vq->last_avail_idx;
}
VMM_EXPORT_SYMBOL(virtio_queue_available);

bool virtio_queue_should_signal(struct virtio_queue *vq)
{
	u32 ret;
	u16 old_idx, new_idx, event_idx;
	physical_addr_t used_pa, avail_pa;

	if (!vq || !vq->guest) {
		return FALSE;
	}

	old_idx = vq->last_used_signalled;

	used_pa = vq->vring.used_pa +
		  offsetof(struct vring_used, idx);
	ret = vmm_guest_memory_read(vq->guest, used_pa,
				    &new_idx, sizeof(new_idx), TRUE);
	if (ret != sizeof(new_idx)) {
		vmm_printf("%s: read failed at used_pa=0x%"PRIPADDR"\n",
			   __func__, used_pa);
		return FALSE;
	}

	avail_pa = vq->vring.avail_pa +
		   offsetof(struct vring_avail, ring[vq->vring.num]);
	ret = vmm_guest_memory_read(vq->guest, avail_pa,
				    &event_idx, sizeof(event_idx), TRUE);
	if (ret != sizeof(event_idx)) {
		vmm_printf("%s: read failed at avail_pa=0x%"PRIPADDR"\n",
			   __func__, avail_pa);
		return FALSE;
	}

	if (vring_need_event(event_idx, new_idx, old_idx)) {
		vq->last_used_signalled = new_idx;
		return TRUE;
	}

	return FALSE;
}
VMM_EXPORT_SYMBOL(virtio_queue_should_signal);

void virtio_queue_set_used_elem(struct virtio_queue *vq,
				u32 head, u32 len)
{
	u32 ret;
	u16 used_idx;
	struct vring_used_elem used_elem;
	physical_addr_t used_idx_pa, used_elem_pa;

	if (!vq || !vq->guest) {
		return;
	}

	used_idx_pa = vq->vring.used_pa +
		      offsetof(struct vring_used, idx);
	ret = vmm_guest_memory_read(vq->guest, used_idx_pa,
				    &used_idx, sizeof(used_idx), TRUE);
	if (ret != sizeof(used_idx)) {
		vmm_printf("%s: read failed at used_idx_pa=0x%"PRIPADDR"\n",
			   __func__, used_idx_pa);
	}

	used_elem.id = head;
	used_elem.len = len;
	ret = umod32(used_idx, vq->vring.num);
	used_elem_pa = vq->vring.used_pa +
		       offsetof(struct vring_used, ring[ret]);
	ret = vmm_guest_memory_write(vq->guest, used_elem_pa,
				     &used_elem, sizeof(used_elem), TRUE);
	if (ret != sizeof(used_elem)) {
		vmm_printf("%s: write failed at used_elem_pa=0x%"PRIPADDR"\n",
			   __func__, used_elem_pa);
	}

	used_idx++;
	ret = vmm_guest_memory_write(vq->guest, used_idx_pa,
				     &used_idx, sizeof(used_idx), TRUE);
	if (ret != sizeof(used_idx)) {
		vmm_printf("%s: write failed at used_idx_pa=0x%"PRIPADDR"\n",
			   __func__, used_idx_pa);
	}
}
VMM_EXPORT_SYMBOL(virtio_queue_set_used_elem);

bool virtio_queue_setup_done(struct virtio_queue *vq)
{
	return (vq) ? ((vq->guest) ? TRUE : FALSE) : FALSE;
}
VMM_EXPORT_SYMBOL(virtio_queue_setup_done);

int virtio_queue_cleanup(struct virtio_queue *vq)
{
	if (!vq || !vq->guest) {
		goto done;
	}

	vq->last_avail_idx = 0;
	vq->last_used_signalled = 0;

	vq->guest = NULL;

	vq->desc_count = 0;
	vq->align = 0;
	vq->guest_pfn = 0;
	vq->guest_page_size = 0;

	vq->guest_addr = 0;
	vq->host_addr = 0;
	vq->total_size = 0;

done:
	return VMM_OK;
}
VMM_EXPORT_SYMBOL(virtio_queue_cleanup);

int virtio_queue_setup(struct virtio_queue *vq,
			struct vmm_guest *guest,
			physical_addr_t guest_pfn,
			physical_size_t guest_page_size,
			u32 desc_count, u32 align)
{
	int rc = VMM_OK;
	u32 reg_flags;
	physical_addr_t gphys_addr, hphys_addr;
	physical_size_t gphys_size, avail_size;

	if (!vq || !guest) {
		return VMM_EFAIL;
	}

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

	vring_init(&vq->vring, desc_count, gphys_addr, align);

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
 * function returns the next descriptor in the chain, max descriptor count
 * if we're at the end.
 */
static unsigned next_desc(struct virtio_queue *vq,
			  struct vring_desc *desc,
			  u32 i, u32 max)
{
	int rc;
	u32 next;

	if (!(desc->flags & VRING_DESC_F_NEXT)) {
		return max;
	}

	next = desc->next;
	
	rc = virtio_queue_get_desc(vq, next, desc);
	if (rc) {
		vmm_printf("%s: failed to get descriptor next=%d error=%d\n",
			   __func__, next, rc);
		return max;
	}

	return next;
}

u16 virtio_queue_get_head_iovec(struct virtio_queue *vq,
				u16 head, struct virtio_iovec *iov,
				u32 *ret_iov_cnt, u32 *ret_total_len)
{
	int i, rc;
	u16 idx, max;
	struct vring_desc desc;

	if (!vq || !vq->guest || !iov) {
		goto fail;
	}

	idx = head;

	if (ret_iov_cnt) {
		*ret_iov_cnt = 0;
	}
	if (ret_total_len) {
		*ret_total_len = 0;
	}

	max = virtio_queue_max_desc(vq);

	rc = virtio_queue_get_desc(vq, idx, &desc);
	if (rc) {
		vmm_printf("%s: failed to get descriptor idx=%d error=%d\n",
			   __func__, idx, rc);
		goto fail;
	}

	if (desc.flags & VRING_DESC_F_INDIRECT) {
#if 0
		max = desc[idx].len / sizeof(struct vring_desc);
		desc = guest_flat_to_host(kvm, desc[idx].addr);
		idx = 0;
#endif
		vmm_printf("%s: indirect descriptor not supported idx=%d\n",
			   __func__, idx);
		goto fail;
	}

	i = 0;
	do {
		iov[i].addr = desc.addr;
		iov[i].len = desc.len;

		if (ret_total_len) {
			*ret_total_len += desc.len;
		}

		if (desc.flags & VRING_DESC_F_WRITE) {
			iov[i].flags = 1;  /* Write */
		} else {
			iov[i].flags = 0; /* Read */
		}

		i++;
	} while ((idx = next_desc(vq, &desc, idx, max)) != max);

	if (ret_iov_cnt) {
		*ret_iov_cnt = i;
	}

	return head;

fail:
	if (ret_iov_cnt) {
		*ret_iov_cnt = 0;
	}
	if (ret_total_len) {
		*ret_total_len = 0;
	}
	return 0;
}
VMM_EXPORT_SYMBOL(virtio_queue_get_head_iovec);

u16 virtio_queue_get_iovec(struct virtio_queue *vq,
			   struct virtio_iovec *iov,
			   u32 *ret_iov_cnt, u32 *ret_total_len)
{
	u16 head = virtio_queue_pop(vq);

	return virtio_queue_get_head_iovec(vq, head, iov, 
					   ret_iov_cnt, ret_total_len);
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

		len = vmm_guest_memory_read(dev->guest, iov[i].addr,
					    buf + pos, len, TRUE);
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

		len = vmm_guest_memory_write(dev->guest, iov[i].addr,
					     buf + pos, len, TRUE);
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
		len = vmm_guest_memory_write(dev->guest, iov[i].addr + pos,
					     zeros, len, TRUE);
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
