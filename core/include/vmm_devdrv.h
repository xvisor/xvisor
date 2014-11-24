/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_devdrv.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device driver framework header
 */

#ifndef __VMM_DEVDRV_H_
#define __VMM_DEVDRV_H_

#include <vmm_limits.h>
#include <vmm_types.h>
#include <vmm_devtree.h>
#include <vmm_spinlocks.h>
#include <vmm_mutex.h>
#include <vmm_notifier.h>
#include <libs/list.h>
#include <vmm_error.h>

#define VMM_DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

struct vmm_class;
struct vmm_classdev;
struct vmm_bus;
struct vmm_device;
struct vmm_driver;
struct vmm_iommu_ops;
struct vmm_iommu_group;

struct vmm_class {
	/* Private fields (for device driver framework) */
	struct dlist head;
	struct vmm_mutex lock;
	struct dlist device_list;
	/* Public fields */
	char name[VMM_FIELD_NAME_SIZE];
	void (*release) (struct vmm_device *);
};

struct vmm_bus {
	/* Private fields (for device driver framework) */
	struct dlist head;
	struct vmm_mutex lock;
	struct dlist device_list;
	struct dlist driver_list;
	struct vmm_blocking_notifier_chain event_listeners;
	/* Public fields */
	char name[VMM_FIELD_NAME_SIZE];
	struct vmm_iommu_ops *iommu_ops;
	int (*match) (struct vmm_device *dev, struct vmm_driver *drv);
	int (*probe) (struct vmm_device *);
	int (*remove) (struct vmm_device *);
	void (*shutdown) (struct vmm_device *);
};

struct vmm_device_type {
	const char *name;
	void (*release) (struct vmm_device *);
};

struct vmm_device {
	/* Private fields (for device driver framework) */
	struct dlist bus_head;
	struct dlist class_head;
	atomic_t ref_count;
	bool is_registered;
	struct dlist child_head;
	struct vmm_mutex child_list_lock;
	struct dlist child_list;
	vmm_spinlock_t devres_lock;
	struct dlist devres_head;
	struct dlist deferred_head;
	/* Public fields */
	u64 *dma_mask;
	char name[VMM_FIELD_NAME_SIZE];
	struct vmm_bus *bus;
	struct vmm_device_type *type;
	struct vmm_devtree_node *node;
	struct vmm_device *parent;
	struct vmm_class *class;
	struct vmm_driver *driver;
	struct vmm_iommu_group *iommu_group;
	void *pins;
	void (*release) (struct vmm_device *);
	void *priv;
};

struct vmm_driver {
	/* Private fields (for device driver framework) */
	struct dlist head;
	/* Public fields */
	char name[VMM_FIELD_NAME_SIZE];
	struct vmm_bus *bus;
	const struct vmm_devtree_nodeid *match_table;
	int (*probe) (struct vmm_device *, const struct vmm_devtree_nodeid *);
	int (*suspend) (struct vmm_device *, u32);
	int (*resume) (struct vmm_device *);
	int (*remove) (struct vmm_device *);
};

/** Set driver data in device */
static inline void *vmm_devdrv_get_data(struct vmm_device *dev)
{
	return (dev) ? dev->priv : NULL;
}

/** Get driver data from device */
static inline void vmm_devdrv_set_data(struct vmm_device *dev, void *data)
{
	if (dev) {
		dev->priv = data;
	}
}

/** get the dma_mask from device */
static inline u64 vmm_dma_get_mask(struct vmm_device *dev)
{
	if (dev && dev->dma_mask && *dev->dma_mask) {
		return *dev->dma_mask;
	}
	return VMM_DMA_BIT_MASK(32);
}

/** set the dma_mask in device */
static inline int vmm_dma_set_mask(struct vmm_device *dev, u64 mask)
{
	if (!dev->dma_mask) {
		return VMM_EIO;
	}	
	*dev->dma_mask = mask;
	return VMM_OK;
}

/** Bind device pins
 *  Note: The device driver framework only provide dummy weak
 *  implementation of this function which does nothing.
 *  Note: The pinctrl framework will provide complete implementation
 *  of this function. If pinctrl framework is not available then
 *  this function will do nothing.
 */
int vmm_devdrv_pinctrl_bind(struct vmm_device *dev);

/** Probe device instances under a given device tree node */
int vmm_devdrv_probe(struct vmm_devtree_node *node);

/** Register class */
int vmm_devdrv_register_class(struct vmm_class *cls);

/** Unregister class */
int vmm_devdrv_unregister_class(struct vmm_class *cls);

/** Find a registered class */
struct vmm_class *vmm_devdrv_find_class(const char *cname);

/** Get a registered class */
struct vmm_class *vmm_devdrv_class(int index);

/** Count available classes */
u32 vmm_devdrv_class_count(void);

/** Find device of a class using match function */
struct vmm_device *vmm_devdrv_class_find_device(struct vmm_class *cls,
				void *data,
				int (*match) (struct vmm_device *, void *));

/** Find device of a class by name */
struct vmm_device *vmm_devdrv_class_find_device_by_name(
						struct vmm_class *cls,
						const char *dname);

/** Get device of a class */
struct vmm_device *vmm_devdrv_class_device(struct vmm_class *cls, int index);

u32 vmm_devdrv_class_device_count(struct vmm_class *cls);

/** Register bus */
int vmm_devdrv_register_bus(struct vmm_bus *bus);

/** Unregister bus */
int vmm_devdrv_unregister_bus(struct vmm_bus *bus);

/** Find a registered bus */
struct vmm_bus *vmm_devdrv_find_bus(const char *bname);

/** Get a registered bus */
struct vmm_bus *vmm_devdrv_bus(int index);

/** Count available buses */
u32 vmm_devdrv_bus_count(void);

/** Find device on a bus */
struct vmm_device *vmm_devdrv_bus_find_device(struct vmm_bus *bus,
				struct vmm_device *start,
				void *data,
				int (*match) (struct vmm_device *, void *));

/** Find device on a bus by name */
struct vmm_device *vmm_devdrv_bus_find_device_by_name(struct vmm_bus *bus,
						struct vmm_device *start,
						const char *dname);

/** Iterate over each device of a bus */
int vmm_devdrv_bus_for_each_dev(struct vmm_bus *bus,
				struct vmm_device *start,
				void *data,
				int (*fn)(struct vmm_device *dev, void *data));

/** Get device on a bus */
struct vmm_device *vmm_devdrv_bus_device(struct vmm_bus *bus, int index);

/** Count available devices on a bus */
u32 vmm_devdrv_bus_device_count(struct vmm_bus *bus);

/** Register driver on a bus */
int vmm_devdrv_bus_register_driver(struct vmm_bus *bus,
				   struct vmm_driver *drv);

/** Unregister driver on a bus */
int vmm_devdrv_bus_unregister_driver(struct vmm_bus *bus,
				     struct vmm_driver *drv);

/** Find driver for a bus */
struct vmm_driver *vmm_devdrv_bus_find_driver(struct vmm_bus *bus,
					      const char *dname);

/** Get driver for a bus */
struct vmm_driver *vmm_devdrv_bus_driver(struct vmm_bus *bus, int index);

/** Count available device drivers for a bus */
u32 vmm_devdrv_bus_driver_count(struct vmm_bus *bus);

/** Register a client for bus events */
int vmm_devdrv_bus_register_notifier(struct vmm_bus *bus,
				     struct vmm_notifier_block *nb);

/** Unregister a client for bus events */
int vmm_devdrv_bus_unregister_notifier(struct vmm_bus *bus,
					struct vmm_notifier_block *nb);

/* All 4 notifers below get called with the target struct device *
 * as an argument. Note that those functions are likely to be called
 * with the device lock held in the core, so be careful.
 */
#define VMM_BUS_NOTIFY_ADD_DEVICE	0x00000001 /* device added */
#define VMM_BUS_NOTIFY_DEL_DEVICE	0x00000002 /* device removed */
#define VMM_BUS_NOTIFY_BIND_DRIVER	0x00000003 /* driver about to be
						      bound */
#define VMM_BUS_NOTIFY_BOUND_DRIVER	0x00000004 /* driver bound to device */
#define VMM_BUS_NOTIFY_UNBIND_DRIVER	0x00000005 /* driver about to be
						      unbound */
#define VMM_BUS_NOTIFY_UNBOUND_DRIVER	0x00000006 /* driver is unbound
						      from the device */

/** Initialize device */
void vmm_devdrv_initialize_device(struct vmm_device *dev);

/** Increment reference count of device */
void vmm_devdrv_ref_device(struct vmm_device *dev);

/** Decrement reference count of device */
void vmm_devdrv_free_device(struct vmm_device *dev);

/** Check whether device is registered or not */
bool vmm_devdrv_isregistered_device(struct vmm_device *dev);

/** Check whether device is attached to driver or not */
bool vmm_devdrv_isattached_device(struct vmm_device *dev);

/** Register device */
int vmm_devdrv_register_device(struct vmm_device *dev);

/** Force attach device with device driver */
int vmm_devdrv_attach_device(struct vmm_device *dev);

/** Force dettach device with device driver */
int vmm_devdrv_dettach_device(struct vmm_device *dev);

/** Unregister device */
int vmm_devdrv_unregister_device(struct vmm_device *dev);

/** Register device driver */
int vmm_devdrv_register_driver(struct vmm_driver *drv);

/** Force attach device driver */
int vmm_devdrv_attach_driver(struct vmm_driver *drv);

/** Force dettach device driver */
int vmm_devdrv_dettach_driver(struct vmm_driver *drv);

/** Unregister device driver */
int vmm_devdrv_unregister_driver(struct vmm_driver *drv);

/** Initalize device driver framework */
int vmm_devdrv_init(void);

#endif /* __VMM_DEVDRV_H_ */
