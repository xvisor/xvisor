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
 * @file hcd.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface to the USB Root Hub support.
 */

#ifndef __USB_HUB_H_
#define __USB_HUB_H_

#include <vmm_types.h>
#include <libs/list.h>
#include <drv/usb/ch11.h>

/** Representation of Hub instance */
struct usb_hub_device {
	struct dlist head;
	bool configured;
	struct usb_device *dev;
	struct usb_hub_descriptor desc;
};

/**
 * usb_get_device_state - get device's current state (usbcore, hcds)
 * @udev: pointer to device whose state is needed
 *
 * Note: This can be called from any context.
 */
enum usb_device_state usb_get_device_state(struct usb_device *udev);

/**
 * usb_set_device_state - change a device's current state (usbcore, hcds)
 * @udev: pointer to device whose state should be changed
 * @new_state: new state value to be stored
 *
 * udev->state is _not_ fully protected by the device lock.  Although
 * most transitions are made only while holding the lock, the state can
 * can change to USB_STATE_NOTATTACHED at almost any time.  This
 * is so that devices can be marked as disconnected as soon as possible,
 * without having to wait for any semaphores to be released.  As a result,
 * all changes to any device's state must be protected by the
 * device_state_lock spinlock.
 *
 * Once a device has been added to the device tree, all changes to its state
 * should be made using this routine.  The state should _not_ be set directly.
 *
 * If udev->state is already USB_STATE_NOTATTACHED then no change is made.
 * Otherwise udev->state is set to new_state, and if new_state is
 * USB_STATE_NOTATTACHED then all of udev's descendants' states are also set
 * to USB_STATE_NOTATTACHED.
 *
 * Note: This can be called from any context.
 */
void usb_set_device_state(struct usb_device *udev,
		enum usb_device_state new_state);

/**
 * usb_alloc_device - usb device constructor (usbcore-internal)
 * @parent: hub to which device is connected; null to allocate a root hub
 * @hcd: pointer to the HCD representing the controller
 * @port: one-based index of port; ignored for root hubs
 *
 * Only hub drivers (including virtual root hub drivers for host
 * controllers) should ever call this.
 *
 * Note: This should be called from Thread (or Orphan) context.
 */
struct usb_device *usb_alloc_device(struct usb_device *parent,
				    struct usb_hcd *hcd, unsigned port);

/**
 * usb_ref_device - increment reference count of usb device
 * @dev: the usb_device structure of existing device
 *
 * Note: This can be called from any context.
 */
void usb_ref_device(struct usb_device *dev);

/**
 * usb_dref_device - de-refernce the usb device
 * @dev: the usb_device structure of existing device
 *
 * Note: This should be called from Thread (or Orphan) context.
 */
void usb_dref_device(struct usb_device *dev);

/**
 * usb_new_device - enumerate newly found usb device
 * @dev: the usb_device structure of new device
 *
 * Only hub drivers (including virtual root hub drivers for host
 * controllers) should ever call this.
 *
 * When calling this function the usb device should be in one of 
 * the following states:
 *
 * USB_STATE_ATTACHED
 * USB_STATE_POWERED
 * USB_STATE_RECONNECTING
 * USB_STATE_UNAUTHENTICATED
 * USB_STATE_DEFAULT
 * USB_STATE_ADDRESS
 *
 * If required use usb_set_device_state() before calling this function.
 *
 * Note: This can be called from any context.
 */
int usb_new_device(struct usb_device *dev);

/**
 * usb_disconnect - disconnect and free-up the usb device
 * @dev: the usb_device structure to disconnect
 *
 * Only hub drivers (including virtual root hub drivers for host
 * controllers) should ever call this.
 *
 * The usb device should be in USB_STATE_NOTATTACHED when this
 * function is called hence use usb_set_device_state() before 
 * calling this function
 *
 * Note: This can be called from any context.
 */
int usb_disconnect(struct usb_device *dev);

/**
 * usb_hub_find_child - find child device of hub (usbcore, hcds)
 * @hdev: pointer to device whose state is needed
 * @port1: port number to look for
 *
 * Note: This can be called from any context.
 */
struct usb_device *usb_hub_find_child(struct usb_device *hdev, int port1);

/**
 * usb_hub_for_each_child - iterate over all child devices on the hub
 * @hdev:  USB device belonging to the usb hub
 * @port1: portnum associated with child device
 * @child: child device pointer
 */
#define usb_hub_for_each_child(hdev, port1, child) \
	for (port1 = 1,	child =	usb_hub_find_child(hdev, port1); \
		port1 <= hdev->maxchild; \
		child = usb_hub_find_child(hdev, ++port1))

/** 
 * usb_hub_init - Initialize USB Hub framework
 *
 * Note: This function is called at module init time and should 
 * not be called directly.
 */
int usb_hub_init(void);

/** 
 * usb_hub_exit - Exit USB Hub framework
 *
 * Note: This function is called at module exit time and should 
 * not be called directly.
 */
void usb_hub_exit(void);

#endif /* __USB_HCD_H_ */
