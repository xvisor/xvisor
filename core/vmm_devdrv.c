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
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_devres.h>
#include <vmm_mutex.h>
#include <vmm_workqueue.h>
#include <vmm_devdrv.h>
#include <libs/stringlib.h>

struct vmm_devdrv_ctrl {
	struct vmm_mutex class_lock;
	struct dlist class_list;
	struct vmm_mutex bus_lock;
	struct dlist bus_list;

	struct vmm_mutex deferred_probe_lock;
	struct dlist deferred_probe_list;
	struct vmm_work deferred_probe_work;

	struct vmm_bus platform_bus;
};

static struct vmm_devdrv_ctrl ddctrl;

static void __bus_probe_this_device(struct vmm_bus *bus,
				    struct vmm_device *dev);

static void deferred_probe_work_func(struct vmm_work *work)
{
	struct vmm_device *d;

	vmm_mutex_lock(&ddctrl.deferred_probe_lock);

	while (!list_empty(&ddctrl.deferred_probe_list)) {
		d = list_first_entry(&ddctrl.deferred_probe_list,
					struct vmm_device, deferred_head);
		list_del_init(&d->deferred_head);

		vmm_mutex_unlock(&ddctrl.deferred_probe_lock);

		if (d->bus) {
			vmm_mutex_lock(&d->bus->lock);
			__bus_probe_this_device(d->bus, d);
			vmm_mutex_unlock(&d->bus->lock);
		}

		vmm_mutex_lock(&ddctrl.deferred_probe_lock);
	}

	vmm_mutex_unlock(&ddctrl.deferred_probe_lock);
}

static void deferred_probe_invoke(void)
{
	vmm_workqueue_schedule_work(NULL, &ddctrl.deferred_probe_work);
}

static void deferred_probe_add(struct vmm_device *dev)
{
	bool found = FALSE;
	struct vmm_device *d;

	vmm_mutex_lock(&ddctrl.deferred_probe_lock);

	found = FALSE;
	list_for_each_entry(d, &ddctrl.deferred_probe_list, deferred_head) {
		if (d == dev) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		list_add_tail(&dev->deferred_head,
			      &ddctrl.deferred_probe_list);
	}

	vmm_mutex_unlock(&ddctrl.deferred_probe_lock);

	if (!found) {
		deferred_probe_invoke();
	}
}

static void deferred_probe_del(struct vmm_device *dev)
{
	struct vmm_device *d;

	vmm_mutex_lock(&ddctrl.deferred_probe_lock);

	list_for_each_entry(d, &ddctrl.deferred_probe_list, deferred_head) {
		if (d == dev) {
			list_del(&dev->deferred_head);
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.deferred_probe_lock);
}

static int platform_bus_match(struct vmm_device *dev, struct vmm_driver *drv)
{
	const struct vmm_devtree_nodeid *match;

	if (!dev || !dev->node || !drv || !drv->match_table) {
		return 0;
	}

	if (dev->parent && (dev->node == dev->parent->node)) {
		return 0;
	}

	match = vmm_devtree_match_node(drv->match_table, dev->node);
	if (!match) {
		return 0;
	}

	return 1;
}

static int platform_bus_probe(struct vmm_device *dev)
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

static int platform_bus_remove(struct vmm_device *dev)
{
	struct vmm_driver *drv;

	if (!dev || !dev->node || !dev->driver) {
		return VMM_EFAIL;
	}
	drv = dev->driver;

	return drv->remove(dev);
}

static void platform_device_release(struct vmm_device *dev)
{
	vmm_devtree_dref_node(dev->node);
	dev->node = NULL;
	vmm_free(dev);
}

/* Note: Must be called with bus->lock held */
static int __bus_probe_device_driver(struct vmm_bus *bus,
				     struct vmm_device *dev,
				     struct vmm_driver *drv)
{
	int rc = VMM_OK;

	/* Device should be registered but not having any driver */
	if (!dev->is_registered ||
	    dev->autoprobe_disabled ||
	    dev->driver) {
		/* Note: we return OK so that caller
		 * does not try more drivers
		 */
		return VMM_OK;
	}

	/* Device should match the driver */
	if (bus->match && !bus->match(dev, drv)) {
		return VMM_ENODEV;
	}

	/* Notify bus event listeners */
	vmm_blocking_notifier_call(&bus->event_listeners,
				   VMM_BUS_NOTIFY_BIND_DRIVER, dev);

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
		if (rc != VMM_EPROBE_DEFER) {
			vmm_printf("devdrv: bus=\"%s\" device=\"%s\" "
				   "probe error %d\n",
				   bus->name, dev->name, rc);
		}
#endif
		dev->driver = NULL;
	} else {
		/* Notify bus event listeners */
		vmm_blocking_notifier_call(&bus->event_listeners,
					   VMM_BUS_NOTIFY_BOUND_DRIVER, dev);
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

	/* Notify bus event listeners */
	vmm_blocking_notifier_call(&bus->event_listeners,
				   VMM_BUS_NOTIFY_UNBIND_DRIVER, dev);

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
	} else {
		/* Notify bus event listeners */
		vmm_blocking_notifier_call(&bus->event_listeners,
					   VMM_BUS_NOTIFY_UNBOUND_DRIVER, dev);
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
	struct vmm_driver *drv;

	/* Try to bind pins for this device */
	rc = vmm_devdrv_pinctrl_bind(dev);
	if (rc == VMM_EPROBE_DEFER) {
		goto done;
	}

	/* Try each and every driver of this bus */
	list_for_each_entry(drv, &bus->driver_list, head) {
		rc = __bus_probe_device_driver(bus, dev, drv);
		if (!rc || rc == VMM_EPROBE_DEFER) {
			break;
		}
	}

done:
	/* Defer device probing if rc == VMM_EPROBE_DEFER */
	if (rc == VMM_EPROBE_DEFER) {
		/* Add device to deferred list */
		deferred_probe_add(dev);
	}
}

/* Note: Must be called with bus->lock held */
static void __bus_remove_this_device(struct vmm_bus *bus,
				     struct vmm_device *dev)
{
	/* Remove device from deferred list */
	deferred_probe_del(dev);

	__bus_remove_device_driver(bus, dev);
}

/* Note: Must be called with bus->lock held */
static void __bus_shutdown_this_device(struct vmm_bus *bus,
				       struct vmm_device *dev)
{
	/* Remove device from deferred list */
	deferred_probe_del(dev);

	__bus_shutdown_device_driver(bus, dev);
}

/* Note: Must be called with bus->lock held */
static void __bus_probe_this_driver(struct vmm_bus *bus,
				    struct vmm_driver *drv)
{
	int rc;
	struct vmm_device *dev;

	/* Try each and every device of this bus */
	list_for_each_entry(dev, &bus->device_list, bus_head) {
		/* If already probed then continue */
		if (dev->driver) {
			continue;
		}

		rc = __bus_probe_device_driver(bus, dev, drv);
		if (rc == VMM_EPROBE_DEFER) {
			/* Add device to deferred list */
			deferred_probe_add(dev);
		}
	}

	/* Invoke deferred device probing */
	deferred_probe_invoke();
}

/* Note: Must be called with bus->lock held */
static void __bus_remove_this_driver(struct vmm_bus *bus,
				     struct vmm_driver *drv)
{
	struct vmm_device *dev;

	/* Try each and every device of this bus */
	list_for_each_entry(dev, &bus->device_list, bus_head) {
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
	struct vmm_device *d;

	/* Forcefully destroy all devices */
	while (!list_empty(&bus->device_list)) {
		d = list_first_entry(&bus->device_list,
					struct vmm_device, bus_head);

		/* Bus shutdown/cleanup this device */
		__bus_shutdown_this_device(bus, d);

		/* Notify bus event listeners */
		vmm_blocking_notifier_call(&bus->event_listeners,
				   VMM_BUS_NOTIFY_DEL_DEVICE, d);

		/* Update parent child list */
		if (d->parent) {
			vmm_mutex_lock(&d->parent->child_list_lock);
			list_del(&d->child_head);
			vmm_mutex_unlock(&d->parent->child_list_lock);
			vmm_devdrv_dref_device(d->parent);
			d->parent = NULL;
		}

		/* Unregister from device list */
		list_del(&d->bus_head);
		d->is_registered = FALSE;

		/* Decrement reference count of device */
		vmm_devdrv_dref_device(d);
	}
}

/* Note: Must be called with cls->lock held */
void __class_release(struct vmm_class *cls)
{
	struct dlist *l;
	struct vmm_device *d;

	/* Forcefully destroy all devices */
	while (!list_empty(&cls->device_list)) {
		l = list_first(&cls->device_list);
		d = list_entry(l, struct vmm_device, class_head);

		/* Update parent child list */
		if (d->parent) {
			vmm_mutex_lock(&d->parent->child_list_lock);
			list_del(&d->child_head);
			vmm_mutex_unlock(&d->parent->child_list_lock);
			vmm_devdrv_dref_device(d->parent);
			d->parent = NULL;
		}

		/* Update class device list */
		list_del(&d->class_head);
		d->is_registered = FALSE;

		/* Decrement reference count of device */
		vmm_devdrv_dref_device(d);
	}
}

int __weak vmm_devdrv_pinctrl_bind(struct vmm_device *dev)
{
	/* Nothing to do here. */
	/* The pinctrl framework will provide actual implementation */
	return VMM_OK;
}

static int devdrv_probe(struct vmm_devtree_node *node,
			struct vmm_device *parent)
{
	int rc;
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
		vmm_free(dev);
		return VMM_EOVERFLOW;
	}
	vmm_devtree_ref_node(node);
	dev->node = node;
	dev->parent = parent;
	dev->bus = &ddctrl.platform_bus;
	dev->release = platform_device_release;
	dev->priv = NULL;

	rc = vmm_devdrv_register_device(dev);
	if (rc) {
		vmm_free(dev);
		return rc;
	}

	vmm_devtree_for_each_child(child, node) {
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
	struct vmm_class *c;

	if (cls == NULL) {
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each_entry(c, &ddctrl.class_list, head) {
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
	struct vmm_class *c;

	vmm_mutex_lock(&ddctrl.class_lock);

	if (cls == NULL || list_empty(&ddctrl.class_list)) {
		vmm_mutex_unlock(&ddctrl.class_lock);
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;
	list_for_each_entry(c, &ddctrl.class_list, head) {
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
	vmm_mutex_lock(&c->lock);
	__class_release(c);
	vmm_mutex_unlock(&c->lock);

	list_del(&c->head);

	vmm_mutex_unlock(&ddctrl.class_lock);

	return VMM_OK;
}

struct vmm_class *vmm_devdrv_find_class(const char *cname)
{
	bool found;
	struct vmm_class *c;

	if (!cname) {
		return NULL;
	}

	found = FALSE;
	c = NULL;

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each_entry(c, &ddctrl.class_list, head) {
		if (strcmp(c->name, cname) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.class_lock);

	if (!found) {
		return NULL;
	}

	return c;
}

int vmm_devdrv_class_iterate(struct vmm_class *start, void *data,
			     int (*fn)(struct vmm_class *cls, void *data))
{
	int rc = VMM_OK;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_class *c = NULL;

	if (!fn) {
		return VMM_EINVALID;
	}
	if (start) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each_entry(c, &ddctrl.class_list, head) {
		if (!start_found) {
			if (start && start == c) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		rc = fn(c, data);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.class_lock);

	return rc;
}

u32 vmm_devdrv_class_count(void)
{
	u32 retval;
	struct vmm_class *c;

	retval = 0;

	vmm_mutex_lock(&ddctrl.class_lock);

	list_for_each_entry(c, &ddctrl.class_list, head) {
		retval++;
	}

	vmm_mutex_unlock(&ddctrl.class_lock);

	return retval;
}

static int devdrv_class_register_device(struct vmm_class *cls,
				        struct vmm_device *dev)
{
	bool found;
	struct vmm_device *d;

	if (!dev || !cls || (dev->class != cls)) {
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;

	vmm_mutex_lock(&cls->lock);

	/* Check duplicacy */
	list_for_each_entry(d, &cls->device_list, class_head) {
		if (strcmp(d->name, dev->name) == 0) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		vmm_mutex_unlock(&cls->lock);
		return VMM_EINVALID;
	}

	/* Update class device list */
	INIT_LIST_HEAD(&dev->class_head);
	vmm_devdrv_ref_device(dev);
	list_add_tail(&dev->class_head, &cls->device_list);
	dev->is_registered = TRUE;

	/* Update parent child list */
	if (dev->parent) {
		vmm_devdrv_ref_device(dev->parent);
		vmm_mutex_lock(&dev->parent->child_list_lock);
		list_add_tail(&dev->child_head, &dev->parent->child_list);
		vmm_mutex_unlock(&dev->parent->child_list_lock);
	}

	vmm_mutex_unlock(&cls->lock);

	return VMM_OK;
}

static int devdrv_class_unregister_device(struct vmm_class *cls,
					  struct vmm_device *dev)
{
	bool found;
	struct vmm_device *d;

	if (!dev || !cls || (dev->class != cls)) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&cls->lock);

	if (list_empty(&cls->device_list)) {
		vmm_mutex_unlock(&cls->lock);
		return VMM_EFAIL;
	}

	/* Check existance */
	d = NULL;
	found = FALSE;
	list_for_each_entry(d, &cls->device_list, class_head) {
		if (strcmp(d->name, dev->name) == 0) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		vmm_mutex_unlock(&cls->lock);
		return VMM_ENOTAVAIL;
	}

	/* Update parent child list */
	if (d->parent) {
		vmm_mutex_lock(&d->parent->child_list_lock);
		list_del(&d->child_head);
		vmm_mutex_unlock(&d->parent->child_list_lock);
		vmm_devdrv_dref_device(d->parent);
		d->parent = NULL;
	}

	/* Update class device list */
	list_del(&d->class_head);
	d->is_registered = FALSE;

	/* Decrement reference count of device */
	vmm_devdrv_dref_device(d);

	vmm_mutex_unlock(&cls->lock);

	return VMM_OK;
}

struct vmm_device *vmm_devdrv_class_find_device(struct vmm_class *cls,
				void *data,
				int (*match) (struct vmm_device *, void *))
{
	bool found;
	struct vmm_device *d;

	if (!cls || !match) {
		return NULL;
	}

	found = FALSE;
	d = NULL;

	vmm_mutex_lock(&cls->lock);

	list_for_each_entry(d, &cls->device_list, class_head) {
		if (match(d, data)) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&cls->lock);

	if (!found) {
		return NULL;
	}

	return d;
}

struct vmm_device *vmm_devdrv_class_find_device_by_name(
						struct vmm_class *cls,
						const char *dname)
{
	bool found;
	struct vmm_device *d;

	if (!cls || !dname) {
		return NULL;
	}

	found = FALSE;
	d = NULL;

	vmm_mutex_lock(&cls->lock);

	list_for_each_entry(d, &cls->device_list, class_head) {
		if (strcmp(d->name, dname) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&cls->lock);

	if (!found) {
		return NULL;
	}

	return d;
}

int vmm_devdrv_class_device_iterate(struct vmm_class *cls,
			struct vmm_device *start, void *data,
			int (*fn)(struct vmm_device *dev, void *data))
{
	int rc = VMM_OK;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_device *d = NULL;

	if (!cls || !fn) {
		return VMM_EINVALID;
	}
	if (start && start->class != cls) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&cls->lock);

	list_for_each_entry(d, &cls->device_list, class_head) {
		if (!start_found) {
			if (start && start == d) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		rc = fn(d, data);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&cls->lock);

	return rc;
}

u32 vmm_devdrv_class_device_count(struct vmm_class *cls)
{
	u32 retval;
	struct vmm_device *d;

	if (!cls) {
		return 0;
	}

	retval = 0;

	vmm_mutex_lock(&cls->lock);

	list_for_each_entry(d, &cls->device_list, class_head) {
		retval++;
	}

	vmm_mutex_unlock(&cls->lock);

	return retval;
}

int vmm_devdrv_register_bus(struct vmm_bus *bus)
{
	bool found;
	struct vmm_bus *b;

	if (bus == NULL) {
		return VMM_EFAIL;
	}

	b = NULL;
	found = FALSE;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each_entry(b, &ddctrl.bus_list, head) {
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
	BLOCKING_INIT_NOTIFIER_CHAIN(&bus->event_listeners);

	list_add_tail(&bus->head, &ddctrl.bus_list);

	vmm_mutex_unlock(&ddctrl.bus_lock);

	return VMM_OK;
}

int vmm_devdrv_unregister_bus(struct vmm_bus *bus)
{
	bool found;
	struct vmm_bus *b;

	vmm_mutex_lock(&ddctrl.bus_lock);

	if (bus == NULL || list_empty(&ddctrl.bus_list)) {
		vmm_mutex_unlock(&ddctrl.bus_lock);
		return VMM_EFAIL;
	}

	b = NULL;
	found = FALSE;
	list_for_each_entry(b, &ddctrl.bus_list, head) {
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
	struct vmm_bus *b;

	if (!bname) {
		return NULL;
	}

	found = FALSE;
	b = NULL;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each_entry(b, &ddctrl.bus_list, head) {
		if (strcmp(b->name, bname) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	if (!found) {
		return NULL;
	}

	return b;
}

int vmm_devdrv_bus_iterate(struct vmm_bus *start, void *data,
			   int (*fn)(struct vmm_bus *bus, void *data))
{
	int rc = VMM_OK;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_bus *b = NULL;

	if (!fn) {
		return VMM_EINVALID;
	}
	if (start) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each_entry(b, &ddctrl.bus_list, head) {
		if (!start_found) {
			if (start && start == b) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		rc = fn(b, data);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	return rc;
}

u32 vmm_devdrv_bus_count(void)
{
	u32 retval;
	struct vmm_bus *b;

	retval = 0;

	vmm_mutex_lock(&ddctrl.bus_lock);

	list_for_each_entry(b, &ddctrl.bus_list, head) {
		retval++;
	}

	vmm_mutex_unlock(&ddctrl.bus_lock);

	return retval;
}

static int devdrv_bus_register_device(struct vmm_bus *bus,
				      struct vmm_device *dev)
{
	bool found;
	struct vmm_device *d;

	if (!dev || !bus || (dev->bus != bus)) {
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;

	vmm_mutex_lock(&bus->lock);

	/* Check duplicacy */
	list_for_each_entry(d, &bus->device_list, bus_head) {
		if (strcmp(d->name, dev->name) == 0) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		vmm_mutex_unlock(&bus->lock);
		return VMM_EINVALID;
	}

	/* Update bus device list */
	INIT_LIST_HEAD(&dev->bus_head);
	vmm_devdrv_ref_device(dev);
	list_add_tail(&dev->bus_head, &bus->device_list);
	dev->is_registered = TRUE;

	/* Update parent child list */
	if (dev->parent) {
		vmm_devdrv_ref_device(dev->parent);
		vmm_mutex_lock(&dev->parent->child_list_lock);
		list_add_tail(&dev->child_head, &dev->parent->child_list);
		vmm_mutex_unlock(&dev->parent->child_list_lock);
	}

	/* Notify bus event listeners */
	vmm_blocking_notifier_call(&bus->event_listeners,
				   VMM_BUS_NOTIFY_ADD_DEVICE, dev);

	/* Bus probe this device */
	__bus_probe_this_device(bus, dev);

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

static int devdrv_bus_unregister_device(struct vmm_bus *bus,
				        struct vmm_device *dev)
{
	bool found;
	struct vmm_device *d;

	if (!dev || !bus || (dev->bus != bus)) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&bus->lock);

	if (list_empty(&bus->device_list)) {
		vmm_mutex_unlock(&bus->lock);
		return VMM_EFAIL;
	}

	/* Check existance */
	d = NULL;
	found = FALSE;
	list_for_each_entry(d, &bus->device_list, bus_head) {
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

	/* Notify bus event listeners */
	vmm_blocking_notifier_call(&bus->event_listeners,
				   VMM_BUS_NOTIFY_DEL_DEVICE, d);

	/* Update parent child list */
	if (d->parent) {
		vmm_mutex_lock(&d->parent->child_list_lock);
		list_del(&d->child_head);
		vmm_mutex_unlock(&d->parent->child_list_lock);
		vmm_devdrv_dref_device(d->parent);
		d->parent = NULL;
	}

	/* Update bus device list */
	list_del(&d->bus_head);
	d->is_registered = FALSE;

	/* Decrement reference count of device */
	vmm_devdrv_dref_device(d);

	vmm_mutex_unlock(&bus->lock);

	return VMM_OK;
}

struct vmm_device *vmm_devdrv_bus_find_device(struct vmm_bus *bus,
				struct vmm_device *start,
				void *data,
				int (*match) (struct vmm_device *, void *))
{
	bool found = FALSE;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_device *d = NULL;

	if (!bus || !match) {
		return NULL;
	}

	vmm_mutex_lock(&bus->lock);

	list_for_each_entry(d, &bus->device_list, bus_head) {
		if (!start_found) {
			if (start && start == d) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		if (match(d, data)) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&bus->lock);

	if (!found) {
		return NULL;
	}

	return d;
}

static int devdrv_name_match(struct vmm_device *dev, void *data)
{
	return (strcmp(dev->name, (const char*)data) == 0) ? 1 : 0;
}

struct vmm_device *vmm_devdrv_bus_find_device_by_name(struct vmm_bus *bus,
						struct vmm_device *start,
						const char *dname)
{
	return vmm_devdrv_bus_find_device(bus, start, (void *)dname,
					  devdrv_name_match);
}

int vmm_devdrv_bus_device_iterate(struct vmm_bus *bus,
			struct vmm_device *start, void *data,
			int (*fn)(struct vmm_device *dev, void *data))
{
	int rc = VMM_OK;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_device *d = NULL;

	if (!bus || !fn) {
		return VMM_EINVALID;
	}
	if (start && start->bus != bus) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&bus->lock);

	list_for_each_entry(d, &bus->device_list, bus_head) {
		if (!start_found) {
			if (start && start == d) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		rc = fn(d, data);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&bus->lock);

	return rc;
}

u32 vmm_devdrv_bus_device_count(struct vmm_bus *bus)
{
	u32 retval;
	struct vmm_device *d;

	if (!bus) {
		return 0;
	}

	retval = 0;

	vmm_mutex_lock(&bus->lock);

	list_for_each_entry(d, &bus->device_list, bus_head) {
		retval++;
	}

	vmm_mutex_unlock(&bus->lock);

	return retval;
}

int vmm_devdrv_bus_register_driver(struct vmm_bus *bus,
				   struct vmm_driver *drv)
{
	bool found;
	struct vmm_driver *d;

	if (!drv || !bus || (drv->bus != bus)) {
		return VMM_EFAIL;
	}

	d = NULL;
	found = FALSE;

	vmm_mutex_lock(&bus->lock);

	list_for_each_entry(d, &bus->driver_list, head) {
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
	list_for_each_entry(d, &bus->driver_list, head) {
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
	struct vmm_driver *d;

	if (!bus || !dname) {
		return NULL;
	}

	found = FALSE;
	d = NULL;

	vmm_mutex_lock(&bus->lock);

	list_for_each_entry(d, &bus->driver_list, head) {
		if (strcmp(d->name, dname) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&bus->lock);

	if (!found) {
		return NULL;
	}

	return d;
}

int vmm_devdrv_bus_driver_iterate(struct vmm_bus *bus,
			struct vmm_driver *start, void *data,
			int (*fn)(struct vmm_driver *drv, void *data))
{
	int rc = VMM_OK;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_driver *d = NULL;

	if (!bus || !fn) {
		return VMM_EINVALID;
	}
	if (start && start->bus != bus) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&bus->lock);

	list_for_each_entry(d, &bus->driver_list, head) {
		if (!start_found) {
			if (start && start == d) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		rc = fn(d, data);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&bus->lock);

	return rc;
}

u32 vmm_devdrv_bus_driver_count(struct vmm_bus *bus)
{
	u32 retval;
	struct vmm_driver *d;

	if (!bus) {
		return 0;
	}

	retval = 0;

	vmm_mutex_lock(&bus->lock);

	list_for_each_entry(d, &bus->driver_list, head) {
		retval++;
	}

	vmm_mutex_unlock(&bus->lock);

	return retval;
}

int vmm_devdrv_bus_register_notifier(struct vmm_bus *bus,
				     struct vmm_notifier_block *nb)
{
	if (!bus || !nb) {
		return VMM_EINVALID;
	}

	return vmm_blocking_notifier_register(&bus->event_listeners, nb);
}

int vmm_devdrv_bus_unregister_notifier(struct vmm_bus *bus,
					struct vmm_notifier_block *nb)
{
	if (!bus || !nb) {
		return VMM_EINVALID;
	}

	return vmm_blocking_notifier_unregister(&bus->event_listeners, nb);
}

void vmm_devdrv_initialize_device(struct vmm_device *dev)
{
	if (!dev) {
		return;
	}

	/* Only initialize the private fields of device */
	INIT_LIST_HEAD(&dev->bus_head);
	INIT_LIST_HEAD(&dev->class_head);
	arch_atomic_write(&dev->ref_count, 1);
	dev->is_registered = FALSE;
	INIT_LIST_HEAD(&dev->child_head);
	INIT_MUTEX(&dev->child_list_lock);
	INIT_LIST_HEAD(&dev->child_list);
	INIT_SPIN_LOCK(&dev->devres_lock);
	INIT_LIST_HEAD(&dev->devres_head);
	INIT_LIST_HEAD(&dev->deferred_head);
}

struct vmm_device *vmm_devdrv_ref_device(struct vmm_device *dev)
{
	if (!dev) {
		return NULL;
	}

	arch_atomic_inc(&dev->ref_count);
	return dev;
}

void vmm_devdrv_dref_device(struct vmm_device *dev)
{
	bool released;

	if (!dev) {
		return;
	}

	if (arch_atomic_sub_return(&dev->ref_count, 1)) {
		return;
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

int vmm_devdrv_for_each_child(struct vmm_device *dev, void *data,
			      int (*fn)(struct vmm_device *dev, void *data))
{
	int err = 0;
	struct vmm_device *child = NULL;
	struct vmm_device *temp = NULL;

	if (!dev) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&dev->child_list_lock);

	list_for_each_entry_safe(child, temp, &dev->child_list, child_head) {
		if (VMM_OK != (err = fn(child, data))) {
			vmm_mutex_unlock(&dev->child_list_lock);
			return err;
		}
	}

	vmm_mutex_unlock(&dev->child_list_lock);

	return VMM_OK;
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
		drv->bus = &ddctrl.platform_bus;
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

	INIT_MUTEX(&ddctrl.deferred_probe_lock);
	INIT_LIST_HEAD(&ddctrl.deferred_probe_list);
	INIT_WORK(&ddctrl.deferred_probe_work, deferred_probe_work_func);

	strcpy(ddctrl.platform_bus.name, "platform");
	ddctrl.platform_bus.match = platform_bus_match;
	ddctrl.platform_bus.probe = platform_bus_probe;
	ddctrl.platform_bus.remove = platform_bus_remove;

	return vmm_devdrv_register_bus(&ddctrl.platform_bus);
}
