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
#include <drv/usb/hub.h>

/* Protected list of all usb devices. */
static DEFINE_MUTEX(usb_drv_list_lock);
static LIST_HEAD(usb_drv_list);
static u32 usb_drv_count;

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
VMM_EXPORT_SYMBOL(usb_interface_match);

int usb_add_dynid(struct usb_driver *driver,
		  u32 idVendor, u32 idProduct, u32 bInterfaceClass)
{
	int retval;
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

	retval = usb_hub_probe_driver(driver);
	if (retval)
		return retval;

	return VMM_OK;
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

int usb_probe_driver(struct usb_interface *intf)
{
	int err;
	struct dlist *l;
	struct usb_driver *drv;
	const struct usb_device_id *id;

	if (!intf) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&usb_drv_list_lock);

	list_for_each(l, &usb_drv_list) {
		drv = list_entry(l, struct usb_driver, head);
		id = usb_match_interface(intf, drv);
		if (id) {
			err = drv->probe(intf, id);
			vmm_mutex_unlock(&usb_drv_list_lock);
			return err;
		}
	}

	vmm_mutex_unlock(&usb_drv_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(usb_probe_driver);

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

void usb_disconnect_driver(struct usb_interface *intf, 
			   struct usb_driver *drv)
{
	if (!intf || !drv) {
		return;
	}

	drv->disconnect(intf);
}
VMM_EXPORT_SYMBOL(usb_disconnect_driver);

int usb_register(struct usb_driver *drv)
{
	struct dlist *l;
	struct usb_driver *tdrv;

	if (!drv) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&usb_drv_list_lock);

	list_for_each(l, &usb_drv_list) {
		tdrv = list_entry(l, struct usb_driver, head);
		if (strcmp(drv->name, tdrv->name) == 0) {
			vmm_printf("%s: driver=\"%s\" alread registered\n",
				   __func__, drv->name);
			vmm_mutex_unlock(&usb_drv_list_lock);
			return VMM_EEXIST;
		}
	}

	usb_drv_count++;
	INIT_LIST_HEAD(&drv->head);
	list_add_tail(&drv->head, &usb_drv_list);

	vmm_mutex_unlock(&usb_drv_list_lock);

	return usb_hub_probe_driver(drv);
}
VMM_EXPORT_SYMBOL(usb_register);

void usb_deregister(struct usb_driver *drv)
{
	if (!drv) {
		return;
	}

	vmm_mutex_lock(&usb_drv_list_lock);

	usb_hub_disconnect_driver(drv);

	list_del(&drv->head);

	vmm_mutex_unlock(&usb_drv_list_lock);
}
VMM_EXPORT_SYMBOL(usb_deregister);

