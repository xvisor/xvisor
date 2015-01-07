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
 * @file notify.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of usb notify APIs
 */

#include <vmm_error.h>
#include <vmm_modules.h>
#include <drv/usb.h>

static BLOCKING_NOTIFIER_CHAIN(usb_notifier_list);

void usb_register_notify(struct vmm_notifier_block *nb)
{
	vmm_blocking_notifier_register(&usb_notifier_list, nb);
}
VMM_EXPORT_SYMBOL(usb_register_notify);

void usb_unregister_notify(struct vmm_notifier_block *nb)
{
	vmm_blocking_notifier_unregister(&usb_notifier_list, nb);
}
VMM_EXPORT_SYMBOL(usb_unregister_notify);

void usb_notify_add_device(struct usb_device *udev)
{
	vmm_blocking_notifier_call(&usb_notifier_list, USB_DEVICE_ADD, udev);
}
VMM_EXPORT_SYMBOL(usb_notify_add_device);

void usb_notify_remove_device(struct usb_device *udev)
{
	vmm_blocking_notifier_call(&usb_notifier_list,
				   USB_DEVICE_REMOVE, udev);
}
VMM_EXPORT_SYMBOL(usb_notify_remove_device);

void usb_notify_add_hcd(struct usb_hcd *hcd)
{
	vmm_blocking_notifier_call(&usb_notifier_list, USB_HCD_ADD, hcd);
}
VMM_EXPORT_SYMBOL(usb_notify_add_hcd);

void usb_notify_remove_hcd(struct usb_hcd *hcd)
{
	vmm_blocking_notifier_call(&usb_notifier_list, USB_HCD_REMOVE, hcd);
}
VMM_EXPORT_SYMBOL(usb_notify_remove_hcd);
