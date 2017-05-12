/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file virtio_host.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO host device driver framework.
 *
 * The source has been largely adapted from Linux
 * drivers/virtio/virtio.c
 * drivers/virtio/virtio_ring.c
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <libs/idr.h>
#include <libs/stringlib.h>
#include <drv/virtio_host.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(...)			vmm_printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

#define BAD_RING(vq, fmt, args...)				\
	do {							\
		vmm_lerror(vq->vdev->dev.name, "%s: "fmt,	\
			   (vq)->name, ##args);			\
		(vq)->broken = TRUE;				\
	} while (0)

#define MODULE_DESC			"VirtIO Host Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		VIRTIO_HOST_IPRIORITY
#define	MODULE_INIT			virtio_host_init
#define	MODULE_EXIT			virtio_host_exit

static DEFINE_IDA(virtio_index_ida);

/* ========== VirtIO host queue APIs ========== */

/*
 * Barriers in virtio are tricky.  Non-SMP virtio guests can't assume
 * they're not on an SMP host system, so they need to assume real
 * barriers.  Non-SMP virtio hosts could skip the barriers, but does
 * anyone care?
 *
 * For virtio_pci on SMP, we don't need to order with respect to MMIO
 * accesses through relaxed memory I/O windows, so virt_mb() et al are
 * sufficient.
 *
 * For using virtio to talk to real devices (eg. other heterogeneous
 * CPUs) we do need real barriers.  In theory, we could be using both
 * kinds of virtio, so it's a runtime decision, and the branch is
 * actually quite cheap.
 */

static inline void virtio_mb(bool weak_barriers)
{
	arch_mb();
}

static inline void virtio_rmb(bool weak_barriers)
{
	arch_rmb();
}

static inline void virtio_wmb(bool weak_barriers)
{
	arch_wmb();
}

static inline void virtio_store_mb(bool weak_barriers,
				   u16 *p, u16 v)
{
	*p = v;
	arch_mb();
}

/*
 * Modern virtio devices have feature bits to specify whether they need a
 * quirk and bypass the IOMMU. If not there, just use the DMA API.
 *
 * If there, the interaction between virtio and DMA API is messy.
 *
 * On most systems with virtio, physical addresses match bus addresses,
 * and it doesn't particularly matter whether we use the DMA API.
 *
 * On some systems, including Xen and any system with a physical device
 * that speaks virtio behind a physical IOMMU, we must use the DMA API
 * for virtio DMA to work at all.
 *
 * On other systems, including SPARC and PPC64, virtio-pci devices are
 * enumerated as though they are behind an IOMMU, but the virtio host
 * ignores the IOMMU, so we must either pretend that the IOMMU isn't
 * there or somehow map everything as the identity.
 *
 * For the time being, we preserve historic behavior and bypass the DMA
 * API.
 *
 * TODO: install a per-device DMA ops structure that does the right thing
 * taking into account all the above quirks, and use the DMA API
 * unconditionally on data path.
 */

static bool virtio_use_dma_api(struct virtio_host_device *vdev)
{
	if (!virtio_host_has_iommu_quirk(vdev))
		return TRUE;

	/* Otherwise, we are left to guess. */

	return FALSE;
}

static physical_addr_t virtio_map_one(const struct virtio_host_queue *vq,
				      struct virtio_host_iovec *iv,
				      enum vmm_dma_direction direction)
{
	int rc;
	physical_addr_t pa;

	if (!virtio_use_dma_api(vq->vdev)) {
		rc = vmm_host_va2pa((virtual_addr_t)iv->buf, &pa);
		BUG_ON(rc);
		return pa;
	}

	return vmm_dma_map((virtual_addr_t)iv->buf,
			   (virtual_size_t)iv->buf_len,
			   direction);
}

static void virtio_unmap_one(const struct virtio_host_queue *vq,
			     struct vmm_vring_desc *desc)
{
	u16 flags;

	if (!virtio_use_dma_api(vq->vdev))
		return;

	flags = virtio16_to_cpu(vq->vdev, desc->flags);

	vmm_dma_unmap(virtio64_to_cpu(vq->vdev, desc->addr),
		      virtio32_to_cpu(vq->vdev, desc->len),
		      (flags & VMM_VRING_DESC_F_WRITE) ?
		      DMA_FROM_DEVICE : DMA_TO_DEVICE);
}

void virtio_host_queue_dump_vring(struct virtio_host_queue *vq)
{
	u16 *last_idx;
	struct vmm_device *dev = &vq->vdev->dev;

	vmm_linfo(dev->name, "desc=0x%"PRIPADDR"\n", vq->vring.desc_pa);
	last_idx = (u16 *)&vq->vring.avail->ring[vq->vring.num];
	vmm_linfo(dev->name, "avail=0x%"PRIPADDR" flags=0x%x "
		  "idx=0x%x last_idx=0x%x\n",
		  vq->vring.avail_pa,
		  virtio16_to_cpu(vq->vdev, vq->vring.avail->flags),
		  virtio16_to_cpu(vq->vdev, vq->vring.avail->idx),
		  virtio16_to_cpu(vq->vdev, *last_idx));
	last_idx = (u16 *)&vq->vring.used->ring[vq->vring.num];
	vmm_linfo(dev->name, "used=0x%"PRIPADDR" flags=0x%x "
		  "idx=0x%x last_idx=0x%x\n",
		  vq->vring.used_pa,
		  virtio16_to_cpu(vq->vdev, vq->vring.used->flags),
		  virtio16_to_cpu(vq->vdev, vq->vring.used->idx),
		  virtio16_to_cpu(vq->vdev, *last_idx));
}
VMM_EXPORT_SYMBOL(virtio_host_queue_dump_vring);

static int virtio_host_queue_add(struct virtio_host_queue *vq,
				 struct virtio_host_iovec *ivs[],
				 unsigned int total_ivs,
				 unsigned int out_ivs,
				 unsigned int in_ivs,
				 void *data)
{
	struct virtio_host_iovec *iv;
	struct vmm_vring_desc *desc;
	unsigned int i, n, avail, descs_used, prev = 0;
	int head;
	physical_addr_t addr;

	BUG_ON(data == NULL);

	if (unlikely(vq->broken)) {
		return VMM_EIO;
	}

	BUG_ON(total_ivs > vq->vring.num);
	BUG_ON(total_ivs == 0);

	head = vq->free_head;

	/* TODO: If the host supports indirect descriptor tables,
	 * and we have multiple buffers, then go indirect.
	 * TODO: tune the threshold of using indirect descriptor tables
	 */
	desc = vq->vring.desc;
	i = head;
	descs_used = total_ivs;

	if (vq->num_free < descs_used) {
		DPRINTF("Can't add buf len %i - avail = %i\n",
			descs_used, vq->num_free);
		/* FIXME: for historical reasons, we force a notify here if
		 * there are outgoing parts to the buffer.  Presumably the
		 * host should service the ring ASAP. */
		if (out_ivs)
			vq->notify(vq);
		return VMM_ENOSPC;
	}

	/* Add output buffers to descriptors */
	for (n = 0; n < out_ivs; n++) {
		iv = ivs[n];
		addr = virtio_map_one(vq, iv, DMA_TO_DEVICE);
		desc[i].flags = cpu_to_virtio16(vq->vdev,
						VMM_VRING_DESC_F_NEXT);
		desc[i].addr = cpu_to_virtio64(vq->vdev, addr);
		desc[i].len = cpu_to_virtio32(vq->vdev, iv->buf_len);
		prev = i;
		i = virtio16_to_cpu(vq->vdev, desc[i].next);
	}

	/* Add input buffers to descriptors */
	for (; n < (out_ivs + in_ivs); n++) {
		iv = ivs[n];
		addr = virtio_map_one(vq, iv, DMA_FROM_DEVICE);
		desc[i].flags = cpu_to_virtio16(vq->vdev,
						VMM_VRING_DESC_F_NEXT |
						VMM_VRING_DESC_F_WRITE);
		desc[i].addr = cpu_to_virtio64(vq->vdev, addr);
		desc[i].len = cpu_to_virtio32(vq->vdev, iv->buf_len);
		prev = i;
		i = virtio16_to_cpu(vq->vdev, desc[i].next);
	}

	/* Last one doesn't continue. */
	desc[prev].flags &= cpu_to_virtio16(vq->vdev, ~VMM_VRING_DESC_F_NEXT);

	/* We're using some buffers from the free list. */
	vq->num_free -= descs_used;

	/* Update free pointer */
	vq->free_head = i;

	/* Store token and indirect buffer state. */
	vq->desc_state[head].data = data;

	/* Put entry in available array (but don't update avail->idx until they
	 * do sync). */
	avail = vq->avail_idx_shadow & (vq->vring.num - 1);
	vq->vring.avail->ring[avail] = cpu_to_virtio16(vq->vdev, head);

	/* Descriptors and available array need to be set before we expose the
	 * new available array entries. */
	virtio_wmb(vq->weak_barriers);
	vq->avail_idx_shadow++;
	vq->vring.avail->idx = cpu_to_virtio16(vq->vdev, vq->avail_idx_shadow);
	vq->num_added++;

	DPRINTF("Added buffer head %i to %p\n", head, vq);

	/* This is very unlikely, but theoretically possible.  Kick
	 * just in case. */
	if (unlikely(vq->num_added == (1 << 16) - 1))
		virtio_host_queue_kick(vq);

	return 0;
}

int virtio_host_queue_add_iovecs(struct virtio_host_queue *vq,
				 struct virtio_host_iovec *ivs[],
				 unsigned int out_ivs,
				 unsigned int in_ivs,
				 void *data)
{
	return virtio_host_queue_add(vq, ivs, out_ivs + in_ivs,
				     out_ivs, in_ivs, data);
}
VMM_EXPORT_SYMBOL(virtio_host_queue_add_iovecs);

int virtio_host_queue_add_outbuf(struct virtio_host_queue *vq,
				 struct virtio_host_iovec *iv,
				 unsigned int num,
				 void *data)
{
	return virtio_host_queue_add(vq, &iv, num, 1, 0, data);
}
VMM_EXPORT_SYMBOL(virtio_host_queue_add_outbuf);

int virtio_host_queue_add_inbuf(struct virtio_host_queue *vq,
				struct virtio_host_iovec *iv,
				unsigned int num,
				void *data)
{
	return virtio_host_queue_add(vq, &iv, num, 0, 1, data);
}
VMM_EXPORT_SYMBOL(virtio_host_queue_add_inbuf);

#define virtio_avail_event(vr) (*(u16 *)&(vr)->used->ring[(vr)->num])

bool virtio_host_queue_kick_prepare(struct virtio_host_queue *vq)
{
	u16 new, old;
	bool needs_kick;

	/* We need to expose available array entries before checking avail
	 * event. */
	virtio_mb(vq->weak_barriers);

	old = vq->avail_idx_shadow - vq->num_added;
	new = vq->avail_idx_shadow;
	vq->num_added = 0;

	if (vq->event) {
		needs_kick = vmm_vring_need_event(
			virtio16_to_cpu(vq->vdev, virtio_avail_event(&vq->vring)),
			new, old);
	} else {
		needs_kick = !(vq->vring.used->flags &
			cpu_to_virtio16(vq->vdev, VMM_VRING_USED_F_NO_NOTIFY));
	}

	return needs_kick;
}
VMM_EXPORT_SYMBOL(virtio_host_queue_kick_prepare);

bool virtio_host_queue_notify(struct virtio_host_queue *vq)
{
	if (unlikely(vq->broken))
		return FALSE;

	/* Prod other side to tell it about changes. */
	if (!vq->notify(vq)) {
		vq->broken = TRUE;
		return FALSE;
	}

	return TRUE;
}
VMM_EXPORT_SYMBOL(virtio_host_queue_notify);

bool virtio_host_queue_kick(struct virtio_host_queue *vq)
{
	if (virtio_host_queue_kick_prepare(vq))
		return virtio_host_queue_notify(vq);
	return TRUE;
}
VMM_EXPORT_SYMBOL(virtio_host_queue_kick);

static void detach_buf(struct virtio_host_queue *vq, unsigned int head)
{
	unsigned int i;
	u16 nextflag = cpu_to_virtio16(vq->vdev, VMM_VRING_DESC_F_NEXT);

	/* Clear data ptr. */
	vq->desc_state[head].data = NULL;

	/* Put back on free list: unmap first-level descriptors and find end */
	i = head;

	while (vq->vring.desc[i].flags & nextflag) {
		virtio_unmap_one(vq, &vq->vring.desc[i]);
		i = virtio16_to_cpu(vq->vdev, vq->vring.desc[i].next);
		vq->num_free++;
	}

	virtio_unmap_one(vq, &vq->vring.desc[i]);
	vq->vring.desc[i].next = cpu_to_virtio16(vq->vdev, vq->free_head);
	vq->free_head = head;

	/* Plus final descriptor */
	vq->num_free++;

	/* TODO: Free the indirect table, if any, now that it's unmapped. */
}

static inline bool more_used(const struct virtio_host_queue *vq)
{
	return vq->last_used_idx != virtio16_to_cpu(vq->vdev, vq->vring.used->idx);
}

#define virtio_used_event(vr) ((vr)->avail->ring[(vr)->num])

void *virtio_host_queue_get_buf(struct virtio_host_queue *vq,
				unsigned int *len)
{
	void *ret;
	unsigned int i;
	u16 last_used;

	if (unlikely(vq->broken)) {
		return NULL;
	}

	if (!more_used(vq)) {
		DPRINTF("No more buffers in queue\n");
		return NULL;
	}

	/* Only get used array entries after they have been exposed by host. */
	virtio_rmb(vq->weak_barriers);

	last_used = (vq->last_used_idx & (vq->vring.num - 1));
	i = virtio32_to_cpu(vq->vdev, vq->vring.used->ring[last_used].id);
	*len = virtio32_to_cpu(vq->vdev, vq->vring.used->ring[last_used].len);

	if (unlikely(i >= vq->vring.num)) {
		BAD_RING(vq, "id %u out of range\n", i);
		return NULL;
	}
	if (unlikely(!vq->desc_state[i].data)) {
		BAD_RING(vq, "id %u is not a head!\n", i);
		return NULL;
	}

	/* detach_buf clears data, so grab it now. */
	ret = vq->desc_state[i].data;
	detach_buf(vq, i);
	vq->last_used_idx++;

	/* If we expect an interrupt for the next entry, tell host
	 * by writing event index and flush out the write before
	 * the read in the next get_buf call. */
	if (!(vq->avail_flags_shadow & VMM_VRING_AVAIL_F_NO_INTERRUPT))
		virtio_store_mb(vq->weak_barriers,
				&virtio_used_event(&vq->vring),
				cpu_to_virtio16(vq->vdev, vq->last_used_idx));

	return ret;
}
VMM_EXPORT_SYMBOL(virtio_host_queue_get_buf);

bool virtio_host_queue_have_buf(struct virtio_host_queue *vq)
{
	virtio_mb(vq->weak_barriers);
	return vq->last_used_idx !=
		virtio16_to_cpu(vq->vdev, vq->vring.used->idx);
}
VMM_EXPORT_SYMBOL(virtio_host_queue_have_buf);

vmm_irq_return_t virtio_host_queue_interrupt(int irq, void *_vq)
{
	struct virtio_host_queue *vq = _vq;

	if (!more_used(vq)) {
		DPRINTF("virtio_host_queue interrupt with no work for %p\n",
			vq);
		return VMM_IRQ_NONE;
	}

	if (unlikely(vq->broken))
		return VMM_IRQ_HANDLED;

	DPRINTF("virtio_host_queue callback for %p (%p)\n",
		vq, vq->callback);
	if (vq->callback)
		vq->callback(vq);

	return VMM_IRQ_HANDLED;
}
VMM_EXPORT_SYMBOL(virtio_host_queue_interrupt);

static struct virtio_host_queue *__virtio_host_new_queue(
					unsigned int index,
					struct vmm_vring vring,
					struct virtio_host_device *vdev,
					bool weak_barriers,
					virtio_host_queue_notify_t notify,
					virtio_host_queue_callback_t callback,
					const char *name)
{
	unsigned int i;
	struct virtio_host_queue *vq;

	vq = vmm_zalloc(sizeof(*vq) +
			vring.num * sizeof(struct virtio_host_desc_state));
	if (!vq)
		return NULL;

	vq->index = index;
	INIT_LIST_HEAD(&vq->head);
	strncpy(vq->name, name, sizeof(vq->name));
	vq->vdev = vdev;
	vq->weak_barriers = weak_barriers;
	vq->indirect =
		virtio_host_has_feature(vdev, VMM_VIRTIO_RING_F_INDIRECT_DESC);
	vq->event =
		virtio_host_has_feature(vdev, VMM_VIRTIO_RING_F_EVENT_IDX);
	vq->broken = FALSE;
	vq->free_head = 0;
	vq->num_free = vring.num;
	vq->num_added = 0;
	vq->last_used_idx = 0;
	vq->avail_flags_shadow = 0;
	vq->avail_idx_shadow = 0;
	vq->notify = notify;
	vq->callback = callback;
	vq->vring_size = 0;
	vq->vring_dma_base = 0;
	vq->vring = vring;

	list_add_tail(&vq->head, &vdev->vqs);

	/* No callback?  Tell other side not to bother us. */
	if (!callback) {
		vq->avail_flags_shadow |= VMM_VRING_AVAIL_F_NO_INTERRUPT;
		if (!vq->event) {
			vq->vring.avail->flags =
				cpu_to_virtio16(vdev, vq->avail_flags_shadow);
		}
	}

	/* Put everything in free lists. */
	vq->free_head = 0;
	for (i = 0; i < vring.num-1; i++)
		vq->vring.desc[i].next = cpu_to_virtio16(vdev, i + 1);
	memset(vq->desc_state, 0,
		vring.num * sizeof(struct virtio_host_desc_state));

	return vq;
}

static void *virtio_host_alloc_queue(struct virtio_host_device *vdev,
				     size_t size,
				     physical_addr_t *dma_handle)
{
	if (virtio_use_dma_api(vdev)) {
		return vmm_dma_zalloc_phy(size, dma_handle);
	} else {
		virtual_addr_t queue;

		queue = vmm_host_alloc_pages(VMM_SIZE_TO_PAGE(size),
					     VMM_MEMORY_FLAGS_NORMAL);
		if (queue) {
			int rc;
			physical_addr_t queue_phys_addr;

			memset((void *)queue, 0, size);

			rc = vmm_host_va2pa(queue, &queue_phys_addr);
			if (rc) {
				vmm_host_free_pages((virtual_addr_t)queue,
						    VMM_SIZE_TO_PAGE(size));
				return NULL;
			}

			*dma_handle = queue_phys_addr;
		}

		return (void *)queue;
	}
}

static void virtio_host_free_queue(struct virtio_host_device *vdev,
				   size_t size, void *queue,
				   physical_addr_t dma_handle)
{
	if (virtio_use_dma_api(vdev)) {
		vmm_dma_free(queue);
	} else {
		vmm_host_free_pages((virtual_addr_t)queue,
				    VMM_SIZE_TO_PAGE(size));
	}
}

u64 virtio_host_queue_get_desc_addr(struct virtio_host_queue *vq)
{
	return vq->vring.desc_pa;
}
VMM_EXPORT_SYMBOL(virtio_host_queue_get_desc_addr);

u64 virtio_host_queue_get_used_addr(struct virtio_host_queue *vq)
{
	return vq->vring.used_pa;
}
VMM_EXPORT_SYMBOL(virtio_host_queue_get_used_addr);

u64 virtio_host_queue_get_avail_addr(struct virtio_host_queue *vq)
{
	return vq->vring.avail_pa;
}
VMM_EXPORT_SYMBOL(virtio_host_queue_get_avail_addr);

u32 virtio_host_queue_get_vring_size(struct virtio_host_queue *vq)
{
	return vq->vring.num;
}
VMM_EXPORT_SYMBOL(virtio_host_queue_get_vring_size);

struct virtio_host_queue *virtio_host_create_queue(unsigned int index,
					unsigned int num,
					unsigned int vring_align,
					struct virtio_host_device *vdev,
					bool weak_barriers,
					virtio_host_queue_notify_t notify,
					virtio_host_queue_callback_t callback,
					const char *name)
{
	struct virtio_host_queue *vq;
	void *queue = NULL;
	physical_addr_t dma_addr;
	size_t queue_size_in_bytes;
	struct vmm_vring vring;

	/* We assume num is a power of 2. */
	if (num & (num - 1)) {
		vmm_lerror(vdev->dev.name,
			   "Bad virtio host queue length %u\n", num);
		return NULL;
	}

	/* Try to get a single page. You are my only hope! */
	queue = virtio_host_alloc_queue(vdev,
					vmm_vring_size(num, vring_align),
					&dma_addr);
	if (!queue)
		return NULL;

	queue_size_in_bytes = vmm_vring_size(num, vring_align);
	vmm_vring_init(&vring, num, queue, dma_addr, vring_align);

	vq = __virtio_host_new_queue(index, vring, vdev, weak_barriers,
				     notify, callback, name);
	if (!vq) {
		virtio_host_free_queue(vdev, queue_size_in_bytes,
					queue, dma_addr);
		return NULL;
	}

	vq->vring_size = queue_size_in_bytes;
	vq->vring_dma_base = dma_addr;

	return vq;
}
VMM_EXPORT_SYMBOL(virtio_host_create_queue);

void virtio_host_destroy_queue(struct virtio_host_queue *vq)
{
	virtio_host_free_queue(vq->vdev, vq->vring_size,
			       vq->vring.desc, vq->vring_dma_base);
	list_del(&vq->head);
	vmm_free(vq);
}
VMM_EXPORT_SYMBOL(virtio_host_destroy_queue);

/* ========== VirtIO host device driver APIs ========== */

static void add_status(struct virtio_host_device *vdev, unsigned s)
{
	vdev->config->set_status(vdev, vdev->config->get_status(vdev) | s);
}

/* Manipulates transport-specific feature bits. */
void virtio_host_transport_features(struct virtio_host_device *vdev)
{
	unsigned int i;

	for (i = VMM_VIRTIO_TRANSPORT_F_START; i < VMM_VIRTIO_TRANSPORT_F_END; i++) {
		switch (i) {
		case VMM_VIRTIO_RING_F_INDIRECT_DESC:
			break;
		case VMM_VIRTIO_RING_F_EVENT_IDX:
			break;
		case VMM_VIRTIO_F_VERSION_1:
			break;
		case VMM_VIRTIO_F_IOMMU_PLATFORM:
			break;
		default:
			/* We don't understand this bit. */
			__virtio_host_clear_bit(vdev, i);
		}
	}
}
VMM_EXPORT_SYMBOL(virtio_host_transport_features);

static void __virtio_host_config_changed(struct virtio_host_device *vdev)
{
	struct virtio_host_driver *vdrv =
			to_virtio_host_driver(vdev->dev.driver);

	if (!vdev->config_enabled)
		vdev->config_change_pending = TRUE;
	else if (vdrv && vdrv->config_changed)
		vdrv->config_changed(vdev);
}

void virtio_host_config_changed(struct virtio_host_device *vdev)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&vdev->config_lock, flags);
	__virtio_host_config_changed(vdev);
	vmm_spin_unlock_irqrestore(&vdev->config_lock, flags);
}
VMM_EXPORT_SYMBOL(virtio_host_config_changed);

static void virtio_host_config_disable(struct virtio_host_device *vdev)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&vdev->config_lock, flags);
	vdev->config_enabled = FALSE;
	vmm_spin_unlock_irqrestore(&vdev->config_lock, flags);
}

static void virtio_host_config_enable(struct virtio_host_device *vdev)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&vdev->config_lock, flags);
	vdev->config_enabled = TRUE;
	if (vdev->config_change_pending)
		__virtio_host_config_changed(vdev);
	vdev->config_change_pending = FALSE;
	vmm_spin_unlock_irqrestore(&vdev->config_lock, flags);
}

static int virtio_host_finalize_features(struct virtio_host_device *vdev)
{
	unsigned status;
	int ret = vdev->config->finalize_features(vdev);

	if (ret)
		return ret;

	if (!virtio_host_has_feature(vdev, VMM_VIRTIO_F_VERSION_1))
		return 0;

	add_status(vdev, VMM_VIRTIO_CONFIG_S_FEATURES_OK);
	status = vdev->config->get_status(vdev);
	if (!(status & VMM_VIRTIO_CONFIG_S_FEATURES_OK)) {
		vmm_lerror(vdev->dev.name,
			   "virtio: device refuses features: %x\n",
			   status);
		return VMM_ENODEV;
	}

	return 0;
}

static int virtio_host_match_device(const struct virtio_host_device_id *ids,
				    struct virtio_host_device *vdev)
{
	while (ids->device) {
		if ((ids->device == VMM_VIRTIO_ID_ANY ||
		     ids->device == vdev->id.device) &&
		    (ids->vendor == VMM_VIRTIO_ID_ANY ||
		     ids->vendor == vdev->id.vendor))
			return 1;
		ids++;
	}
	return 0;
}

static int virtio_host_bus_match(struct vmm_device *dev,
				 struct vmm_driver *drv)
{
	struct virtio_host_device *vdev = to_virtio_host_device(dev);
	struct virtio_host_driver *vdrv = to_virtio_host_driver(drv);

	return virtio_host_match_device(vdrv->id_table, vdev);
}

static int virtio_host_driver_probe(struct vmm_device *dev)
{
	int err, i;
	struct virtio_host_device *vdev = to_virtio_host_device(dev);
	struct virtio_host_driver *vdrv = to_virtio_host_driver(dev->driver);
	u64 device_features;
	u64 driver_features;
	u64 driver_features_legacy;

	/* We have a driver! */
	add_status(vdev, VMM_VIRTIO_CONFIG_S_DRIVER);

	/* Figure out what features the device supports. */
	device_features = vdev->config->get_features(vdev);

	/* Figure out what features the driver supports. */
	driver_features = 0;
	for (i = 0; i < vdrv->feature_table_size; i++) {
		unsigned int f = vdrv->feature_table[i];
		BUG_ON(f >= 64);
		driver_features |= (1ULL << f);
	}

	/* Some drivers have a separate feature table for virtio v1.0 */
	if (vdrv->feature_table_legacy) {
		driver_features_legacy = 0;
		for (i = 0; i < vdrv->feature_table_size_legacy; i++) {
			unsigned int f = vdrv->feature_table_legacy[i];
			BUG_ON(f >= 64);
			driver_features_legacy |= (1ULL << f);
		}
	} else {
		driver_features_legacy = driver_features;
	}

	if (device_features & (1ULL << VMM_VIRTIO_F_VERSION_1))
		vdev->features = driver_features & device_features;
	else
		vdev->features = driver_features_legacy & device_features;

	/* Transport features always preserved to pass to finalize_features. */
	for (i = VMM_VIRTIO_TRANSPORT_F_START;
	     i < VMM_VIRTIO_TRANSPORT_F_END; i++)
		if (device_features & (1ULL << i))
			__virtio_host_set_bit(vdev, i);

	err = virtio_host_finalize_features(vdev);
	if (err)
		goto err;

	err = vdrv->probe(vdev);
	if (err)
		goto err;

	/* If probe didn't do it, mark device DRIVER_OK ourselves. */
	if (!(vdev->config->get_status(vdev) & VMM_VIRTIO_CONFIG_S_DRIVER_OK))
		virtio_host_device_ready(vdev);

	if (vdrv->scan)
		vdrv->scan(vdev);

	virtio_host_config_enable(vdev);

	return 0;
err:
	add_status(vdev, VMM_VIRTIO_CONFIG_S_FAILED);
	return err;
}

static int virtio_host_driver_remove(struct vmm_device *dev)
{
	struct virtio_host_device *vdev = to_virtio_host_device(dev);
	struct virtio_host_driver *vdrv = to_virtio_host_driver(dev->driver);

	virtio_host_config_disable(vdev);

	vdrv->remove(vdev);

	/* Driver should have reset device. */
	WARN_ON(vdev->config->get_status(vdev));

	/* Acknowledge the device's existence again. */
	add_status(vdev, VMM_VIRTIO_CONFIG_S_ACKNOWLEDGE);

	return 0;
}

static struct vmm_bus virtio_host_bus = {
	.name		= "virtio_host",
	.match		= virtio_host_bus_match,
	.probe		= virtio_host_driver_probe,
	.remove		= virtio_host_driver_remove,
};

int virtio_host_add_device(struct virtio_host_device *vdev,
			   const struct virtio_host_config_ops *config,
			   struct vmm_device *parent)
{
	int err;

	if (!vdev || !config) {
		return VMM_EINVALID;
	}

	/* Assign a unique device index and hence name. */
	err = ida_simple_get(&virtio_index_ida, 0, 0, 0);
	if (err < 0)
		goto out;

	vdev->index = err;

	vmm_devdrv_initialize_device(&vdev->dev);
	vdev->dev.bus = &virtio_host_bus;
	vdev->dev.parent = parent;
	vmm_snprintf(vdev->dev.name, sizeof(vdev->dev.name),
		     "virtio%u", vdev->index);

	INIT_SPIN_LOCK(&vdev->config_lock);
	vdev->config_enabled = FALSE;
	vdev->config_change_pending = FALSE;
	vdev->config = config;

	/* We always start by resetting the device, in case a previous
	 * driver messed it up.  This also tests that code path a little. */
	vdev->config->reset(vdev);

	/* Acknowledge that we've seen the device. */
	add_status(vdev, VMM_VIRTIO_CONFIG_S_ACKNOWLEDGE);

	INIT_LIST_HEAD(&vdev->vqs);

	err = vmm_devdrv_register_device(&vdev->dev);
out:
	if (err)
		add_status(vdev, VMM_VIRTIO_CONFIG_S_FAILED);
	return err;
}
VMM_EXPORT_SYMBOL(virtio_host_add_device);

void virtio_host_remove_device(struct virtio_host_device *vdev)
{
	int index = vdev->index;

	if (!vdev) {
		return;
	}

	vmm_devdrv_unregister_device(&vdev->dev);
	ida_simple_remove(&virtio_index_ida, index);
}
VMM_EXPORT_SYMBOL(virtio_host_remove_device);

int virtio_host_register_driver(struct virtio_host_driver *vdrv)
{
	if (!vdrv) {
		return VMM_EINVALID;
	}

	vdrv->drv.bus = &virtio_host_bus;
	if (strlcpy(vdrv->drv.name, vdrv->name,
		sizeof(vdrv->drv.name)) >= sizeof(vdrv->drv.name)) {
		return VMM_EOVERFLOW;
	}

	return vmm_devdrv_register_driver(&vdrv->drv);
}
VMM_EXPORT_SYMBOL(virtio_host_register_driver);

void virtio_host_unregister_driver(struct virtio_host_driver *vdrv)
{
	if (!vdrv) {
		return;
	}

	vmm_devdrv_unregister_driver(&vdrv->drv);
}
VMM_EXPORT_SYMBOL(virtio_host_unregister_driver);

static int __init virtio_host_init(void)
{
	return vmm_devdrv_register_bus(&virtio_host_bus);
}

static void __exit virtio_host_exit(void)
{
	vmm_devdrv_unregister_bus(&virtio_host_bus);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
