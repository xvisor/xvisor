/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file device.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of usb device APIs
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <drv/usb.h>
#include <drv/usb/hcd.h>

/* Protect struct usb_device->state and ->children members */
static DEFINE_SPINLOCK(device_state_lock);

enum usb_device_state usb_get_device_state(struct usb_device *udev)
{
	irq_flags_t flags;
	enum usb_device_state ret;

	vmm_spin_lock_irqsave(&device_state_lock, flags);
	ret = udev->state;
	vmm_spin_unlock_irqrestore(&device_state_lock, flags);

	return ret;
}
VMM_EXPORT_SYMBOL(usb_get_device_state);

static void recursively_mark_NOTATTACHED(struct usb_device *udev)
{
	int i;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&udev->children_lock, flags);
	for (i = 0; i < udev->maxchild; ++i) {
		if (!udev->children[i]) {
			continue;
		}
		vmm_spin_unlock_irqrestore(&udev->children_lock, flags);
		recursively_mark_NOTATTACHED(udev->children[i]);
		vmm_spin_lock_irqsave(&udev->children_lock, flags);
	}
	vmm_spin_unlock_irqrestore(&udev->children_lock, flags);

	if (udev->state == USB_STATE_SUSPENDED) {
		udev->active_duration -= vmm_timer_timestamp();
	}

	udev->state = USB_STATE_NOTATTACHED;
}

void usb_set_device_state(struct usb_device *udev,
			  enum usb_device_state new_state)
{
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&device_state_lock, flags);
	if (udev->state == USB_STATE_NOTATTACHED) {
		;	/* do nothing */
	} else if (new_state != USB_STATE_NOTATTACHED) {
		if (udev->state == USB_STATE_SUSPENDED &&
			new_state != USB_STATE_SUSPENDED)
			udev->active_duration -= vmm_timer_timestamp();
		else if (new_state == USB_STATE_SUSPENDED &&
				udev->state != USB_STATE_SUSPENDED)
			udev->active_duration += vmm_timer_timestamp();
		udev->state = new_state;
	} else {
		recursively_mark_NOTATTACHED(udev);
	}
	vmm_spin_unlock_irqrestore(&device_state_lock, flags);
}
VMM_EXPORT_SYMBOL(usb_set_device_state);

static void usb_release_device(struct vmm_device *ddev)
{
	irq_flags_t flags;
	struct usb_device *dev = to_usb_device(ddev);
	struct usb_device *parent = (ddev->parent) ?
				to_usb_device(ddev->parent) : NULL;

	/* Update HCD device number bitmap */
	vmm_spin_lock_irqsave(&dev->hcd->devicemap_lock, flags);
	__clear_bit(dev->devnum - 1, dev->hcd->devicemap);
	vmm_spin_unlock_irqrestore(&dev->hcd->devicemap_lock, flags);

	/* Update parent device */
	if (parent) {
		vmm_spin_lock_irqsave(&parent->children_lock, flags);
		parent->children[dev->portnum] = NULL;
		vmm_spin_unlock_irqrestore(&parent->children_lock, flags);
	}

	/* Root hubs aren't true devices, so free HCD resources */
	if (dev->hcd->driver->free_dev && parent) {
		dev->hcd->driver->free_dev(dev->hcd, dev);
	}

	/* Destroy HCD this will reduce HCD referenece count */
	usb_dref_hcd(dev->hcd);

	/* Release memory of the usb device */
	vmm_free(dev);
}

static void usb_release_interface(struct vmm_device *ddev)
{
	/* Nothing to do here because usb interface device will
	 * be released automatically when parent usb device is
	 * released.
	 */
}

struct vmm_device_type usb_device_type = {
	.name = "usb_device",
	.release = usb_release_device,
};
VMM_EXPORT_SYMBOL(usb_device_type);

struct vmm_device_type usb_interface_type = {
	.name = "usb_interface",
	.release = usb_release_interface,
};
VMM_EXPORT_SYMBOL(usb_interface_type);

struct usb_device *usb_alloc_device(struct usb_device *parent,
				    struct usb_hcd *hcd, unsigned port)
{
	int i;
	irq_flags_t flags;
	struct usb_device *dev;

	/* Sanity checks */
	if (parent) {
		if (USB_MAXCHILDREN <= port) {
			return NULL;
		}
		vmm_spin_lock_irqsave(&parent->children_lock, flags);
		if (parent->children[port]) {
			vmm_spin_unlock_irqrestore(&parent->children_lock,
						   flags);
			return NULL;
		}
		vmm_spin_unlock_irqrestore(&parent->children_lock, flags);
	}

	/* Alloc new device */
	dev = vmm_zalloc(sizeof(*dev));
	if (!dev) {
		return NULL;
	}

	/* Set parent usb device pointer */
	dev->parent = parent;

	/* Initialize devdrv context */
	vmm_devdrv_initialize_device(&dev->dev);
	dev->dev.autoprobe_disabled = TRUE;
	dev->dev.parent = (parent) ? &parent->dev : NULL;
	dev->dev.bus = &usb_bus_type;
	dev->dev.type = &usb_device_type;

	/* Increment reference count of HCD */
	usb_ref_hcd(hcd);

	/* Root hubs aren't true devices, so don't allocate HCD resources */
	if (hcd->driver->alloc_dev && parent &&
		!hcd->driver->alloc_dev(hcd, dev)) {
		usb_dref_hcd(hcd);
		vmm_free(dev);
		return NULL;
	}

	/* Update device state */
	dev->state = USB_STATE_NOTATTACHED;

	/* Update device name, devpath, route, and level */
	if (unlikely(!parent)) {
		dev->devpath[0] = '0';
		dev->route = 0;
		dev->level = 0;
		vmm_snprintf(dev->dev.name, sizeof(dev->dev.name),
			     "usb%d", hcd->bus_num);
	} else {
		if (parent->level == 0) {
			/* Root hub port is not counted in route string
			 * because it is always zero.
			 */
			vmm_snprintf(dev->devpath, sizeof(dev->devpath),
				     "%d", port);
		} else {
			vmm_snprintf(dev->devpath, sizeof(dev->devpath),
				     "%s.%d", parent->devpath, port);
		}
		/* Route string assumes hubs have less than 16 ports */
		if (port < 15) {
			dev->route = parent->route +
				(port << (parent->level * 4));
		} else {
			dev->route = parent->route +
				(15 << (parent->level * 4));
		}
		dev->level = parent->level + 1;
		vmm_snprintf(dev->dev.name, sizeof(dev->dev.name),
			     "usb%d-%s", hcd->bus_num, dev->devpath);
		/* FIXME: hub driver sets up TT records */
		/* Update parent device */
		vmm_spin_lock_irqsave(&parent->children_lock, flags);
		parent->children[port] = dev;
		vmm_spin_unlock_irqrestore(&parent->children_lock, flags);
	}

	/* Update rest of the device fields */
	dev->portnum = port;
	dev->hcd = hcd;
	dev->maxchild = 0;
	INIT_SPIN_LOCK(&dev->children_lock);
	for (i = 0; i < USB_MAXCHILDREN; i++) {
		dev->children[i] = NULL;
	}

	/* Assign device number based on HCD device bitmap
	 * Note: Device number starts from 1.
	 * Note: Device number 0 is default device.
	 */
	vmm_spin_lock_irqsave(&hcd->devicemap_lock, flags);
	dev->devnum = 0;
	for (i = 0; i < USB_MAX_DEVICE; i++) {
		if (!test_bit(i, hcd->devicemap)) {
			__set_bit(i, hcd->devicemap);
			dev->devnum = i + 1;
			break;
		}
	}
	i = dev->devnum;
	vmm_spin_unlock_irqrestore(&hcd->devicemap_lock, flags);
	if (i == 0) {
		usb_dref_hcd(hcd);
		vmm_free(dev);
		return NULL;
	}

	return dev;
}
VMM_EXPORT_SYMBOL(usb_alloc_device);

void usb_ref_device(struct usb_device *dev)
{
	if (dev) {
		vmm_devdrv_ref_device(&dev->dev);
	}
}
VMM_EXPORT_SYMBOL(usb_ref_device);

void usb_dref_device(struct usb_device *dev)
{
	if (dev) {
		vmm_devdrv_dref_device(&dev->dev);
	}
}
VMM_EXPORT_SYMBOL(usb_dref_device);

struct usb_device *usb_find_child(struct usb_device *hdev, int port1)
{
	irq_flags_t flags;
	struct usb_device *ret;

	if (!hdev || port1 < 1 || port1 > hdev->maxchild)
		return NULL;

	vmm_spin_lock_irqsave(&hdev->children_lock, flags);
	ret = hdev->children[port1 - 1];
	vmm_spin_unlock_irqrestore(&hdev->children_lock, flags);

	return ret;
}
VMM_EXPORT_SYMBOL(usb_find_child);

int usb_get_usb2_hub_address_port(struct usb_device *dev,
				  u8 *hub_addr, u8 *hub_port)
{
	u8 haddr = 0, hport = 0;

	if (!dev || !hub_addr || !hub_port)
		return VMM_EINVALID;

	while (dev->parent != NULL) {
		if (dev->parent->speed != USB_SPEED_HIGH) {
			dev = dev->parent;
		} else {
			haddr = dev->parent->devnum;
			hport = dev->portnum;
			break;
		}
	}

	if (hub_addr)
		*hub_addr = haddr;
	if (hub_port)
		*hub_port = hport;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(usb_get_parent_address_port);
