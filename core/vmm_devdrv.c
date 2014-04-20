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
 * @file vmm_devdrv.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device driver framework source
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_devres.h>
#include <vmm_devdrv.h>
#include <vmm_mutex.h>
#include <libs/stringlib.h>

struct vmm_devdrv_ctrl {
	struct vmm_mutex class_lock;
	struct dlist class_list;
	struct vmm_mutex bus_lock;
	struct dlist bus_list;

	struct vmm_bus default_bus;
};

static struct vmm_devdrv_ctrl ddctrl;

static int default_bus_match(struct vmm_device *dev, struct vmm_driver *drv)
{
	const struct vmm_devtree_nodeid *match;

	if (!dev || !dev->node || !drv || !drv->match_table) {
		return 0;
	}

	match = vmm_devtree_match_node(drv->match_table, dev->node);
	if (!match) {
		return 0;
	}

	return 1;
}

static int default_bus_probe(struct vmm_device *dev)
{
	struct vmm_driver *drv;
	const struct vmm_devtree_nodeid *match;

	if (!dev || !dev->node || !dev->driver) {
		return VMM_EFAIL;
	}
	drv = dev->driver;

	if (!drv->match_table) {
		return VMM_EFAIL;
	}

	match = vmm_devtree_match_node(drv->match_table, dev->node);
	if (match) {
		return drv->probe(dev, match);
	}

	return VMM_OK;
}

static int default_bus_remove(struct vmm_device *dev)
{
	struct vmm_driver *drv;

	if (!dev || !dev->node || !dev->driver) {
		return VMM_EFAIL;
	}
	drv = dev->driver;

	return drv->remove(dev);
}

static void default_device_release(struct vmm_device *dev)
{
	vmm_free(dev);
}

/* Note: Must be called with bus->lock held */
static int __bus_probe_device_driver(struct vmm_bus *bus,
				     struct vmm_device *dev,
				     struct vmm_driver *drv)
{
	int rc = VMM_OK;

	/* Device should be registered but not having any driver */
	if (!dev->is_registered || dev->driver) {
		/* Note: we return OK so that caller
		 * does not try more drivers
		 */
		return VMM_OK;
	}

	/* Device should match the driver */
	if (bus->match && !bus->match(dev, drv)) {
		return VMM_ENODEV;
	}

	/* If bus probe is available then device should
	 * probe without failure
	 */
	dev->driver = drv;
	if (bus->probe) {
#if defined(CONFIG_VERBOSE_MODE)
		vmm_printf("devdrv: bus=\"%s\" device=\"%s\" "
			   "driver=\"%s\" bus probe.\n",
			   bus->name, dev->name, dev->driver->name);
#endif
		rc = bus->probe(dev);
	} else if (drv->probe) {
#if defined(CONFIG_VERBOSE_MODE)
		vmm_printf("devdrv: bus=\"%s\" device=\"%s\" "
			   "driver=\"%s\" probe.\n",
			   bus->name, dev->name, dev->driver->name);
#endif
		rc = drv->probe(dev, NULL);
	}

	if (rc) {
#if defined(CONFIG_VERBOSE_MODE)
		vmm_printf("devdrv: bus=\"%s\" device=\"%s\" "
			   "probe error %d\n",
			   bus->name, dev->name, rc);
#endif
		dev->driver = NULL;
	}

	return rc;
}

/* Note: Must be called with bus->lock held */
static void __bus_remove_device_driver(struct vmm_bus *bus,
				       struct vmm_device *dev)
{
	int rc = VMM_OK;

	/* Device should be registered and having a driver */
	if (!dev->is_registered || !dev->driver) {
		return;
	}

	if (bus->remove) {
#if defined(CONFIG_VERBOSE_MODE)
		vmm_printf("devdrv: bus=\"%s\" device=\"%s\" "
			   "driver=\"%s\" bus remove.\n",
			   bus->name, dev->name, dev->driver->name);
#endif
		rc = bus->remove(dev);
	} else if (dev->driver->remove) {
#if defined(CONFIG_VERBOSE_MODE)
		vmm_printf("devdrv: bus=\"%s\" device=\"%s\" "
			   "driver=\"%s\" remove.\n",
			   bus->name, dev->name, dev->driver->name);
#endif
		rc = dev->driver->remove(dev);
	}

	if (rc) {
		vmm_printf("devdrv: bus=\"%s\" device=\"%s\" "
			   "remove error %d\n",
			   bus->name, dev->name, rc);
	}

	/* Purge all managed resources */
	rc = vmm_devres_release_all(dev);
	if (rc) {
		vmm_printf("devdrv: bus=\"%s\" device=\"%s\" "
			   "resource remove all error %d\n",
			   bus->name, dev->name, rc);
	}

	dev->driver = NULL;
}

/* Note: Must be called with bus->lock held */
static void __bus_shutdown_device_driver(struct vmm_bus *bus,
					 struct vmm_device *dev)
{
	if (bus->shutdown) {
#if defined(CONFIG_VERBOSE_MODE)
		vmm_printf("devdrv: bus=\"%s\" device=\"%s\" "
			   "shutdown\n",
			   bus->name, dev->name);
#endif
		bus->shutdown(dev);
	}
}

/* Note: Must be called with bus->lock held */
static void __bus_probe_this_device(struct vmm_bus *bus,
				    struct vmm_device *dev)
{
	int rc;
	struct dlist *l;
	struct vmm_driver *drv;

	/* Try each and every driver of this bus */
	list_for_each(l, &bus->driver_list) {
		drv = list_entry(l, struct vmm_driver, head);

		rc = __bus_probe_device_driver(bus, dev, drv);
		if (!rc) {
			break;
		}
	}
}

/* Note: Must be called with bus->lock held */
static void __bus_remove_this_device(struct vmm_bus *bus,
				     struct vmm_device *dev)
{
	__bus_remove_device_driver(bus, dev);
}

/* Note: Must be called with bus->lock held */
static void __bus_shutdown_this_device(struct vmm_bus *bus,
				       struct vmm_device *dev)
{
	__bus_shutdown_device_driver(bus, dev);
}

/* Note: Must be called with bus->lock held */
static void __bus_probe_this_driver(struct vmm_bus *bus,
				    struct vmm_driver *drv)
{
	struct dlist *l;
	struct vmm_device *dev;

	/* Try each and every device of this bus */
	list_for_each(l, &bus->device_list) {
		dev = list_entry(l, struct vmm_device, bus_head);

		/* If already probed then continue */
		if (dev->driver) {
			continue;
		}

		__bus_probe_device_driver(bus, dev, drv);
	}
}

/* Note: Must be called with bus->lock held */
static void __bus_remove_this_driver(struct vmm_bus *bus,
				     struct vmm_driver *drv)
{
	struct dlist *l;
	struct vmm_device *dev;

	/* Try each and every device of this bus */
	list_for_each(l, &bus->device_list) {
		dev = list_entry(l, struct vmm_device, bus_head);

		/* If device not probed with this driver then continue */
		if (dev->driver != drv) {
			continue;
		}

		__bus_remove_device_driver(bus, dev);
	}
}

/* Note: Must be called with bus->lock held */
void __bus_shutdown(struct vmm_bus *bus)
{
	struct dlist *l;
	struct vmm_device *dev;

	/* Forcefully destroy all devices */
	while (!list_empty(&bus->device_list)) {
		l = list_first(&bus->device_list);
		dev = list_entry(l, struct vmm_device, bus_head);

		/* Bus shutdown/cleanup this device */
		__bus_shutdown_this_device(bus, dev);

		/* Unregister from device list */
		list_del(&dev->bus_head);
		dev->is_registered = FALSE;

		/* Decrement reference count of device */
		vmm_devdrv_free_device(dev);
	}
}

/* Note: Must be called with cls->lock held */
void __class_release(struct vmm_class *cls)
{
	struct dlist *l;
	struct vmm_device *dev;

	/* Forcefully destroy all devices */
	while (!list_empty(&cls->device_list)) {
		l = list_first(&cls->device_list);
		dev = list_entry(l, struct vmm_device, class_head);

		/* Unregister from device list */
		list_del(&dev->class_head);
		dev->is_registered = FALSE;

		/* Decrement reference count of device */
		vmm_devdrv_free_device(dev);
	}
}

static int devdrv_probe(struct vmm_devtree_node *node,
			struct vmm_device *parent)
{
	int rc;
	struct dlist *l;
	struct vmm_device *dev;
	struct vmm_devtree_node *child;

	if (!node) {
		return VMM_EFAIL;
	}

	dev = vmm_zalloc(sizeof(struct vmm_device));
	if (!dev) {
		return VMM_ENOMEM;
	}

	vmm_devdrv_initialize_device(dev);

	if (strlcpy(dev->name, node->name, sizeof(dev->name)) >=
	    sizeof(dev->name)) {
		return VMM_EOVERFLOW;
	}
	dev->node = node;
	dev->parent = parent;
	dev->bus = &ddctrl.default_bus;
	dev->release = default_device_release;
	dev->priv = NULL;

	rc = vmm_devdrv_register_device(dev);
	if (rc) {
		vmm_devdrv_free_device(dev);
		return rc;
	}

	list_for_each(l, &node->child_list) {
		child = list_entry(l, struct vmm_devtree_node, head);
		devdrv_probe(child, dev);
	}

	return VMM_OK;
}

int vmm_devdrv_probe(struct vmm_devtree_node *node)
{
	return devdrv_probe(node, NULL);
}

int vmm_devdrv_register_class(struct vmm_class *cls)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;

	if (cls == NULL) {
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each(l, &ddctrl.class_list) {
		c = list_entry(l, struct vmm_class, head);
		if (strcmp(c->name, cls->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&ddctrl.class_lock);
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&cls->head);
	INIT_MUTEX(&cls->lock);
	INIT_LIST_HEAD(&cls->device_list);

	list_add_tail(&cls->head, &ddctrl.class_list);

	vmm_mutex_unlock(&ddctrl.class_lock);

	return VMM_OK;
}

int vmm_devdrv_unregister_class(struct vmm_class *cls)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;

	vmm_mutex_lock(&ddctrl.class_lock);

	if (cls == NULL || list_empty(&ddctrl.class_list)) {
		vmm_mutex_unlock(&ddctrl.class_lock);
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.class_list) {
		c = list_entry(l, struct vmm_class, head);
		if (strcmp(c->name, cls->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&ddctrl.class_lock);
		return VMM_ENOTAVAIL;
	}

	/* Clean release to nuke all devices */
	__class_release(c);

	list_del(&c->head);

	vmm_mutex_unlock(&ddctrl.class_lock);

	return VMM_OK;
}

struct vmm_class *vmm_devdrv_find_class(const char *cname)
{
	bool found;
	struct dlist *l;
	struct vmm_class *cls;

	if (!cname) {
		return NULL;
	}

	found = FALSE;
	cls = NULL;

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each(l, &ddctrl.class_list) {
		cls = list_entry(l, struct vmm_class, head);
		if (strcmp(cls->name, cname) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.class_lock);

	if (!found) {
		return NULL;
	}

	return cls;
}

struct vmm_class *vmm_devdrv_class(int index)
{
	bool found;
	struct dlist *l;
	struct vmm_class *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each(l, &ddctrl.class_list) {
		retval = list_entry(l, struct vmm_class, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&ddctrl.class_lock);

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devdrv_class_count(void)
{
	u32 retval;
	struct dlist *l;

	retval = 0;

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each(l, &ddctrl.class_list) {
		retval++;
	}

	vmm_mutex_unlock(&ddctrl.class_lock);

	return retval;
}

static int devdrv_class_register_device(struct vmm_class *cls,
				        struct vmm_device *dev)
{
	bool found;
	struct dlist *l;
	struct vmm_device *d;

	if (!dev || !cls || (dev->class != cls)) {
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;

	vmm_mutex_lock(&cls->lock);

	list_for_each(l, &cls->device_list) {
		d = list_entry(l, struct vmm_device, class_head);
		if (strcmp(d->name, dev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&cls->lock);
		return VMM_EINVALID;
	}

	if (dev->parent) {
		vmm_devdrv_ref_device(dev->parent);
		vmm_mutex_lock(&dev->parent->child_list_lock);
		list_add_tail(&dev->child_head, &dev->parent->child_list);
		vmm_mutex_unlock(&dev->parent->child_list_lock);
	}

	INIT_LIST_HEAD(&dev->class_head);
	list_add_tail(&dev->class_head, &cls->device_list);
	dev->is_registered = TRUE;

	vmm_mutex_unlock(&cls->lock);

	return VMM_OK;
}

static int devdrv_class_unregister_device(struct vmm_class *cls,
					  struct vmm_device *dev)
{
	bool found;
	struct dlist *l;
	struct vmm_device *d;

	if (!dev || !cls || (dev->class != cls)) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&cls->lock);

	if (list_empty(&cls->device_list)) {
		vmm_mutex_unlock(&cls->lock);
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;
	list_for_each(l, &cls->device_list) {
		d = list_entry(l, struct vmm_device, class_head);
		if (strcmp(d->name, dev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&cls->lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&d->class_head);
	d->is_registered = FALSE;

	vmm_mutex_unlock(&cls->lock);

	return VMM_OK;
}

struct vmm_device *vmm_devdrv_class_find_device(struct vmm_class *cls,
						const char *dname)
{
	bool found;
	struct dlist *l;
	struct vmm_device *dev;

	if (!cls || !dname) {
		return NULL;
	}

	found = FALSE;
	dev = NULL;

	vmm_mutex_lock(&cls->lock);

	list_for_each(l, &cls->device_list) {
		dev = list_entry(l, struct vmm_device, class_head);
		if (strcmp(dev->name, dname) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&cls->lock);

	if (!found) {
		return NULL;
	}

	return dev;
}

struct vmm_device *vmm_devdrv_class_device(struct vmm_class *cls, int index)
{
	bool found;
	struct dlist *l;
	struct vmm_device *retval;

	if (!cls || index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	vmm_mutex_lock(&cls->lock);

	list_for_each(l, &cls->device_list) {
		retval = list_entry(l, struct vmm_device, class_head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&cls->lock);

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devdrv_class_device_count(struct vmm_class *cls)
{
	u32 retval;
	struct dlist *l;

	if (!cls) {
		return 0;
	}

	retval = 0;

	vmm_mutex_lock(&cls->lock);

	list_for_each(l, &cls->device_list) {
		retval++;
	}

	vmm_mutex_unlock(&cls->lock);

	return retval;
}

int vmm_devdrv_register_bus(struct vmm_bus *bus)
{
	bool found;
	struct dlist *l;
	struct vmm_bus *b;

	if (bus == NULL) {
		return VMM_EFAIL;
	}

	b = NULL;
	found = FALSE;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each(l, &ddctrl.bus_list) {
		b = list_entry(l, struct vmm_bus, head);
		if (strcmp(b->name, bus->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&ddctrl.bus_lock);
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&bus->head);
	INIT_MUTEX(&bus->lock);
	INIT_LIST_HEAD(&bus->device_list);
	INIT_LIST_HEAD(&bus->driver_list);

	list_add_tail(&bus->head, &ddctrl.bus_list);

	vmm_mutex_unlock(&ddctrl.bus_lock);

	return VMM_OK;
}

int vmm_devdrv_unregister_bus(struct vmm_bus *bus)
{
	bool found;
	struct dlist *l;
	struct vmm_bus *b;

	vmm_mutex_lock(&ddctrl.bus_lock);

	if (bus == NULL || list_empty(&ddctrl.bus_list)) {
		vmm_mutex_unlock(&ddctrl.bus_lock);
		return VMM_EFAIL;
	}

	b = NULL;
	found = FALSE;
	list_for_each(l, &ddctrl.bus_list) {
		b = list_entry(l, struct vmm_bus, head);
		if (strcmp(b->name, bus->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&ddctrl.bus_lock);
		return VMM_ENOTAVAIL;
	}

	vmm_mutex_lock(&b->lock);

	/* Bus shutdown to nuke all devices */
	__bus_shutdown(b);

	vmm_mutex_unlock(&b->lock);

	list_del(&b->head);

	vmm_mutex_unlock(&ddctrl.bus_lock);

	return VMM_OK;
}

struct vmm_bus *vmm_devdrv_find_bus(const char *bname)
{
	bool found;
	struct dlist *l;
	struct vmm_bus *bus;

	if (!bname) {
		return NULL;
	}

	found = FALSE;
	bus = NULL;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each(l, &ddctrl.bus_list) {
		bus = list_entry(l, struct vmm_bus, head);
		if (strcmp(bus->name, bname) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	if (!found) {
		return NULL;
	}

	return bus;
}

struct vmm_bus *vmm_devdrv_bus(int index)
{
	bool found;
	struct dlist *l;
	struct vmm_bus *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each(l, &ddctrl.bus_list) {
		retval = list_entry(l, struct vmm_bus, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devdrv_bus_count(void)
{
	u32 retval;
	struct dlist *l;

	retval = 0;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each(l, &ddctrl.bus_list) {
		retval++;
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	return retval;
}

static int devdrv_bus_register_device(struct vmm_bus *bus,
				      struct vmm_device *dev)
{
	bool found;
	struct dlist *l;
	struct vmm_device *d;

	if (!dev || !bus || (dev->bus != bus)) {
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;

	vmm_mutex_lock(&bus->lock);

	list_for_each(l, &bus->device_list) {
		d = list_entry(l, struct vmm_device, bus_head);
		if (strcmp(d->name, dev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&bus->lock);
		return VMM_EINVALID;
	}

	if (dev->parent) {
		vmm_devdrv_ref_device(dev->parent);
		vmm_mutex_lock(&dev->parent->child_list_lock);
		list_add_tail(&dev->child_head, &dev->parent->child_list);
		vmm_mutex_unlock(&dev->parent->child_list_lock);
	}

	INIT_LIST_HEAD(&dev->bus_head);
	list_add_tail(&dev->bus_head, &bus->device_list);
	dev->is_registered = TRUE;

	/* Bus probe this device */
	__bus_probe_this_device(bus, dev);

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

static int devdrv_bus_unregister_device(struct vmm_bus *bus,
				        struct vmm_device *dev)
{
	bool found;
	struct dlist *l;
	struct vmm_device *d;

	if (!dev || !bus || (dev->bus != bus)) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&bus->lock);

	if (list_empty(&bus->device_list)) {
		vmm_mutex_unlock(&bus->lock);
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;
	list_for_each(l, &bus->device_list) {
		d = list_entry(l, struct vmm_device, bus_head);
		if (strcmp(d->name, dev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&bus->lock);
		return VMM_ENOTAVAIL;
	}

	/* Bus remove this device */
	__bus_remove_this_device(bus, d);

	list_del(&d->bus_head);
	d->is_registered = FALSE;

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

struct vmm_device *vmm_devdrv_bus_find_device(struct vmm_bus *bus,
			void *data, int (*match) (struct vmm_device *, void *))
{
	bool found;
	struct dlist *l;
	struct vmm_device *dev;

	if (!bus || !match) {
		return NULL;
	}

	found = FALSE;
	dev = NULL;

	vmm_mutex_lock(&bus->lock);

	list_for_each(l, &bus->device_list) {
		dev = list_entry(l, struct vmm_device, bus_head);
		if (match(dev, data)) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&bus->lock);

	if (!found) {
		return NULL;
	}

	return dev;
}

struct vmm_device *vmm_devdrv_bus_find_device_by_name(struct vmm_bus *bus,
					              const char *dname)
{
	bool found;
	struct dlist *l;
	struct vmm_device *dev;

	if (!bus || !dname) {
		return NULL;
	}

	found = FALSE;
	dev = NULL;

	vmm_mutex_lock(&bus->lock);

	list_for_each(l, &bus->device_list) {
		dev = list_entry(l, struct vmm_device, bus_head);
		if (strcmp(dev->name, dname) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&bus->lock);

	if (!found) {
		return NULL;
	}

	return dev;
}

struct vmm_device *vmm_devdrv_bus_device(struct vmm_bus *bus, int index)
{
	bool found;
	struct dlist *l;
	struct vmm_device *retval;

	if (!bus || index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	vmm_mutex_lock(&bus->lock);

	list_for_each(l, &bus->device_list) {
		retval = list_entry(l, struct vmm_device, bus_head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&bus->lock);

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devdrv_bus_device_count(struct vmm_bus *bus)
{
	u32 retval;
	struct dlist *l;

	if (!bus) {
		return 0;
	}

	retval = 0;

	vmm_mutex_lock(&bus->lock);

	list_for_each(l, &bus->device_list) {
		retval++;
	}

	vmm_mutex_unlock(&bus->lock);

	return retval;
}

int vmm_devdrv_bus_register_driver(struct vmm_bus *bus,
				   struct vmm_driver *drv)
{
	bool found;
	struct dlist *l;
	struct vmm_driver *d;

	if (!drv || !bus || (drv->bus != bus)) {
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;

	vmm_mutex_lock(&bus->lock);

	list_for_each(l, &bus->driver_list) {
		d = list_entry(l, struct vmm_driver, head);
		if (strcmp(d->name, drv->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&bus->lock);
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&drv->head);
	list_add_tail(&drv->head, &bus->driver_list);

	/* Bus probe this driver */
	__bus_probe_this_driver(bus, drv);

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

int vmm_devdrv_bus_unregister_driver(struct vmm_bus *bus,
				     struct vmm_driver *drv)
{
	bool found;
	struct dlist *l;
	struct vmm_driver *d;

	if (!drv || !bus || (drv->bus != bus)) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&bus->lock);

	if (list_empty(&bus->driver_list)) {
		vmm_mutex_unlock(&bus->lock);
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;
	list_for_each(l, &bus->driver_list) {
		d = list_entry(l, struct vmm_driver, head);
		if (strcmp(d->name, drv->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&bus->lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&d->head);

	/* Bus remove this driver */
	__bus_remove_this_driver(bus, d);

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

struct vmm_driver *vmm_devdrv_bus_find_driver(struct vmm_bus *bus,
					      const char *dname)
{
	bool found;
	struct dlist *l;
	struct vmm_driver *drv;

	if (!bus || !dname) {
		return NULL;
	}

	found = FALSE;
	drv = NULL;

	vmm_mutex_lock(&bus->lock);

	list_for_each(l, &bus->driver_list) {
		drv = list_entry(l, struct vmm_driver, head);
		if (strcmp(drv->name, dname) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&bus->lock);

	if (!found) {
		return NULL;
	}

	return drv;
}

struct vmm_driver *vmm_devdrv_bus_driver(struct vmm_bus *bus, int index)
{
	bool found;
	struct dlist *l;
	struct vmm_driver *retval;

	if (!bus || index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	vmm_mutex_lock(&bus->lock);

	list_for_each(l, &bus->driver_list) {
		retval = list_entry(l, struct vmm_driver, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&bus->lock);

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devdrv_bus_driver_count(struct vmm_bus *bus)
{
	u32 retval;
	struct dlist *l;

	if (!bus) {
		return 0;
	}

	retval = 0;

	vmm_mutex_lock(&bus->lock);

	list_for_each(l, &bus->driver_list) {
		retval++;
	}

	vmm_mutex_unlock(&bus->lock);

	return retval;
}

void vmm_devdrv_initialize_device(struct vmm_device *dev)
{
	if (!dev) {
		return;
	}

	INIT_LIST_HEAD(&dev->bus_head);
	INIT_LIST_HEAD(&dev->child_head);
	arch_atomic_write(&dev->ref_count, 1);
	INIT_LIST_HEAD(&dev->child_head);
	INIT_MUTEX(&dev->child_list_lock);
	INIT_LIST_HEAD(&dev->child_list);
	INIT_SPIN_LOCK(&dev->devres_lock);
	INIT_LIST_HEAD(&dev->devres_head);
}

void vmm_devdrv_ref_device(struct vmm_device *dev)
{
	if (!dev) {
		return;
	}

	arch_atomic_inc(&dev->ref_count);
}

void vmm_devdrv_free_device(struct vmm_device *dev)
{
	bool released;

	if (!dev) {
		return;
	}

	if (arch_atomic_sub_return(&dev->ref_count, 1)) {
		return;
	}

	/* Update parent */
	if (dev->parent) {
		vmm_mutex_lock(&dev->parent->child_list_lock);
		list_del(&dev->child_head);
		vmm_mutex_unlock(&dev->parent->child_list_lock);
		vmm_devdrv_free_device(dev->parent);
	}

	released = TRUE;
	if (dev->release) {
		dev->release(dev);
	} else if (dev->type && dev->type->release) {
		dev->type->release(dev);
	} else if (dev->class && dev->class->release) {
		dev->class->release(dev);
	} else {
		released = FALSE;
	}

	WARN_ON(!released);
}

bool vmm_devdrv_isregistered_device(struct vmm_device *dev)
{
	return (dev) ? dev->is_registered : FALSE;
}

bool vmm_devdrv_isattached_device(struct vmm_device *dev)
{
	return (dev) ? ((dev->driver) ? TRUE : FALSE) : FALSE;
}

int vmm_devdrv_register_device(struct vmm_device *dev)
{
	if (dev && dev->bus && !dev->class) {
		return devdrv_bus_register_device(dev->bus, dev);
	} else if (dev && !dev->bus && dev->class) {
		return devdrv_class_register_device(dev->class, dev);
	}

	return VMM_EFAIL;
}

int vmm_devdrv_attach_device(struct vmm_device *dev)
{
	struct vmm_bus *bus;

	/* Device should be registered with a valid bus */
	if (!dev || !dev->is_registered || !dev->bus) {
		return VMM_EFAIL;
	}
	bus = dev->bus;

	vmm_mutex_lock(&bus->lock);

	/* Bus probe this device */
	__bus_probe_this_device(bus, dev);

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

int vmm_devdrv_dettach_device(struct vmm_device *dev)
{
	struct vmm_bus *bus;
	
	/* Device should be registered with a valid bus */
	if (!dev || !dev->is_registered || !dev->bus) {
		return VMM_EFAIL;
	}
	bus = dev->bus;

	vmm_mutex_lock(&bus->lock);

	/* Bus remove this device */
	__bus_remove_this_device(bus, dev);

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

int vmm_devdrv_unregister_device(struct vmm_device *dev)
{
	if (dev && dev->bus && !dev->class) {
		return devdrv_bus_unregister_device(dev->bus, dev);
	} else if (dev && !dev->bus && dev->class) {
		return devdrv_class_unregister_device(dev->class, dev);
	}

	return VMM_EFAIL;
}

int vmm_devdrv_register_driver(struct vmm_driver *drv)
{
	if (!drv) {
		return VMM_EFAIL;
	}

	if (!drv->bus) {
		drv->bus = &ddctrl.default_bus;
	}

	return vmm_devdrv_bus_register_driver(drv->bus, drv);
}

int vmm_devdrv_attach_driver(struct vmm_driver *drv)
{
	struct vmm_bus *bus;

	if (!drv || !drv->bus) {
		return VMM_EFAIL;
	}
	bus = drv->bus;

	vmm_mutex_lock(&bus->lock);

	/* Bus probe this driver */
	__bus_probe_this_driver(bus, drv);

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

int vmm_devdrv_dettach_driver(struct vmm_driver *drv)
{
	struct vmm_bus *bus;

	if (!drv || !drv->bus) {
		return VMM_EFAIL;
	}
	bus = drv->bus;

	vmm_mutex_lock(&bus->lock);

	/* Bus remove this driver */
	__bus_remove_this_driver(bus, drv);

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

int vmm_devdrv_unregister_driver(struct vmm_driver *drv)
{
	if (!drv || !drv->bus) {
		return VMM_EFAIL;
	}

	return vmm_devdrv_bus_unregister_driver(drv->bus, drv);
}

int __init vmm_devdrv_init(void)
{
	memset(&ddctrl, 0, sizeof(ddctrl));

	INIT_MUTEX(&ddctrl.class_lock);
	INIT_LIST_HEAD(&ddctrl.class_list);
	INIT_MUTEX(&ddctrl.bus_lock);
	INIT_LIST_HEAD(&ddctrl.bus_list);

	strcpy(ddctrl.default_bus.name, "default");
	ddctrl.default_bus.match = default_bus_match;
	ddctrl.default_bus.probe = default_bus_probe;
	ddctrl.default_bus.remove = default_bus_remove;

	return vmm_devdrv_register_bus(&ddctrl.default_bus);
}
