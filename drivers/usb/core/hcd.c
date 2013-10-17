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
 * @file hcd.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for USB host controller driver framework
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <arch_atomic.h>
#include <libs/bitops.h>
#include <libs/stringlib.h>
#include <drv/usb.h>
#include <drv/usb/hcd.h>
#include <drv/usb/hub.h>

/*
 * Protected list of usb host controllers.
 */
static DEFINE_MUTEX(usb_hcd_list_lock);
static LIST_HEAD(usb_hcd_list);
static u32 usb_hcd_count;

static DEFINE_SPINLOCK(hcd_root_hub_lock);

int usb_hcd_submit_urb(struct urb *urb)
{
	int			status;
	struct usb_hcd		*hcd = urb->dev->hcd;

	/* increment urb's reference count as part of giving it to the HCD
	 * (which will control it).  HCD guarantees that it either returns
	 * an error or calls giveback(), but not both.
	 */
	usb_ref_urb(urb);

	status = hcd->driver->urb_enqueue(hcd, urb);
	if (unlikely(status)) {
		INIT_LIST_HEAD(&urb->urb_list);
		usb_free_urb(urb);
	}

	return status;
}
VMM_EXPORT_SYMBOL(usb_hcd_submit_urb);

int usb_hcd_unlink_urb(struct urb *urb, int status)
{
	struct usb_hcd		*hcd = urb->dev->hcd;
	int			retval = VMM_OK;

	/* Prevent the device and bus from going away while
	 * the unlink is carried out.  If they are already gone
	 * then urb->use_count must be 0, since disconnected
	 * devices can't have any active URBs.
	 */
	usb_ref_device(urb->dev);

	/* The only reason an HCD might fail this call is if
	 * it has not yet fully queued the urb to begin with.
	 * Such failures should be harmless. 
	 */
	retval = hcd->driver->urb_dequeue(hcd, urb, status);

	usb_free_device(urb->dev);

	return retval;
}
VMM_EXPORT_SYMBOL(usb_hcd_unlink_urb);

void usb_hcd_giveback_urb(struct usb_hcd *hcd, struct urb *urb, int status)
{
	urb->hcpriv = NULL;
	INIT_LIST_HEAD(&urb->urb_list);

	/* pass ownership to the completion handler */
	urb->status = status;

	urb->complete(urb);

	usb_free_urb(urb);
}
VMM_EXPORT_SYMBOL(usb_hcd_giveback_urb);

vmm_irq_return_t usb_hcd_irq(int irq, void *__hcd)
{
	struct usb_hcd		*hcd = __hcd;
	irq_flags_t		flags;
	vmm_irq_return_t	rc;

	arch_cpu_irq_save(flags);

	if (unlikely(HCD_DEAD(hcd) || !HCD_HW_ACCESSIBLE(hcd))) {
		rc = VMM_IRQ_NONE;
	} else if (hcd->driver->irq(hcd) == VMM_IRQ_NONE) {
		rc = VMM_IRQ_NONE;
	} else {
		rc = VMM_IRQ_HANDLED;
	}

	arch_cpu_irq_restore(flags);

	return rc;
}
VMM_EXPORT_SYMBOL(usb_hcd_irq);

struct usb_hcd *usb_create_hcd(const struct hc_driver *driver,
			       struct vmm_device *dev, const char *bus_name)
{
	struct usb_hcd *hcd;

	hcd = vmm_zalloc(sizeof(*hcd) + driver->hcd_priv_size);
	if (!hcd) {
		vmm_printf("%s: hcd alloc failed\n", dev->node->name);
		return NULL;
	}

	INIT_LIST_HEAD(&hcd->head);
	arch_atomic_write(&hcd->refcnt, 1);
	hcd->dev = dev;
	hcd->bus_name = bus_name;

	hcd->driver = driver;
	hcd->speed = driver->flags & HCD_MASK;
	hcd->product_desc = (driver->product_desc) ? driver->product_desc :
			"USB Host Controller";

	INIT_SPIN_LOCK(&hcd->devicemap_lock);
	memset(&hcd->devicemap, 0, sizeof(hcd->devicemap));

	return hcd;
}
VMM_EXPORT_SYMBOL(usb_create_hcd);

static int usb_hcd_request_irqs(struct usb_hcd *hcd,
		unsigned int irqnum, unsigned long irqflags)
{
	int rc;

	if (hcd->driver->irq) {
		vmm_snprintf(hcd->irq_descr, sizeof(hcd->irq_descr), "%s:usb%d",
				hcd->driver->description, hcd->bus_num);
		rc = vmm_host_irq_register(irqnum, hcd->irq_descr, 
					   &usb_hcd_irq, hcd);
		if (rc != 0) {
			vmm_printf("%s: request interrupt %d failed\n",
				   hcd->dev->node->name, irqnum);
			return rc;
		}
		hcd->irq = irqnum;
		vmm_printf("%s: %s 0x%08llx\n", hcd->dev->node->name,
				(hcd->driver->flags & HCD_MEMORY) ?
					"io mem" : "io base",
					(unsigned long long)hcd->rsrc_start);
	} else {
		hcd->irq = 0;
		if (hcd->rsrc_start) {
			vmm_printf("%s: %s 0x%08llx\n", hcd->dev->node->name,
					(hcd->driver->flags & HCD_MEMORY) ?
					"io mem" : "io base",
					(unsigned long long)hcd->rsrc_start);
		}
	}

	return VMM_OK;
}

/**
 * register_root_hub - called by usb_add_hcd() to register a root hub
 * @hcd: host controller for this root hub
 *
 * This function registers the root hub with the USB subsystem.  It sets up
 * the device properly in the device tree and then calls usb_new_device()
 * to register the usb device.  It also assigns the root hub's USB address
 * (always 1).
 */
static int register_root_hub(struct usb_hcd *hcd)
{
	struct usb_device *usb_dev = hcd->root_hub;
	int retval;

	usb_set_device_state(usb_dev, USB_STATE_ADDRESS);

	vmm_mutex_lock(&usb_hcd_list_lock);

	retval = usb_new_device(usb_dev);
	if (retval) {
		vmm_printf("%s: can't register root hub for %s, %d\n",
			   __func__, hcd->dev->node->name, retval);
	} else {
		vmm_spin_lock_irq (&hcd_root_hub_lock);
		hcd->rh_registered = 1;
		vmm_spin_unlock_irq (&hcd_root_hub_lock);

		/* Did the HC die before the root hub was registered? */
		if (HCD_DEAD(hcd))
			usb_hcd_died(hcd);	/* This time clean up */
	}
	vmm_mutex_unlock(&usb_hcd_list_lock);

	return retval;
}

int usb_add_hcd(struct usb_hcd *hcd,
		unsigned int irqnum, unsigned long irqflags)
{
	int retval;
	struct dlist *l;
	struct usb_hcd *thcd;
	struct usb_device *rhdev;

	vmm_printf("%s: %s\n", hcd->dev->node->name, hcd->product_desc);

	vmm_mutex_lock(&usb_hcd_list_lock);
	list_for_each(l, &usb_hcd_list) {
		thcd = list_entry(l, struct usb_hcd, head);
		if (strcmp(hcd->bus_name, thcd->bus_name) == 0) {
			vmm_printf("%s: bus_name=%s alread registered\n",
				   hcd->dev->node->name, hcd->bus_name);
			vmm_mutex_unlock(&usb_hcd_list_lock);
			return VMM_EEXIST;
		}
	}
	hcd->bus_num = usb_hcd_count;
	usb_hcd_count++;
	list_add_tail(&hcd->head, &usb_hcd_list);
	vmm_mutex_unlock(&usb_hcd_list_lock);

	set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);

	if ((rhdev = usb_alloc_device(NULL, hcd, 0)) == NULL) {
		vmm_printf("%s: unable to allocate root hub\n", 
			   hcd->dev->node->name);
		retval = VMM_ENOMEM;
		goto err_allocate_root_hub;
	}
	hcd->root_hub = rhdev;

	switch (hcd->speed) {
	case HCD_USB11:
		rhdev->speed = USB_SPEED_FULL;
		break;
	case HCD_USB2:
		rhdev->speed = USB_SPEED_HIGH;
		break;
	case HCD_USB3:
		rhdev->speed = USB_SPEED_SUPER;
		break;
	default:
		retval = VMM_EINVALID;
		goto err_set_rh_speed;
	}

	/* HCD_FLAG_RH_RUNNING doesn't matter until the root hub is
	 * registered.  But since the controller can die at any time,
	 * let's initialize the flag before touching the hardware.
	 */
	set_bit(HCD_FLAG_RH_RUNNING, &hcd->flags);

	/* "reset" is misnamed; its role is now one-time init. the controller
	 * should already have been reset (and boot firmware kicked off etc).
	 */
	if (hcd->driver->reset && (retval = hcd->driver->reset(hcd)) < 0) {
		vmm_printf("%s: can't setup\n", hcd->dev->node->name);
		goto err_hcd_driver_setup;
	}
	hcd->rh_pollable = 1;

	/* Enable irqs just before we start the controller */
	if (irqnum) {
		retval = usb_hcd_request_irqs(hcd, irqnum, irqflags);
		if (retval)
			goto err_request_irq;
	}

	/* Mark HCD as running */
	hcd->state = HC_STATE_RUNNING;
	retval = hcd->driver->start(hcd);
	if (retval < 0) {
		vmm_printf("%s: startup error %d\n", 
					hcd->dev->node->name, retval);
		goto err_hcd_driver_start;
	}

	/* Starting here, usbcore will pay attention to this root hub */
	rhdev->bus_mA = min(500u, hcd->power_budget);
	if ((retval = register_root_hub(hcd)) != 0)
		goto err_register_root_hub;

	return VMM_OK;

err_register_root_hub:
	hcd->rh_pollable = 0;
	clear_bit(HCD_FLAG_POLL_RH, &hcd->flags);
	hcd->driver->stop(hcd);
	hcd->state = HC_STATE_HALT;
	clear_bit(HCD_FLAG_POLL_RH, &hcd->flags);
err_hcd_driver_start:
	if (hcd->irq > 0)
		vmm_host_irq_unregister(irqnum, hcd);
err_request_irq:
err_hcd_driver_setup:
err_set_rh_speed:
	usb_free_device(hcd->root_hub);
err_allocate_root_hub:
	return retval;
}
VMM_EXPORT_SYMBOL(usb_add_hcd);

void usb_hcd_died(struct usb_hcd *hcd)
{
	irq_flags_t flags;

	vmm_printf("%s: HC died; cleaning up\n", hcd->dev->node->name);

	vmm_spin_lock_irqsave (&hcd_root_hub_lock, flags);
	clear_bit(HCD_FLAG_RH_RUNNING, &hcd->flags);
	set_bit(HCD_FLAG_DEAD, &hcd->flags);
	if (hcd->rh_registered) {
		clear_bit(HCD_FLAG_POLL_RH, &hcd->flags);

		/* make hubd clean up old urbs and devices */
		usb_set_device_state(hcd->root_hub,
				USB_STATE_NOTATTACHED);
		usb_disconnect(hcd->root_hub);
		hcd->root_hub = NULL;
	}
	vmm_spin_unlock_irqrestore (&hcd_root_hub_lock, flags);
	/* Make sure that the other roothub is also deallocated. */
}
VMM_EXPORT_SYMBOL(usb_hcd_died);

void usb_remove_hcd(struct usb_hcd *hcd)
{
	struct usb_device *rhdev = hcd->root_hub;

	vmm_printf("%s: remove, state %x\n", hcd->dev->node->name, hcd->state);

	clear_bit(HCD_FLAG_RH_RUNNING, &hcd->flags);
	if (HC_IS_RUNNING (hcd->state))
		hcd->state = HC_STATE_QUIESCING;

	vmm_spin_lock_irq (&hcd_root_hub_lock);
	hcd->rh_registered = 0;
	vmm_spin_unlock_irq (&hcd_root_hub_lock);

	vmm_mutex_lock(&usb_hcd_list_lock);
	usb_disconnect(rhdev);
	hcd->root_hub = NULL;
	vmm_mutex_unlock(&usb_hcd_list_lock);

	hcd->rh_pollable = 0;
	clear_bit(HCD_FLAG_POLL_RH, &hcd->flags);

	hcd->driver->stop(hcd);
	hcd->state = HC_STATE_HALT;

	/* In case the HCD restarted the timer, stop it again. */
	clear_bit(HCD_FLAG_POLL_RH, &hcd->flags);

	vmm_mutex_lock(&usb_hcd_list_lock);
	list_del(&hcd->head);
	usb_hcd_count--;
	vmm_mutex_unlock(&usb_hcd_list_lock);
}
VMM_EXPORT_SYMBOL(usb_remove_hcd);

void usb_ref_hcd(struct usb_hcd *hcd)
{
	arch_atomic_add(&hcd->refcnt, 1);
}
VMM_EXPORT_SYMBOL(usb_ref_hcd);

void usb_destroy_hcd(struct usb_hcd *hcd)
{
	if (arch_atomic_sub_return(&hcd->refcnt, 1)) {
		return;
	}

	vmm_free(hcd);
}
VMM_EXPORT_SYMBOL(usb_destroy_hcd);

void usb_hcd_shutdown(struct usb_hcd *hcd)
{
	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}
VMM_EXPORT_SYMBOL(usb_hcd_shutdown);

int __init usb_hcd_init(void)
{
	return VMM_OK;
}

void __exit usb_hcd_exit(void)
{
}

