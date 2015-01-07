/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file driver.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of usb driver APIs
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <drv/usb.h>

static const struct usb_device_id *usb_match_dynamic_id(struct usb_interface *intf,
							struct usb_driver *drv)
{
	struct usb_dynid *dynid;

	vmm_spin_lock(&drv->dynids.lock);
	list_for_each_entry(dynid, &drv->dynids.list, node) {
		if (usb_match_one_id(intf, &dynid->id)) {
			vmm_spin_unlock(&drv->dynids.lock);
			return &dynid->id;
		}
	}
	vmm_spin_unlock(&drv->dynids.lock);

	return NULL;
}

/* returns 0 if no match, 1 if match */
static int usb_match_device(struct usb_device *dev,
			    const struct usb_device_id *id)
{
	if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
	    id->idVendor != vmm_le16_to_cpu(dev->descriptor.idVendor))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
	    id->idProduct != vmm_le16_to_cpu(dev->descriptor.idProduct))
		return 0;

	/* No need to test id->bcdDevice_lo != 0, since 0 is never
	   greater than any unsigned number. */
	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO) &&
	    (id->bcdDevice_lo > vmm_le16_to_cpu(dev->descriptor.bcdDevice)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI) &&
	    (id->bcdDevice_hi < vmm_le16_to_cpu(dev->descriptor.bcdDevice)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
	    (id->bDeviceClass != dev->descriptor.bDeviceClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
	    (id->bDeviceSubClass != dev->descriptor.bDeviceSubClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
	    (id->bDeviceProtocol != dev->descriptor.bDeviceProtocol))
		return 0;

	return 1;
}

/* returns 0 if no match, 1 if match */
static int usb_match_one_id_intf(struct usb_device *dev,
				 struct usb_interface *intf,
				 const struct usb_device_id *id)
{
	/* The interface class, subclass, protocol and number should never be
	 * checked for a match if the device class is Vendor Specific,
	 * unless the match record specifies the Vendor ID. */
	if (dev->descriptor.bDeviceClass == USB_CLASS_VENDOR_SPEC &&
			!(id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
			(id->match_flags & (USB_DEVICE_ID_MATCH_INT_CLASS |
				USB_DEVICE_ID_MATCH_INT_SUBCLASS |
				USB_DEVICE_ID_MATCH_INT_PROTOCOL |
				USB_DEVICE_ID_MATCH_INT_NUMBER)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_CLASS) &&
	    (id->bInterfaceClass != intf->desc.bInterfaceClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_SUBCLASS) &&
	    (id->bInterfaceSubClass != intf->desc.bInterfaceSubClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_PROTOCOL) &&
	    (id->bInterfaceProtocol != intf->desc.bInterfaceProtocol))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_NUMBER) &&
	    (id->bInterfaceNumber != intf->desc.bInterfaceNumber))
		return 0;

	return 1;
}

bool usb_match_one_id(struct usb_interface *intf,
		      const struct usb_device_id *id)
{
	struct usb_device *dev;

	if (id == NULL || intf == NULL)
		return FALSE;

	dev = interface_to_usbdev(intf);

	if (!usb_match_device(dev, id))
		return FALSE;

	return usb_match_one_id_intf(dev, intf, id);
}
VMM_EXPORT_SYMBOL(usb_match_one_id);

const struct usb_device_id *usb_match_id(struct usb_interface *intf,
					 const struct usb_device_id *id)
{
	if (id == NULL || intf == NULL)
		return NULL;

	for (; id->idVendor || id->idProduct || id->bDeviceClass ||
	       id->bInterfaceClass || id->driver_info; id++) {
		if (usb_match_one_id(intf, id))
			return id;
	}

	return NULL;
}
VMM_EXPORT_SYMBOL(usb_match_id);

const struct usb_device_id *usb_match_interface(struct usb_interface *intf,
						struct usb_driver *drv)
{
	const struct usb_device_id *id;

	if (drv == NULL || intf == NULL)
		return NULL;

	id = usb_match_id(intf, drv->id_table);
	if (id)
		return id;

	id = usb_match_dynamic_id(intf, drv);
	if (id)
		return id;

	return NULL;
}
VMM_EXPORT_SYMBOL(usb_match_interface);

int usb_add_dynid(struct usb_driver *driver,
		  u32 idVendor, u32 idProduct, u32 bInterfaceClass)
{
	irq_flags_t flags;
	struct usb_dynid *dynid;

	dynid = vmm_zalloc(sizeof(*dynid));
	if (!dynid) {
		return VMM_ENOMEM;
	}

	INIT_LIST_HEAD(&dynid->node);
	dynid->id.idVendor = idVendor;
	dynid->id.idProduct = idProduct;
	dynid->id.match_flags = USB_DEVICE_ID_MATCH_DEVICE;
	if (bInterfaceClass) {
		dynid->id.bInterfaceClass = (u8)bInterfaceClass;
		dynid->id.match_flags |= USB_DEVICE_ID_MATCH_INT_CLASS;
	}

	vmm_spin_lock_irqsave(&driver->dynids.lock, flags);
	list_add_tail(&dynid->node, &driver->dynids.list);
	vmm_spin_unlock_irqrestore(&driver->dynids.lock, flags);

	return vmm_devdrv_attach_driver(&driver->drv);
}
VMM_EXPORT_SYMBOL(usb_add_dynid);

int usb_del_dynid(struct usb_driver *driver,
		  u32 idVendor, u32 idProduct)
{
	irq_flags_t flags;
	struct usb_dynid *dynid, *n;

	if (!driver) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&driver->dynids.lock, flags);
	list_for_each_entry_safe(dynid, n, &driver->dynids.list, node) {
		struct usb_device_id *id = &dynid->id;
		if ((id->idVendor == idVendor) &&
		    (id->idProduct == idProduct)) {
			list_del(&dynid->node);
			vmm_free(dynid);
			break;
		}
	}
	vmm_spin_unlock_irqrestore(&driver->dynids.lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(usb_del_dynid);

int usb_pre_reset_driver(struct usb_interface *intf,
			 struct usb_driver *drv)
{
	if (!intf || !drv) {
		return VMM_EINVALID;
	}

	return drv->pre_reset(intf);
}
VMM_EXPORT_SYMBOL(usb_pre_reset_driver);

int usb_post_reset_driver(struct usb_interface *intf,
			  struct usb_driver *drv)
{
	if (!intf || !drv) {
		return VMM_EINVALID;
	}

	return drv->post_reset(intf);
}
VMM_EXPORT_SYMBOL(usb_post_reset_driver);

int usb_register(struct usb_driver *drv)
{
	if (!drv) {
		return VMM_EINVALID;
	}

	strncpy(drv->drv.name, drv->name, sizeof(drv->drv.name));
	drv->drv.bus = &usb_bus_type;

	INIT_SPIN_LOCK(&drv->dynids.lock);
	INIT_LIST_HEAD(&drv->dynids.list);

	return vmm_devdrv_register_driver(&drv->drv);
}
VMM_EXPORT_SYMBOL(usb_register);

void usb_deregister(struct usb_driver *drv)
{
	if (!drv) {
		return;
	}

	vmm_devdrv_unregister_driver(&drv->drv);
}
VMM_EXPORT_SYMBOL(usb_deregister);

static int usb_bus_match(struct vmm_device *dev, struct vmm_driver *drv)
{
	struct usb_interface *intf;
	struct usb_driver *udrv;

	if (dev->type != &usb_interface_type) {
		return 0;
	}

	intf = to_usb_interface(dev);
	udrv = to_usb_driver(drv);

	return usb_match_interface(intf, udrv) ? 1 : 0;
}

static int usb_bus_probe(struct vmm_device *dev)
{
	int err;
	struct usb_interface *intf;
	struct usb_driver *udrv;
	const struct usb_device_id *id;

	if (dev->type != &usb_interface_type) {
		return VMM_ENODEV;
	}

	intf = to_usb_interface(dev);
	udrv = to_usb_driver(dev->driver);

	id = usb_match_interface(intf, udrv);
	if (!id) {
		return VMM_ENODEV;
	}

	if (udrv->probe) {
		err = udrv->probe(intf, id);
	} else {
		err = VMM_ENODEV;
	}

	return err;
}

static int usb_bus_remove(struct vmm_device *dev)
{
	int err;
	struct usb_interface *intf;
	struct usb_driver *udrv;

	if (dev->type != &usb_interface_type) {
		return VMM_ENODEV;
	}

	intf = to_usb_interface(dev);
	udrv = to_usb_driver(dev->driver);

	if (udrv->disconnect) {
		udrv->disconnect(intf);
		err = VMM_OK;
	} else {
		err = VMM_ENODEV;
	}

	return err;
}

struct vmm_bus usb_bus_type = {
	.name = "usb",
	.match = usb_bus_match,
	.probe = usb_bus_probe,
	.remove = usb_bus_remove,
};
VMM_EXPORT_SYMBOL(usb_bus_type);
