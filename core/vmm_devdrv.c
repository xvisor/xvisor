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

/* Note: Must be called with bus->lock held */
static int __bus_probe_device_driver(struct vmm_bus *bus,
				     struct vmm_device *dev,
				     struct vmm_driver *drv)
{
	int rc = VMM_OK;

	if (bus->match && !bus->match(dev, drv)) {
		return VMM_ENODEV;
	}

	if (!bus->probe) {
		dev->driver = drv;
	} else {
#if defined(CONFIG_VERBOSE_MODE)
		vmm_printf("Probe device %s\n", dev->name);
#endif
		dev->driver = drv;
		rc = bus->probe(dev);
		if (rc) {
			vmm_printf("%s: %s probe error %d\n", 
				   __func__, dev->name, rc);
			dev->driver = NULL;
		}
	}

	return rc;
}

/* Note: Must be called with bus->lock held */
static int __bus_remove_device_driver(struct vmm_bus *bus,
				      struct vmm_device *dev)
{
	int rc = VMM_OK;

	if (bus->remove) {
#if defined(CONFIG_VERBOSE_MODE)
		vmm_printf("Remove device %s\n", dev->name);
#endif
		rc = bus->remove(dev);
		dev->driver = NULL;
		if (rc) {
			vmm_printf("%s: %s remove error %d\n", 
				   __func__, dev->name, rc);
		}
	} else {
		dev->driver = NULL;
	}

	return rc;
}

/* Note: Must be called with bus->lock held */
static void __bus_probe_this_device(struct vmm_bus *bus,
				    struct vmm_device *dev)
{
	int rc;
	struct dlist *l;
	struct vmm_driver *drv;

	/* If device already probed then do nothing */
	if (dev->driver) {
		return;
	}

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
	/* If device not probed then do nothing */
	if (!dev->driver) {
		return;
	}

	__bus_remove_device_driver(bus, dev);
}

/* Note: Must be called with bus->lock held */
static void __bus_probe_this_driver(struct vmm_bus *bus,
				    struct vmm_driver *drv)
{
	struct dlist *l;
	struct vmm_device *dev;

	/* Try each and every device of this bus */
	list_for_each(l, &bus->device_list) {
		dev = list_entry(l, struct vmm_device, head);

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
		dev = list_entry(l, struct vmm_device, head);

		/* If device not probed with this driver then continue */
		if (dev->driver != drv) {
			continue;
		}

		__bus_remove_device_driver(bus, dev);
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
	strncpy(dev->name, node->name, sizeof(dev->name));
	dev->node = node;
	dev->parent = parent;
	dev->priv = NULL;

	rc = vmm_devdrv_register_device(dev);
	if (rc) {
		vmm_free(dev);
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
	INIT_LIST_HEAD(&cls->classdev_list);

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

int vmm_devdrv_register_classdev(const char *cname, struct vmm_classdev *cdev)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;
	struct vmm_classdev *cd;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL || cdev == NULL) {
		return VMM_EFAIL;
	}

	cd = NULL;
	found = FALSE;

	vmm_mutex_lock(&c->lock);

	list_for_each(l, &c->classdev_list) {
		cd = list_entry(l, struct vmm_classdev, head);
		if (strcmp(cd->name, cdev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&c->lock);
		return VMM_EINVALID;
	}

	if (cdev->dev) {
		cdev->dev->class = c;
		cdev->dev->classdev = cdev;
	}
	INIT_LIST_HEAD(&cdev->head);

	list_add_tail(&cdev->head, &c->classdev_list);

	vmm_mutex_unlock(&c->lock);

	return VMM_OK;
}

int vmm_devdrv_unregister_classdev(const char *cname, struct vmm_classdev *cdev)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;
	struct vmm_classdev *cd;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL || cdev == NULL) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&c->lock);

	if (list_empty(&c->classdev_list)) {
		vmm_mutex_unlock(&c->lock);
		return VMM_EFAIL;
	}

	cd = NULL;
	found = FALSE;
	list_for_each(l, &c->classdev_list) {
		cd = list_entry(l, struct vmm_classdev, head);
		if (strcmp(cd->name, cdev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&c->lock);
		return VMM_ENOTAVAIL;
	}

	if (cd->dev) {
		cd->dev->class = NULL;
		cd->dev->classdev = NULL;
	}

	list_del(&cd->head);

	vmm_mutex_unlock(&c->lock);

	return VMM_OK;
}

struct vmm_classdev *vmm_devdrv_find_classdev(const char *cname,
					 const char *cdev_name)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;
	struct vmm_classdev *cd;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL || cdev_name == NULL) {
		return NULL;
	}

	found = FALSE;
	cd = NULL;

	vmm_mutex_lock(&c->lock);

	list_for_each(l, &c->classdev_list) {
		cd = list_entry(l, struct vmm_classdev, head);
		if (strcmp(cd->name, cdev_name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&c->lock);

	if (!found) {
		return NULL;
	}

	return cd;
}

struct vmm_classdev *vmm_devdrv_classdev(const char *cname, int index)
{
	bool found;
	struct dlist *l;
	struct vmm_class *c;
	struct vmm_classdev *retval;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL || index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	vmm_mutex_lock(&c->lock);

	list_for_each(l, &c->classdev_list) {
		retval = list_entry(l, struct vmm_classdev, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&c->lock);

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devdrv_classdev_count(const char *cname)
{
	u32 retval;
	struct dlist *l;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(cname);

	if (c == NULL) {
		return VMM_EFAIL;
	}

	retval = 0;

	vmm_mutex_lock(&c->lock);

	list_for_each(l, &c->classdev_list) {
		retval++;
	}

	vmm_mutex_unlock(&c->lock);

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
	struct vmm_device *d;

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

	while (!list_empty(&bus->device_list)) {
		l = list_pop(&bus->device_list);
		d = list_entry(l, struct vmm_device, head);

		/* Bus remove this device */
		__bus_remove_this_device(b, d);
	}

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

int vmm_devdrv_register_device(struct vmm_device *dev)
{
	bool found;
	struct dlist *l;
	struct vmm_bus *bus;
	struct vmm_device *d;

	if (!dev) {
		return VMM_EFAIL;
	}
	if (!dev->bus) {
		dev->bus = &ddctrl.default_bus;
	}
	bus = dev->bus;

	d = NULL;
	found = FALSE;

	vmm_mutex_lock(&bus->lock);

	list_for_each(l, &bus->device_list) {
		d = list_entry(l, struct vmm_device, head);
		if (strcmp(d->name, dev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&bus->lock);
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&dev->head);

	list_add_tail(&dev->head, &bus->device_list);

	/* Bus probe this device */
	__bus_probe_this_device(bus, dev);

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

int vmm_devdrv_attach_device(struct vmm_device *dev)
{
	struct vmm_bus *bus;
	
	if (!dev || !dev->bus) {
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
	
	if (!dev || !dev->bus) {
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
	bool found;
	struct dlist *l;
	struct vmm_bus *bus;
	struct vmm_device *d;

	if (!dev || !dev->bus) {
		return VMM_EFAIL;
	}
	bus = dev->bus;

	vmm_mutex_lock(&bus->lock);

	if (list_empty(&bus->device_list)) {
		vmm_mutex_unlock(&bus->lock);
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;
	list_for_each(l, &bus->device_list) {
		d = list_entry(l, struct vmm_device, head);
		if (strcmp(d->name, dev->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&bus->lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&d->head);

	/* Bus remove this device */
	__bus_remove_this_device(bus, d);

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

struct vmm_device *vmm_devdrv_find_device(const char *dname)
{
	bool found;
	struct dlist *lb, *ld;
	struct vmm_bus *bus;
	struct vmm_device *dev;

	if (!dname) {
		return NULL;
	}

	found = FALSE;
	dev = NULL;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each(lb, &ddctrl.bus_list) {
		bus = list_entry(lb, struct vmm_bus, head);

		vmm_mutex_lock(&bus->lock);
		list_for_each(ld, &bus->device_list) {
			dev = list_entry(ld, struct vmm_device, head);
			if (strcmp(dev->name, dname) == 0) {
				found = TRUE;
				break;
			}
		}
		vmm_mutex_unlock(&bus->lock);

		if (found) {
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	if (!found) {
		return NULL;
	}

	return dev;
}

struct vmm_device *vmm_devdrv_device(int index)
{
	bool found;
	struct dlist *lb, *ld;
	struct vmm_bus *bus;
	struct vmm_device *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each(lb, &ddctrl.bus_list) {
		bus = list_entry(lb, struct vmm_bus, head);

		vmm_mutex_lock(&bus->lock);
		list_for_each(ld, &bus->device_list) {
			retval = list_entry(ld, struct vmm_device, head);
			if (!index) {
				found = TRUE;
				break;
			}
			index--;
		}
		vmm_mutex_unlock(&bus->lock);

		if (found) {
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devdrv_device_count(void)
{
	u32 retval;
	struct vmm_bus *bus;
	struct dlist *lb, *ld;

	retval = 0;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each(lb, &ddctrl.bus_list) {
		bus = list_entry(lb, struct vmm_bus, head);

		vmm_mutex_lock(&bus->lock);
		list_for_each(ld, &bus->device_list) {
			retval++;
		}
		vmm_mutex_unlock(&bus->lock);
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	return retval;
}

int vmm_devdrv_register_driver(struct vmm_driver *drv)
{
	bool found;
	struct dlist *l;
	struct vmm_bus *bus;
	struct vmm_driver *d;

	if (!drv) {
		return VMM_EFAIL;
	}
	if (!drv->bus) {
		drv->bus = &ddctrl.default_bus;
	}
	bus = drv->bus;

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
	bool found;
	struct dlist *l;
	struct vmm_bus *bus;
	struct vmm_driver *d;

	if (!drv || !drv->bus) {
		return VMM_EFAIL;
	}
	bus = drv->bus;

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

struct vmm_driver *vmm_devdrv_find_driver(const char *dname)
{
	bool found;
	struct dlist *lb, *ld;
	struct vmm_bus *bus;
	struct vmm_driver *drv;

	if (!dname) {
		return NULL;
	}

	found = FALSE;
	drv = NULL;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each(lb, &ddctrl.bus_list) {
		bus = list_entry(lb, struct vmm_bus, head);

		vmm_mutex_lock(&bus->lock);
		list_for_each(ld, &bus->driver_list) {
			drv = list_entry(ld, struct vmm_driver, head);
			if (strcmp(drv->name, dname) == 0) {
				found = TRUE;
				break;
			}
		}
		vmm_mutex_unlock(&bus->lock);

		if (found) {
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	if (!found) {
		return NULL;
	}

	return drv;
}

struct vmm_driver *vmm_devdrv_driver(int index)
{
	bool found;
	struct dlist *lb, *ld;
	struct vmm_bus *bus;
	struct vmm_driver *retval;

	if (index < 0) {
		return NULL;
	}

	retval = NULL;
	found = FALSE;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each(lb, &ddctrl.bus_list) {
		bus = list_entry(lb, struct vmm_bus, head);

		vmm_mutex_lock(&bus->lock);
		list_for_each(ld, &bus->driver_list) {
			retval = list_entry(ld, struct vmm_driver, head);
			if (!index) {
				found = TRUE;
				break;
			}
			index--;
		}
		vmm_mutex_unlock(&bus->lock);

		if (found) {
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	if (!found) {
		return NULL;
	}

	return retval;
}

u32 vmm_devdrv_driver_count(void)
{
	u32 retval;
	struct vmm_bus *bus;
	struct dlist *lb, *ld;

	retval = 0;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each(lb, &ddctrl.bus_list) {
		bus = list_entry(lb, struct vmm_bus, head);

		vmm_mutex_lock(&bus->lock);
		list_for_each(ld, &bus->driver_list) {
			retval++;
		}
		vmm_mutex_unlock(&bus->lock);
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	return retval;
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
