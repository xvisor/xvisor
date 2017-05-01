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
 * @file virtio_host.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO host device driver framework header.
 *
 * The source has been largely adapted from Linux
 * include/linux/virtio.h
 * include/linux/virtio_config.h
 * include/linux/virtio_byteorder.h
 * include/linux/virtio_ring.h
 *
 * The original code is licensed under the GPL.
 */

#ifndef __VIRTIO_HOST_H_
#define __VIRTIO_HOST_H_

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_devdrv.h>
#include <vio/vmm_virtio_config.h>
#include <vio/vmm_virtio_ids.h>
#include <vio/vmm_virtio_ring.h>
#include <libs/bitops.h>
#include <libs/list.h>

#define VIRTIO_HOST_IPRIORITY		(1)

struct virtio_host_queue;
struct virtio_host_device_id;
struct virtio_host_device;

typedef bool (*virtio_host_queue_notify_t)(struct virtio_host_queue *);
typedef void (*virtio_host_queue_callback_t)(struct virtio_host_queue *);

/** VirtIO host IO vector */
struct virtio_host_iovec {
	void *buf;
	unsigned int buf_len;
};

/** Initialize VirtIO host IO vector */
#define VIRTIO_HOST_INIT_IOVEC(__iovec, __buf, __buf_len) \
do { \
	(__iovec)->buf = (__buf); \
	(__iovec)->buf_len = (__buf_len); \
while (0)

/** VirtIO host descriptor state */
struct virtio_host_desc_state {
	void *data;
};

/** VirtIO host queue */
struct virtio_host_queue {
	unsigned int index;
	struct dlist head;
	const char *name;

	/* Opaque pointer saved by transport driver */
	void *priv;

	/* Parent VirtIO host device */
	struct virtio_host_device *vdev;

	/* Can we use weak barriers? */
	bool weak_barriers;

	/* Host publishes indirect descriptor support */
	bool indirect;

	/* Host publishes avail event idx */
	bool event;

	/* Host is broken */
	bool broken;

	/* Head of free buffer list. */
	unsigned int free_head;

	/* Number of free descriptors */
	unsigned int num_free;

	/* Number we've added since last sync. */
	unsigned int num_added;

	/* Last used index we've seen. */
	u16 last_used_idx;

	/* Last written value to avail->flags */
	u16 avail_flags_shadow;

	/* Last written value to avail->idx in guest byte order */
	u16 avail_idx_shadow;

	/* VirtIO host transport notify */
	virtio_host_queue_notify_t notify;

	/* VirtIO host driver callback */
	virtio_host_queue_callback_t callback;

	/* Location and size of VirtIO ring */
	size_t vring_size;
	physical_addr_t vring_dma_base;

	/* Underlying VirtIO ring */
	struct vmm_vring vring;

	/* VirtIO host descriptor state */
	struct virtio_host_desc_state desc_state[];
};

/**
 * Expose buffers to other end
 *
 * @vq: the struct virtio_host_queue we're talking about.
 * @ivs: virtio IO vectors (must be well-formed and terminated!)
 * @out_num: the number of virtio IO vectors readable by other side
 * @in_num: the number of virtio IO vectors which are writable
 * (after readable ones)
 * @data: the token identifying the buffer.
 *
 * Caller must ensure we don't call this with other virtio_host_queue
 * operations at the same time (except where noted).
 *
 * Returns zero or a negative error.
 */
int virtio_host_queue_add_iovecs(struct virtio_host_queue *vq,
				 struct virtio_host_iovec *ivs[],
				 unsigned int out_ivs,
				 unsigned int in_ivs,
				 void *data);

/**
 * Expose output buffers to other end
 *
 * @vq: the struct virtio_host_queue we're talking about.
 * @iv: virtio IO vectors (must be well-formed and terminated!)
 * @num: the number of entries in @iv readable by other side
 * @data: the token identifying the buffer.
 *
 * Caller must ensure we don't call this with other virtio_host_queue
 * operations at the same time (except where noted).
 *
 * Returns zero or a negative error.
 */
int virtio_host_queue_add_outbuf(struct virtio_host_queue *vq,
				 struct virtio_host_iovec *iv,
				 unsigned int num,
				 void *data);

/**
 * Expose input buffers to other end
 *
 * @vq: the struct virtqueue we're talking about.
 * @iv: virtio IO vectors (must be well-formed and terminated!)
 * @num: the number of entries in @iv writable by other side
 * @data: the token identifying the buffer.
 *
 * Caller must ensure we don't call this with other virtio_host_queue
 * operations at the same time (except where noted).
 *
 * Returns zero or a negative error.
 */
int virtio_host_queue_add_inbuf(struct virtio_host_queue *vq,
				struct virtio_host_iovec *iv,
				unsigned int num,
				void *data);

/**
 * First half of split virtio_host_queue_kick call.
 *
 * @vq: the struct virtqueue
 *
 * Instead of virtio_host_queue_kick(), you can do:
 *	if (virtio_host_queue_kick_prepare(vq))
 *		virtio_host_queue_notify(vq);
 *
 * This is sometimes useful because the virtio_host_queue_kick_prepare()
 * needs to be serialized, but the actual virtio_host_queue_notify()
 * call does not.
 */
bool virtio_host_queue_kick_prepare(struct virtio_host_queue *vq);

/**
 * Second half of split virtio_host_queue_kick call.
 *
 * @vq: the struct virtio_host_queue
 *
 * This does not need to be serialized.
 *
 * Returns FALSE if host notify failed or queue is broken, otherwise TRUE.
 */
bool virtio_host_queue_notify(struct virtio_host_queue *vq);

/**
 * Update after add_buf
 *
 * @vq: the struct virtio_host_queue
 *
 * After one or more virtio_host_queue_add_* calls, invoke this
 * to kick the other side.
 *
 * Caller must ensure we don't call this with other virtio_host_queue
 * operations at the same time (except where noted).
 *
 * Returns FALSE if kick failed, otherwise TRUE.
 */
bool virtio_host_queue_kick(struct virtio_host_queue *vq);

/**
 * Get the next used buffer
 *
 * @vq: the struct virtio_host_queue we're talking about.
 * @len: the length written into the buffer
 *
 * If the driver wrote data into the buffer, @len will be set to the
 * amount written.  This means you don't need to clear the buffer
 * beforehand to ensure there's no data leakage in the case of short
 * writes.
 *
 * Caller must ensure we don't call this with other virtio_host_queue
 * operations at the same time (except where noted).
 *
 * Returns NULL if there are no used buffers, or the "data" token
 * handed to virtio_host_queue_add_*().
 */
void *virtio_host_queue_get_buf(struct virtio_host_queue *vq,
				unsigned int *len);

/**
 * Query pending used buffers
 *
 * @vq: the struct virtio_host_queue we're talking about.
 * @last_used_idx: virtio_host_queue state.
 *
 * Returns "TRUE" if there are pending used buffers in the queue.
 *
 * This does not need to be serialized.
 */
bool virtio_host_queue_poll(struct virtio_host_queue *vq,
			    unsigned last_used_idx);

/** Handle VirtIO host queue interrupt (called by transport drivers) */
vmm_irq_return_t virtio_host_queue_interrupt(int irq, void *_vq);

/** Physical address of descriptor table */
u64 virtio_host_queue_get_desc_addr(struct virtio_host_queue *vq);

/** Physical address of used ring */
u64 virtio_host_queue_get_used_addr(struct virtio_host_queue *vq);

/** Physical address of avail ring */
u64 virtio_host_queue_get_avail_addr(struct virtio_host_queue *vq);

/** Number of descriptors in the VirtIO host queue */
u32 virtio_host_queue_get_vring_size(struct virtio_host_queue *vq);

/** Creates a VirtIO host queue and allocates the descriptor ring. */
struct virtio_host_queue *virtio_host_create_queue(unsigned int index,
					unsigned int num,
					unsigned int vring_align,
					struct virtio_host_device *vdev,
					bool weak_barriers,
					virtio_host_queue_notify_t notify,
					virtio_host_queue_callback_t callback,
					const char *name);

/** Destroys a VirtIO host queue */
void virtio_host_destroy_queue(struct virtio_host_queue *vq);

/** VirtIO host device ID */
struct virtio_host_device_id {
	u32 device;
	u32 vendor;
	void *data;
};

/**
 * Operations for configuring a virtio host device
 *
 * @get: read the value of a configuration field
 *	vdev: the virtio_device
 *	offset: the offset of the configuration field
 *	buf: the buffer to write the field value into.
 *	len: the length of the buffer
 * @set: write the value of a configuration field
 *	vdev: the virtio_device
 *	offset: the offset of the configuration field
 *	buf: the buffer to read the field value from.
 *	len: the length of the buffer
 * @generation: config generation counter
 *	vdev: the virtio_device
 *	Returns the config generation counter
 * @get_status: read the status byte
 *	vdev: the virtio_device
 *	Returns the status byte
 * @set_status: write the status byte
 *	vdev: the virtio_device
 *	status: the new status byte
 * @reset: reset the device
 *	vdev: the virtio device
 *	After this, status and feature negotiation must be done again
 *	Device must not be reset from its vq/config callbacks, or in
 *	parallel with being added/removed.
 * @find_vqs: find virtqueues and instantiate them.
 *	vdev: the virtio_device
 *	nvqs: the number of virtqueues to find
 *	vqs: on success, includes new virtqueues
 *	callbacks: array of callbacks, for each virtqueue
 *		include a NULL entry for vqs that do not need a callback
 *	names: array of virtqueue names (mainly for debugging)
 *		include a NULL entry for vqs unused by driver
 *	Returns 0 on success or error status
 * @del_vqs: free virtqueues found by find_vqs().
 * @get_features: get the array of feature bits for this device.
 *	vdev: the virtio_device
 *	Returns the first 32 feature bits (all we currently need).
 * @finalize_features: confirm what device features we'll be using.
 *	vdev: the virtio_device
 *	This gives the final feature bits for the device: it can change
 *	the dev->feature bits if it wants.
 *	Returns 0 on success or error status
 * @bus_name: return the bus name associated with the device
 *	vdev: the virtio_device
 *      This returns a pointer to the bus name a la pci_name from which
 *      the caller can then copy.
 * @set_vq_affinity: set the affinity for a virtqueue.
 */
struct virtio_host_config_ops {
	void (*get)(struct virtio_host_device *vdev, unsigned offset,
		    void *buf, unsigned len);
	void (*set)(struct virtio_host_device *vdev, unsigned offset,
		    const void *buf, unsigned len);
	u32 (*generation)(struct virtio_host_device *vdev);
	u8 (*get_status)(struct virtio_host_device *vdev);
	void (*set_status)(struct virtio_host_device *vdev, u8 status);
	void (*reset)(struct virtio_host_device *vdev);
	int (*find_vqs)(struct virtio_host_device *vdev, unsigned nvqs,
			struct virtio_host_queue *vqs[],
			virtio_host_queue_callback_t callbacks[],
			const char * const names[]);
	void (*del_vqs)(struct virtio_host_device *vdev);
	u64 (*get_features)(struct virtio_host_device *vdev);
	int (*finalize_features)(struct virtio_host_device *vdev);
	const char *(*bus_name)(struct virtio_host_device *vdev);
	int (*set_vq_affinity)(struct virtio_host_queue *vq, int cpu);
};

struct virtio_host_device {
	int index;
	struct vmm_device dev;

	struct virtio_host_device_id id;

	vmm_spinlock_t config_lock;
	bool config_enabled;
	bool config_change_pending;
	const struct virtio_host_config_ops *config;

	struct dlist vqs;
	u64 features;
	void *priv;
};
#define to_virtio_host_device(d)	\
	container_of(d, struct virtio_host_device, dev)

struct virtio_host_driver {
	struct vmm_driver drv;
	const char *name;
	const struct virtio_host_device_id *id_table;
	const unsigned int *feature_table;
	unsigned int feature_table_size;
	const unsigned int *feature_table_legacy;
	unsigned int feature_table_size_legacy;
	int (*probe)(struct virtio_host_device *vdev);
	void (*scan)(struct virtio_host_device *vdev);
	void (*remove)(struct virtio_host_device *vdev);
	void (*config_changed)(struct virtio_host_device *vdev);
};
#define to_virtio_host_driver(d)	\
	container_of(d, struct virtio_host_driver, drv)

/**
 * Helper to test feature bits. For use by transports.
 * Devices should normally use virtio_host_has_feature,
 * which includes more checks.
 *
 * @vdev: the device
 * @fbit: the feature bit
 */
static inline bool __virtio_host_test_bit(
				const struct virtio_host_device *vdev,
				unsigned int fbit)
{
	/* Did you forget to fix assumptions on max features? */
	BUG_ON(fbit >= 64);

	return vdev->features & BIT_ULL(fbit);
}

/**
 * Helper to set feature bits. For use by transports.
 *
 * @vdev: the device
 * @fbit: the feature bit
 */
static inline void __virtio_host_set_bit(
				struct virtio_host_device *vdev,
				unsigned int fbit)
{
	/* Did you forget to fix assumptions on max features? */
	BUG_ON(fbit >= 64);

	vdev->features |= BIT_ULL(fbit);
}

/**
 * Helper to clear feature bits. For use by transports.
 *
 * @vdev: the device
 * @fbit: the feature bit
 */
static inline void __virtio_host_clear_bit(
				struct virtio_host_device *vdev,
				unsigned int fbit)
{
	/* Did you forget to fix assumptions on max features? */
	BUG_ON(fbit >= 64);

	vdev->features &= ~BIT_ULL(fbit);
}

/**
 * Helper to determine if this device has this feature.
 *
 * @vdev: the device
 * @fbit: the feature bit
 */
static inline bool virtio_host_has_feature(
				const struct virtio_host_device *vdev,
				unsigned int fbit)
{
	return __virtio_host_test_bit(vdev, fbit);
}

/**
 * Determine whether this device has the iommu quirk
 *
 * @vdev: the device
 */
static inline bool virtio_host_has_iommu_quirk(
				const struct virtio_host_device *vdev)
{
	/*
	 * Note the reverse polarity of the quirk feature (compared to most
	 * other features), this is for compatibility with legacy systems.
	 */
	return !virtio_host_has_feature(vdev, VMM_VIRTIO_F_IOMMU_PLATFORM);
}

/**
 * Enable vq use in probe function
 *
 * @vdev: the device
 *
 * Driver must call this to use vqs in the probe function.
 *
 * Note: vqs are enabled automatically after probe returns.
 */
static inline
void virtio_host_device_ready(struct virtio_host_device *vdev)
{
	unsigned status = vdev->config->get_status(vdev);

	BUG_ON(status & VMM_VIRTIO_CONFIG_S_DRIVER_OK);

	vdev->config->set_status(vdev, status | VMM_VIRTIO_CONFIG_S_DRIVER_OK);
}

static inline
const char *virtio_host_bus_name(struct virtio_host_device *vdev)
{
	if (!vdev->config->bus_name)
		return "virtio";
	return vdev->config->bus_name(vdev);
}

/**
 * Setting affinity for a virtqueue
 *
 * @vq: the virtqueue
 * @cpu: the cpu no.
 *
 * Pay attention the function are best-effort: the affinity hint may not be
 * set due to config support, irq type and sharing.
 *
 */
static inline
int virtio_host_queue_set_affinity(struct virtio_host_queue *vq, int cpu)
{
	struct virtio_host_device *vdev = vq->vdev;
	if (vdev->config->set_vq_affinity)
		return vdev->config->set_vq_affinity(vq, cpu);
	return 0;
}

static inline bool virtio_host_legacy_is_little_endian(void)
{
#ifdef CONFIG_CPU_LE
	return TRUE;
#else
	return FALSE;
#endif
}

static inline u16 __virtio16_to_cpu(bool little_endian, u16 val)
{
	if (little_endian)
		return vmm_le16_to_cpu(val);
	else
		return vmm_be16_to_cpu(val);
}

static inline u16 __cpu_to_virtio16(bool little_endian, u16 val)
{
	if (little_endian)
		return vmm_cpu_to_le16(val);
	else
		return vmm_cpu_to_be16(val);
}

static inline u32 __virtio32_to_cpu(bool little_endian, u32 val)
{
	if (little_endian)
		return vmm_le32_to_cpu(val);
	else
		return vmm_be32_to_cpu(val);
}

static inline u32 __cpu_to_virtio32(bool little_endian, u32 val)
{
	if (little_endian)
		return vmm_cpu_to_le32(val);
	else
		return vmm_cpu_to_be32(val);
}

static inline u64 __virtio64_to_cpu(bool little_endian, u64 val)
{
	if (little_endian)
		return vmm_le64_to_cpu(val);
	else
		return vmm_be64_to_cpu(val);
}

static inline u64 __cpu_to_virtio64(bool little_endian, u64 val)
{
	if (little_endian)
		return vmm_cpu_to_le64(val);
	else
		return vmm_cpu_to_be64(val);
}

static inline bool virtio_host_is_little_endian(
					struct virtio_host_device *vdev)
{
	return virtio_host_has_feature(vdev, VMM_VIRTIO_F_VERSION_1) ||
		virtio_host_legacy_is_little_endian();
}

static inline u16 virtio16_to_cpu(struct virtio_host_device *vdev, u16 val)
{
	return __virtio16_to_cpu(virtio_host_is_little_endian(vdev), val);
}

static inline u16 cpu_to_virtio16(struct virtio_host_device *vdev, u16 val)
{
	return __cpu_to_virtio16(virtio_host_is_little_endian(vdev), val);
}

static inline u32 virtio32_to_cpu(struct virtio_host_device *vdev, u32 val)
{
	return __virtio32_to_cpu(virtio_host_is_little_endian(vdev), val);
}

static inline u32 cpu_to_virtio32(struct virtio_host_device *vdev, u32 val)
{
	return __cpu_to_virtio32(virtio_host_is_little_endian(vdev), val);
}

static inline u64 virtio64_to_cpu(struct virtio_host_device *vdev, u64 val)
{
	return __virtio64_to_cpu(virtio_host_is_little_endian(vdev), val);
}

static inline u64 cpu_to_virtio64(struct virtio_host_device *vdev, u64 val)
{
	return __cpu_to_virtio64(virtio_host_is_little_endian(vdev), val);
}

#define virtio_cread(vdev, structname, member, ptr)			\
do {									\
	switch (sizeof(*ptr)) {						\
	case 1:								\
		*(ptr) = virtio_cread8(vdev,				\
				       offsetof(structname, member));	\
		break;							\
	case 2:								\
		*(ptr) = virtio_cread16(vdev,				\
					offsetof(structname, member));	\
		break;							\
	case 4:								\
		*(ptr) = virtio_cread32(vdev,				\
					offsetof(structname, member));	\
		break;							\
	case 8:								\
		*(ptr) = virtio_cread64(vdev,				\
					offsetof(structname, member));	\
		break;							\
	default:							\
		BUG();							\
	}								\
} while(0)

/* Config space accessors. */
#define virtio_cwrite(vdev, structname, member, ptr)			\
do {									\
	switch (sizeof(*ptr)) {						\
	case 1:								\
		virtio_cwrite8(vdev,					\
			       offsetof(structname, member),		\
			       *(ptr));					\
		break;							\
	case 2:								\
		virtio_cwrite16(vdev,					\
				offsetof(structname, member),		\
				*(ptr));				\
		break;							\
	case 4:								\
		virtio_cwrite32(vdev,					\
				offsetof(structname, member),		\
				*(ptr));				\
		break;							\
	case 8:								\
		virtio_cwrite64(vdev,					\
				offsetof(structname, member),		\
				*(ptr));				\
		break;							\
	default:							\
		BUG();							\
	}								\
} while(0)

/* Read @count fields, @bytes each. */
static inline void __virtio_cread_many(struct virtio_host_device *vdev,
				       unsigned int offset,
				       void *buf, size_t count, size_t bytes)
{
	u32 old, gen = vdev->config->generation ?
		vdev->config->generation(vdev) : 0;
	int i;

	do {
		old = gen;

		for (i = 0; i < count; i++)
			vdev->config->get(vdev, offset + bytes * i,
					  buf + i * bytes, bytes);

		gen = vdev->config->generation ?
			vdev->config->generation(vdev) : 0;
	} while (gen != old);
}

static inline void virtio_cread_bytes(struct virtio_host_device *vdev,
				      unsigned int offset,
				      void *buf, size_t len)
{
	__virtio_cread_many(vdev, offset, buf, len, 1);
}

static inline u8 virtio_cread8(struct virtio_host_device *vdev,
				unsigned int offset)
{
	u8 ret;
	vdev->config->get(vdev, offset, &ret, sizeof(ret));
	return ret;
}

static inline void virtio_cwrite8(struct virtio_host_device *vdev,
				  unsigned int offset, u8 val)
{
	vdev->config->set(vdev, offset, &val, sizeof(val));
}

static inline u16 virtio_cread16(struct virtio_host_device *vdev,
				 unsigned int offset)
{
	u16 ret;
	vdev->config->get(vdev, offset, &ret, sizeof(ret));
	return virtio16_to_cpu(vdev, ret);
}

static inline void virtio_cwrite16(struct virtio_host_device *vdev,
				   unsigned int offset, u16 val)
{
	val = cpu_to_virtio16(vdev, val);
	vdev->config->set(vdev, offset, &val, sizeof(val));
}

static inline u32 virtio_cread32(struct virtio_host_device *vdev,
				 unsigned int offset)
{
	u32 ret;
	vdev->config->get(vdev, offset, &ret, sizeof(ret));
	return virtio32_to_cpu(vdev, ret);
}

static inline void virtio_cwrite32(struct virtio_host_device *vdev,
				   unsigned int offset, u32 val)
{
	val = cpu_to_virtio32(vdev, val);
	vdev->config->set(vdev, offset, &val, sizeof(val));
}

static inline u64 virtio_cread64(struct virtio_host_device *vdev,
				 unsigned int offset)
{
	u64 ret;
	__virtio_cread_many(vdev, offset, &ret, 1, sizeof(ret));
	return virtio64_to_cpu(vdev, ret);
}

static inline void virtio_cwrite64(struct virtio_host_device *vdev,
				   unsigned int offset, u64 val)
{
	val = cpu_to_virtio64(vdev, val);
	vdev->config->set(vdev, offset, &val, sizeof(val));
}

static inline int virtio_host_find_vqs(struct virtio_host_device *vdev,
				       unsigned nvqs,
				       struct virtio_host_queue *vqs[],
				       virtio_host_queue_callback_t cbs[],
				       const char * const names[])
{
	return vdev->config->find_vqs(vdev, nvqs, vqs, cbs, names);
}

static inline void virtio_host_del_vqs(struct virtio_host_device *vdev)
{
	vdev->config->del_vqs(vdev);
}

/** Filter out transport-specific feature bits. */
void virtio_host_transport_features(struct virtio_host_device *vdev);

/** Notify VirtIO host driver about change in config */
void virtio_host_config_changed(struct virtio_host_device *vdev);

/** Add a new VirtIO host device */
int virtio_host_add_device(struct virtio_host_device *vdev,
			   const struct virtio_host_config_ops *config,
			   struct vmm_device *parent);

/** Remove a VirtIO host device */
void virtio_host_remove_device(struct virtio_host_device *vdev);

/** Register VirtIO host driver */
int virtio_host_register_driver(struct virtio_host_driver *vdrv);

/** Unregister VirtIO host driver */
void virtio_host_unregister_driver(struct virtio_host_driver *vdrv);

#endif /* __VIRTIO_HOST_H_ */
