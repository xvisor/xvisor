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
 * @file usb.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface to the USB core framework.
 */

#ifndef __USB_CORE_H_
#define __USB_CORE_H_

#include <vmm_limits.h>
#include <vmm_types.h>
#include <vmm_macros.h>
#include <vmm_spinlocks.h>
#include <vmm_notifier.h>
#include <vmm_devdrv.h>
#include <libs/list.h>
#include <drv/usb/ch9.h>

#define USB_CORE_IPRIORITY		(1)

extern struct vmm_bus usb_bus_type;
extern struct vmm_device_type usb_device_type;
extern struct vmm_device_type usb_interface_type;

/* FIXME:
 * The EHCI spec says that we must align to at least 32 bytes.  However,
 * some platforms require larger alignment.
 */
#if ARCH_DMA_MINALIGN > 32
#define USB_DMA_MINALIGN	ARCH_DMA_MINALIGN
#else
#define USB_DMA_MINALIGN	32
#endif

/* Everything is aribtrary */
#define USB_ALTSETTINGALLOC		4
#define USB_MAXALTSETTING		128	/* Hard limit */

#define USB_MAX_DEVICE			127
#define USB_MAXCONFIG			8
#define USB_MAXINTERFACES		8
#define USB_MAXENDPOINTS		16
#define USB_MAXCHILDREN			8	/* This is arbitrary */
#define USB_MAX_HUB			16

#define USB_CNTL_TIMEOUT 100 /* 100ms timeout */

/*
 * This is the timeout to allow for submitting an urb in ms. We allow more
 * time for a BULK device to react - some are slow.
 */
#define USB_TIMEOUT_MS(pipe) (usb_pipebulk(pipe) ? 5000 : 1000)

struct usb_device_id;
struct usb_device;
struct usb_driver;
struct usb_hcd;
struct urb;

/* Interface */
struct usb_interface {
	struct vmm_device dev;

	struct usb_interface_descriptor desc;

	unsigned char	no_of_ep;
	unsigned char	num_altsetting;
	unsigned char	act_altsetting;

	struct usb_endpoint_descriptor ep_desc[USB_MAXENDPOINTS];
	/*
	 * Super Speed Device will have Super Speed Endpoint
	 * Companion Descriptor  (section 9.6.7 of usb 3.0 spec)
	 * Revision 1.0 June 6th 2011
	 */
	struct usb_ss_ep_comp_descriptor ss_ep_comp_desc[USB_MAXENDPOINTS];
} __attribute__ ((packed));

#define	to_usb_interface(_i) container_of((_i), struct usb_interface, dev)
#define interface_set_data(_i, _p) vmm_devdrv_set_data(&(_i)->dev, (_p))
#define interface_get_data(_i) vmm_devdrv_get_data(&(_i)->dev)

/* Configuration information.. */
struct usb_config {
	struct usb_config_descriptor desc;

	unsigned char	no_of_intf;	/* number of interfaces */
	struct usb_interface intf[USB_MAXINTERFACES];
} __attribute__ ((packed));

enum {
	/* Maximum packet size; encoded as 0,1,2,3 = 8,16,32,64 */
	PACKET_SIZE_8   = 0,
	PACKET_SIZE_16  = 1,
	PACKET_SIZE_32  = 2,
	PACKET_SIZE_64  = 3,
};

/*-------------------------------------------------------------------*
 *                    USB device support                             *
 *-------------------------------------------------------------------*/

struct usb_device {
	struct usb_device *parent;
	struct vmm_device dev;

	char	devpath[VMM_FIELD_NAME_SIZE];
	u32	route;
	u8	portnum;
	u8	level;

	struct usb_hcd *hcd;

	/*
	 * Child devices -  if this is a hub device
	 * Each instance needs its own set of data structures.
	 */
	int maxchild;			/* Number of ports if hub */
	vmm_spinlock_t children_lock;
	struct usb_device *children[USB_MAXCHILDREN];

	u8	devnum;		/* Device number on USB bus */
	u16	bus_mA;
	enum usb_device_state	state;
	enum usb_device_speed	speed;
	u64 active_duration;

	char	manufacturer[32];	/* manufacturer */
	char	product[32];		/* product */
	char	serial[32];		/* serial number */

	/* Maximum packet size; one of: PACKET_SIZE_* */
	int maxpacketsize;
	/* one bit for each endpoint ([0] = IN, [1] = OUT) */
	unsigned int toggle[2];
	/* endpoint halts; one bit per endpoint # & direction;
	 * [0] = IN, [1] = OUT
	 */
	unsigned int halted[2];
	int epmaxpacketin[16];		/* INput endpoint specific maximums */
	int epmaxpacketout[16];		/* OUTput endpoint specific maximums */

	int configno;			/* selected config number */

	/* Device Descriptor */
	struct usb_device_descriptor descriptor
		__attribute__((aligned(USB_DMA_MINALIGN)));
	struct usb_config config; /* config descriptor */

	int have_langid;		/* whether string_langid is valid yet */
	int string_langid;		/* language ID for strings */
	int (*irq_handle)(struct usb_device *dev);
	unsigned long irq_status;
	int irq_act_len;		/* transfered bytes */
};

#define	to_usb_device(_d)	container_of((_d), struct usb_device, dev)
#define interface_to_usbdev(_i)	to_usb_device((_i)->dev.parent)

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
 * usb_find_child - find child device of a usb device (usbcore, hcds)
 * @hdev: USB device whose child is needed
 * @port1: port number to look for
 *
 * Note: This can be called from any context.
 */
struct usb_device *usb_find_child(struct usb_device *hdev, int port1);

/**
 * Get USB2 HUB address and port of given usb device (usbcore, hcds)
 * @dev: USB device whose parent address and port is needed
 * @hub_addr: address of hub
 * @hub_port: port of hub
 *
 * Note: This can be called from any context.
 */
int usb_get_usb2_hub_address_port(struct usb_device *dev,
				  u8 *hub_addr, u8 *hub_port);

/**
 * usb_hub_for_each_child - iterate over all child devices
 * @hdev:  parent USB device
 * @port1: portnum associated with child device
 * @child: child device pointer
 */
#define usb_for_each_child(hdev, port1, child) \
	for (port1 = 1,	child =	usb_find_child(hdev, port1); \
		port1 <= hdev->maxchild; \
		child = usb_find_child(hdev, ++port1))

/*-------------------------------------------------------------------*
 *                    USB device driver support                      *
 *-------------------------------------------------------------------*/

/**
 * struct usb_device_id - identifies USB devices for probing and hotplugging
 * @match_flags: Bit mask controlling of the other fields are used to match
 *	against new devices.  Any field except for driver_info may be used,
 *	although some only make sense in conjunction with other fields.
 *	This is usually set by a USB_DEVICE_*() macro, which sets all
 *	other fields in this structure except for driver_info.
 * @idVendor: USB vendor ID for a device; numbers are assigned
 *	by the USB forum to its members.
 * @idProduct: Vendor-assigned product ID.
 * @bcdDevice_lo: Low end of range of vendor-assigned product version numbers.
 *	This is also used to identify individual product versions, for
 *	a range consisting of a single device.
 * @bcdDevice_hi: High end of version number range.  The range of product
 *	versions is inclusive.
 * @bDeviceClass: Class of device; numbers are assigned
 *	by the USB forum.  Products may choose to implement classes,
 *	or be vendor-specific.  Device classes specify behavior of all
 *	the interfaces on a devices.
 * @bDeviceSubClass: Subclass of device; associated with bDeviceClass.
 * @bDeviceProtocol: Protocol of device; associated with bDeviceClass.
 * @bInterfaceClass: Class of interface; numbers are assigned
 *	by the USB forum.  Products may choose to implement classes,
 *	or be vendor-specific.  Interface classes specify behavior only
 *	of a given interface; other interfaces may support other classes.
 * @bInterfaceSubClass: Subclass of interface; associated with bInterfaceClass.
 * @bInterfaceProtocol: Protocol of interface; associated with bInterfaceClass.
 * @driver_info: Holds information used by the driver.  Usually it holds
 *	a pointer to a descriptor understood by the driver, or perhaps
 *	device flags.
 *
 * In most cases, drivers will create a table of device IDs by using
 * USB_DEVICE(), or similar macros designed for that purpose.
 * They will then export it to userspace using MODULE_DEVICE_TABLE(),
 * and provide it to the USB core through their usb_driver structure.
 *
 * See the usb_match_id() function for information about how matches are
 * performed.  Briefly, you will normally use one of several macros to help
 * construct these entries.  Each entry you provide will either identify
 * one or more specific products, or will identify a class of products
 * which have agreed to behave the same.  You should put the more specific
 * matches towards the beginning of your table, so that driver_info can
 * record quirks of specific products.
 */
struct usb_device_id {
	/* which fields to match against? */
	u16		match_flags;

	/* Used for product specific matches; range is inclusive */
	u16		idVendor;
	u16		idProduct;
	u16		bcdDevice_lo;
	u16		bcdDevice_hi;

	/* Used for device class matches */
	u8		bDeviceClass;
	u8		bDeviceSubClass;
	u8		bDeviceProtocol;

	/* Used for interface class matches */
	u8		bInterfaceClass;
	u8		bInterfaceSubClass;
	u8		bInterfaceProtocol;

	/* Used for vendor-specific interface matches */
	u8		bInterfaceNumber;

	/* not matched against */
	unsigned long	driver_info;
};

/* Some useful macros to use to create struct usb_device_id */
#define USB_DEVICE_ID_MATCH_VENDOR		0x0001
#define USB_DEVICE_ID_MATCH_PRODUCT		0x0002
#define USB_DEVICE_ID_MATCH_DEV_LO		0x0004
#define USB_DEVICE_ID_MATCH_DEV_HI		0x0008
#define USB_DEVICE_ID_MATCH_DEV_CLASS		0x0010
#define USB_DEVICE_ID_MATCH_DEV_SUBCLASS	0x0020
#define USB_DEVICE_ID_MATCH_DEV_PROTOCOL	0x0040
#define USB_DEVICE_ID_MATCH_INT_CLASS		0x0080
#define USB_DEVICE_ID_MATCH_INT_SUBCLASS	0x0100
#define USB_DEVICE_ID_MATCH_INT_PROTOCOL	0x0200
#define USB_DEVICE_ID_MATCH_INT_NUMBER		0x0400

#define USB_DEVICE_ID_MATCH_DEVICE \
		(USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT)
#define USB_DEVICE_ID_MATCH_DEV_RANGE \
		(USB_DEVICE_ID_MATCH_DEV_LO | USB_DEVICE_ID_MATCH_DEV_HI)
#define USB_DEVICE_ID_MATCH_DEVICE_AND_VERSION \
		(USB_DEVICE_ID_MATCH_DEVICE | USB_DEVICE_ID_MATCH_DEV_RANGE)
#define USB_DEVICE_ID_MATCH_DEV_INFO \
		(USB_DEVICE_ID_MATCH_DEV_CLASS | \
		USB_DEVICE_ID_MATCH_DEV_SUBCLASS | \
		USB_DEVICE_ID_MATCH_DEV_PROTOCOL)
#define USB_DEVICE_ID_MATCH_INT_INFO \
		(USB_DEVICE_ID_MATCH_INT_CLASS | \
		USB_DEVICE_ID_MATCH_INT_SUBCLASS | \
		USB_DEVICE_ID_MATCH_INT_PROTOCOL)

/**
 * USB_DEVICE - macro used to describe a specific usb device
 * @vend: the 16 bit USB Vendor ID
 * @prod: the 16 bit USB Product ID
 *
 * This macro is used to create a struct usb_device_id that matches a
 * specific device.
 */
#define USB_DEVICE(vend, prod) \
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE, \
	.idVendor = (vend), \
	.idProduct = (prod)
/**
 * USB_DEVICE_VER - describe a specific usb device with a version range
 * @vend: the 16 bit USB Vendor ID
 * @prod: the 16 bit USB Product ID
 * @lo: the bcdDevice_lo value
 * @hi: the bcdDevice_hi value
 *
 * This macro is used to create a struct usb_device_id that matches a
 * specific device, with a version range.
 */
#define USB_DEVICE_VER(vend, prod, lo, hi) \
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE_AND_VERSION, \
	.idVendor = (vend), \
	.idProduct = (prod), \
	.bcdDevice_lo = (lo), \
	.bcdDevice_hi = (hi)

/**
 * USB_DEVICE_INTERFACE_PROTOCOL - describe a usb device with a specific interface protocol
 * @vend: the 16 bit USB Vendor ID
 * @prod: the 16 bit USB Product ID
 * @pr: bInterfaceProtocol value
 *
 * This macro is used to create a struct usb_device_id that matches a
 * specific interface protocol of devices.
 */
#define USB_DEVICE_INTERFACE_PROTOCOL(vend, prod, pr) \
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE | \
		       USB_DEVICE_ID_MATCH_INT_PROTOCOL, \
	.idVendor = (vend), \
	.idProduct = (prod), \
	.bInterfaceProtocol = (pr)

/**
 * USB_DEVICE_INTERFACE_NUMBER - describe a usb device with a specific interface number
 * @vend: the 16 bit USB Vendor ID
 * @prod: the 16 bit USB Product ID
 * @num: bInterfaceNumber value
 *
 * This macro is used to create a struct usb_device_id that matches a
 * specific interface number of devices.
 */
#define USB_DEVICE_INTERFACE_NUMBER(vend, prod, num) \
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE | \
		       USB_DEVICE_ID_MATCH_INT_NUMBER, \
	.idVendor = (vend), \
	.idProduct = (prod), \
	.bInterfaceNumber = (num)

/**
 * USB_DEVICE_INFO - macro used to describe a class of usb devices
 * @cl: bDeviceClass value
 * @sc: bDeviceSubClass value
 * @pr: bDeviceProtocol value
 *
 * This macro is used to create a struct usb_device_id that matches a
 * specific class of devices.
 */
#define USB_DEVICE_INFO(cl, sc, pr) \
	.match_flags = USB_DEVICE_ID_MATCH_DEV_INFO, \
	.bDeviceClass = (cl), \
	.bDeviceSubClass = (sc), \
	.bDeviceProtocol = (pr)

/**
 * USB_INTERFACE_INFO - macro used to describe a class of usb interfaces
 * @cl: bInterfaceClass value
 * @sc: bInterfaceSubClass value
 * @pr: bInterfaceProtocol value
 *
 * This macro is used to create a struct usb_device_id that matches a
 * specific class of interfaces.
 */
#define USB_INTERFACE_INFO(cl, sc, pr) \
	.match_flags = USB_DEVICE_ID_MATCH_INT_INFO, \
	.bInterfaceClass = (cl), \
	.bInterfaceSubClass = (sc), \
	.bInterfaceProtocol = (pr)

/**
 * USB_DEVICE_AND_INTERFACE_INFO - describe a specific usb device with a class of usb interfaces
 * @vend: the 16 bit USB Vendor ID
 * @prod: the 16 bit USB Product ID
 * @cl: bInterfaceClass value
 * @sc: bInterfaceSubClass value
 * @pr: bInterfaceProtocol value
 *
 * This macro is used to create a struct usb_device_id that matches a
 * specific device with a specific class of interfaces.
 *
 * This is especially useful when explicitly matching devices that have
 * vendor specific bDeviceClass values, but standards-compliant interfaces.
 */
#define USB_DEVICE_AND_INTERFACE_INFO(vend, prod, cl, sc, pr) \
	.match_flags = USB_DEVICE_ID_MATCH_INT_INFO \
		| USB_DEVICE_ID_MATCH_DEVICE, \
	.idVendor = (vend), \
	.idProduct = (prod), \
	.bInterfaceClass = (cl), \
	.bInterfaceSubClass = (sc), \
	.bInterfaceProtocol = (pr)

/**
 * USB_VENDOR_AND_INTERFACE_INFO - describe a specific usb vendor with a class of usb interfaces
 * @vend: the 16 bit USB Vendor ID
 * @cl: bInterfaceClass value
 * @sc: bInterfaceSubClass value
 * @pr: bInterfaceProtocol value
 *
 * This macro is used to create a struct usb_device_id that matches a
 * specific vendor with a specific class of interfaces.
 *
 * This is especially useful when explicitly matching devices that have
 * vendor specific bDeviceClass values, but standards-compliant interfaces.
 */
#define USB_VENDOR_AND_INTERFACE_INFO(vend, cl, sc, pr) \
	.match_flags = USB_DEVICE_ID_MATCH_INT_INFO \
		| USB_DEVICE_ID_MATCH_VENDOR, \
	.idVendor = (vend), \
	.bInterfaceClass = (cl), \
	.bInterfaceSubClass = (sc), \
	.bInterfaceProtocol = (pr)

/* Stuff for dynamic usb ids */
struct usb_dynids {
	vmm_spinlock_t lock;
	struct dlist list;
};

struct usb_dynid {
	struct dlist node;
	struct usb_device_id id;
};

/**
 * struct usb_driver - identifies USB interface driver to usbcore
 * @drv: used internally to register to device driver framework.
 * @dynids: used internally to hold the list of dynamically added device
 *	ids for this driver.
 * @name: The driver name should be unique among USB drivers,
 *	and should normally be the same as the module name.
 * @probe: Called to see if the driver is willing to manage a particular
 *	interface on a device.  If it is, probe returns zero and uses
 *	usb_set_intfdata() to associate driver-specific data with the
 *	interface.  It may also use usb_set_interface() to specify the
 *	appropriate altsetting.  If unwilling to manage the interface,
 *	return VMM_ENODEV, if genuine IO errors occurred, an appropriate
 *	negative errno value.
 * @disconnect: Called when the interface is no longer accessible, usually
 *	because its device has been (or is being) disconnected or the
 *	driver module is being unloaded.
 * @pre_reset: Called by usb_reset_device() when the device is about to be
 *	reset.  This routine must not return until the driver has no active
 *	URBs for the device, and no more URBs may be submitted until the
 *	post_reset method is called.
 * @post_reset: Called by usb_reset_device() after the device
 *	has been reset
 * @id_table: USB drivers use ID table to support hotplugging.
 *	This must be set or your driver's probe function will never get called.
 * @no_dynamic_id: if set to 1, the USB core will not allow dynamic ids to be
 *	added to this driver by preventing the sysfs file from being created.
 * @supports_autosuspend: if set to 0, the USB core will not allow autosuspend
 *	for interfaces bound to this driver.
 * @soft_unbind: if set to 1, the USB core will not kill URBs and disable
 *	endpoints before calling the driver's disconnect method.
 * @disable_hub_initiated_lpm: if set to 0, the USB core will not allow hubs
 *	to initiate lower power link state transitions when an idle timeout
 *	occurs.  Device-initiated USB 3.0 link PM will still be allowed.
 *
 * USB interface drivers must provide a name, probe() and disconnect()
 * methods, and an id_table.  Other driver fields are optional.
 *
 * The id_table is used in hotplugging.  It holds a set of descriptors,
 * and specialized data may be associated with each entry.  That table
 * is used by both user and kernel mode hotplugging support.
 *
 * The probe() and disconnect() methods are called in a context where
 * they can sleep.  The disconnect code needs to address concurrency 
 * issues with respect to open() and close() methods, as well as forcing 
 * all pending I/O requests to complete (by unlinking them as necessary, 
 * and blocking until the unlinks complete).
 */
struct usb_driver {
	struct vmm_driver drv;
	struct usb_dynids dynids;

	const char *name;

	int (*probe) (struct usb_interface *intf,
		      const struct usb_device_id *id);
	void (*disconnect) (struct usb_interface *intf);
	int (*pre_reset)(struct usb_interface *intf);
	int (*post_reset)(struct usb_interface *intf);

	const struct usb_device_id *id_table;

	unsigned int no_dynamic_id:1;
	unsigned int supports_autosuspend:1;
	unsigned int disable_hub_initiated_lpm:1;
	unsigned int soft_unbind:1;
};

#define	to_usb_driver(_d) container_of((_d), struct usb_driver, drv)

/**
 * usb_match_one_id - Try to match usb_device_id with usb interface
 * @intf: the interface of interest
 * @id: array of usb_device_id structures, terminated by zero entry
 *
 * Note: This can be called from any context.
 */
bool usb_match_one_id(struct usb_interface *intf,
		      const struct usb_device_id *id);

/**
 * usb_match_id - find first usb_device_id matching device or interface
 * @intf: the interface of interest
 * @id: array of usb_device_id structures, terminated by zero entry
 *
 * usb_match_id searches an array of usb_device_id's and returns
 * the first one matching the device or interface, or null.
 * This is used when binding (or rebinding) a driver to an interface.
 * Most USB device drivers will use this indirectly, through the usb core,
 * but some layered driver framework use it directly.
 *
 * Note: This can be called from any context.
 */
const struct usb_device_id *usb_match_id(struct usb_interface *intf,
					 const struct usb_device_id *id);

/**
 * usb_interface_match - match interface with given usb driver and
 *                       return matching usb device id
 * @intf: the interface of interest
 * @drv: usb driver to match
 *
 * Note: This can be called from any context.
 */
const struct usb_device_id *usb_match_interface(struct usb_interface *intf,
						struct usb_driver *drv);

/**
 * usb_add_dynid - add dynamic id to usb driver
 * @drv: usb driver instance
 * @idVendor: usb device vendor 
 * @idProduct: usb device product 
 * @bInterfaceClass: usb device interface class 
 *
 * Note: This can be called from any context.
 */
int usb_add_dynid(struct usb_driver *driver,
		  u32 idVendor, u32 idProduct, u32 bInterfaceClass);

/**
 * usb_del_dynid - delete dynamic id from usb driver
 * @drv: usb driver instance
 * @idVendor: usb device vendor 
 * @idProduct: usb device product 
 *
 * Note: This can be called from any context.
 */
int usb_del_dynid(struct usb_driver *driver,
		  u32 idVendor, u32 idProduct);

/**
 * usb_pre_reset_driver - pre reset usb driver for given usb interface
 * @intf: interface of a usb device instance
 * @drv: usb driver connected to this interface
 *
 * Note: This should be called from Thread (or Orphan) context.
 */
int usb_pre_reset_driver(struct usb_interface *intf, 
			 struct usb_driver *drv);

/**
 * usb_post_reset_driver - post reset usb driver for given usb interface
 * @intf: interface of a usb device instance
 * @drv: usb driver connected to this interface
 *
 * Note: This should be called from Thread (or Orphan) context.
 */
int usb_post_reset_driver(struct usb_interface *intf, 
			  struct usb_driver *drv);

/**
 * usb_register - register a new usb driver 
 * @driver: usb driver instance to register
 *
 * Note: Use this in module_init()
 * Note: This should be called from Thread (or Orphan) context.
 */
int usb_register(struct usb_driver *driver);

/**
 * usb_deregister - de-register/remove a usb driver 
 * @driver: usb driver instance to deregister
 *
 * Note: Use this in module_exit()
 * Note: This should be called from Thread (or Orphan) context.
 */
void usb_deregister(struct usb_driver *driver);

/*-------------------------------------------------------------------*
 *          URB support, for asynchronous request completions        *
 *-------------------------------------------------------------------*/

typedef void (*usb_complete_t)(struct urb *);

struct urb {
	/* Reference count of this urb */
	atomic_t refcnt;

	/* List head for use by current owner of this urb */
	struct dlist urb_list;

	/* Release function */
	void (*release)(struct urb *);

	/* Parameters for doing this urb */
	struct usb_device *dev;
	u32 pipe;
	u8 *setup_packet;
	void *transfer_buffer;
	u32 transfer_buffer_length;
	u32 actual_length;
	int start_frame;
	int number_of_packets;
	int interval;

	/* Completion context of this urb */
	void *context;
	usb_complete_t complete;

	/* Return code for this urb */
	int status;

	/* Private context of HCD for this urb */
	void *hcpriv;
};

/**
 * usb_fill_control_urb - initializes a control urb
 * @urb: pointer to the urb to initialize.
 * @dev: pointer to the struct usb_device for this urb.
 * @pipe: the endpoint pipe
 * @setup_packet: pointer to the setup_packet buffer
 * @transfer_buffer: pointer to the transfer buffer
 * @buffer_length: length of the transfer buffer
 * @complete_fn: pointer to the usb_complete_t function
 * @context: what to set the urb context to.
 *
 * Initializes a control urb with the proper information needed to submit
 * it to a device.
 */
static inline void usb_fill_control_urb(struct urb *urb,
					struct usb_device *dev,
					u32 pipe,
					u8 *setup_packet,
					void *transfer_buffer,
					int buffer_length,
					usb_complete_t complete_fn,
					void *context)
{
	urb->dev = dev;
	urb->pipe = pipe;
	urb->setup_packet = setup_packet;
	urb->transfer_buffer = transfer_buffer;
	urb->transfer_buffer_length = buffer_length;
	urb->complete = complete_fn;
	urb->context = context;
}

/**
 * usb_fill_bulk_urb - macro to help initialize a bulk urb
 * @urb: pointer to the urb to initialize.
 * @dev: pointer to the struct usb_device for this urb.
 * @pipe: the endpoint pipe
 * @transfer_buffer: pointer to the transfer buffer
 * @buffer_length: length of the transfer buffer
 * @complete_fn: pointer to the usb_complete_t function
 * @context: what to set the urb context to.
 *
 * Initializes a bulk urb with the proper information needed to submit it
 * to a device.
 */
static inline void usb_fill_bulk_urb(struct urb *urb,
				     struct usb_device *dev,
				     u32 pipe,
				     void *transfer_buffer,
				     int buffer_length,
				     usb_complete_t complete_fn,
				     void *context)
{
	urb->dev = dev;
	urb->pipe = pipe;
	urb->transfer_buffer = transfer_buffer;
	urb->transfer_buffer_length = buffer_length;
	urb->complete = complete_fn;
	urb->context = context;
}

/**
 * usb_fill_int_urb - macro to help initialize a interrupt urb
 * @urb: pointer to the urb to initialize.
 * @dev: pointer to the struct usb_device for this urb.
 * @pipe: the endpoint pipe
 * @transfer_buffer: pointer to the transfer buffer
 * @buffer_length: length of the transfer buffer
 * @complete_fn: pointer to the usb_complete_t function
 * @context: what to set the urb context to.
 * @interval: what to set the urb interval to, encoded like
 *	the endpoint descriptor's bInterval value.
 *
 * Initializes a interrupt urb with the proper information needed to submit
 * it to a device.
 *
 * Note that High Speed and SuperSpeed interrupt endpoints use a logarithmic
 * encoding of the endpoint interval, and express polling intervals in
 * microframes (eight per millisecond) rather than in frames (one per
 * millisecond).
 *
 * Wireless USB also uses the logarithmic encoding, but specifies it in units of
 * 128us instead of 125us.  For Wireless USB devices, the interval is passed
 * through to the host controller, rather than being translated into microframe
 * units.
 */
static inline void usb_fill_int_urb(struct urb *urb,
				    struct usb_device *dev,
				    u32 pipe,
				    void *transfer_buffer,
				    int buffer_length,
				    usb_complete_t complete_fn,
				    void *context,
				    int interval)
{
	urb->dev = dev;
	urb->pipe = pipe;
	urb->transfer_buffer = transfer_buffer;
	urb->transfer_buffer_length = buffer_length;
	urb->complete = complete_fn;
	urb->context = context;
	if (dev->speed == USB_SPEED_HIGH || dev->speed == USB_SPEED_SUPER)
		urb->interval = 1 << (interval - 1);
	else
		urb->interval = interval;
	urb->start_frame = -1;
}

/**
 * usb_init_urb - initializes a urb so that it can be used by a USB driver
 * @urb: pointer to the urb to initialize
 *
 * Initializes a urb so that the USB subsystem can use it properly.
 *
 * If a urb is created with a call to usb_alloc_urb() it is not
 * necessary to call this function.  Only use this if you allocate the
 * space for a struct urb on your own.  If you call this function, be
 * careful when freeing the memory for your urb that it is no longer in
 * use by the USB core.
 *
 * Only use this function if you _really_ understand what you are doing.
 */
void usb_init_urb(struct urb *urb);

/**
 * usb_alloc_urb - creates a new urb for a USB driver to use
 *
 * Creates an urb for the USB driver to use, initializes a few internal
 * structures, incrementes the usage counter, and returns a pointer to it.
 *
 * If no memory is available, NULL is returned.
 *
 * The driver must call usb_free_urb() when it is finished with the urb.
 */
struct urb *usb_alloc_urb(void);

/**
 * usb_ref_urb - increments the reference count of the urb
 * @urb: pointer to the urb to modify, may be NULL
 *
 * This must be  called whenever a urb is transferred from a device driver to a
 * host controller driver.  This allows proper reference counting to happen
 * for urbs.
 *
 * A pointer to the urb with the incremented reference counter is returned.
 */
void usb_ref_urb(struct urb *urb);

/**
 * usb_free_urb - frees the memory used by a urb when all users of it are finished
 * @urb: pointer to the urb to free, may be NULL
 *
 * Must be called when a user of a urb is finished with it.  When the last user
 * of the urb calls this function, the memory of the urb is freed.
 *
 * Note: The transfer buffer associated with the urb is not freed unless the
 * URB_FREE_BUFFER transfer flag is set.
 */
void usb_free_urb(struct urb *urb);

/**
 * usb_submit_urb - submit URB
 * @urb: urb that is to be submitted.
 * @status: completion status code for the URB.
 *
 * Note: This can be called from any context.
 */
int usb_submit_urb(struct urb *urb);

/**
 * usb_unlink_urb - unlink a submitted URB
 * @urb: urb that is already submitted.
 * @status: completion status code for the URB.
 *
 * Note: This can be called from any context.
 */
int usb_unlink_urb(struct urb *urb, int status);

/*
 * Calling this entity a "pipe" is glorifying it. A USB pipe
 * is something embarrassingly simple: it basically consists
 * of the following information:
 *  - device number (7 bits)
 *  - endpoint number (4 bits)
 *  - current Data0/1 state (1 bit)
 *  - direction (1 bit)
 *  - speed (2 bits)
 *  - max packet size (2 bits: 8, 16, 32 or 64)
 *  - pipe type (2 bits: control, interrupt, bulk, isochronous)
 *
 * That's 18 bits. Really. Nothing more. And the USB people have
 * documented these eighteen bits as some kind of glorious
 * virtual data structure.
 *
 * Let's not fall in that trap. We'll just encode it as a simple
 * unsigned int. The encoding is:
 *
 *  - max size:		bits 0-1	(00 = 8, 01 = 16, 10 = 32, 11 = 64)
 *  - direction:	bit 7		(0 = Host-to-Device [Out],
 *					(1 = Device-to-Host [In])
 *  - device:		bits 8-14
 *  - endpoint:		bits 15-18
 *  - Data0/1:		bit 19
 *  - pipe type:	bits 30-31	(00 = isochronous, 01 = interrupt,
 *					 10 = control, 11 = bulk)
 *
 * Why? Because it's arbitrary, and whatever encoding we select is really
 * up to us. This one happens to share a lot of bit positions with the UHCI
 * specification, so that much of the uhci driver can just mask the bits
 * appropriately.
 */

#define USB_PIPE_ISOCHRONOUS	0
#define USB_PIPE_INTERRUPT	1
#define USB_PIPE_CONTROL	2
#define USB_PIPE_BULK		3

#define usb_packetid(pipe)	(((pipe) & USB_DIR_IN) ? USB_PID_IN : \
				 USB_PID_OUT)

#define usb_pipeout(pipe)	((((pipe) >> 7) & 1) ^ 1)
#define usb_pipein(pipe)	(((pipe) >> 7) & 1)
#define usb_pipedevice(pipe)	(((pipe) >> 8) & 0x7f)
#define usb_pipe_endpdev(pipe)	(((pipe) >> 8) & 0x7ff)
#define usb_pipeendpoint(pipe)	(((pipe) >> 15) & 0xf)
#define usb_pipedata(pipe)	(((pipe) >> 19) & 1)
#define usb_pipetype(pipe)	(((pipe) >> 30) & 3)
#define usb_pipeisoc(pipe)	(usb_pipetype((pipe)) == USB_PIPE_ISOCHRONOUS)
#define usb_pipeint(pipe)	(usb_pipetype((pipe)) == USB_PIPE_INTERRUPT)
#define usb_pipecontrol(pipe)	(usb_pipetype((pipe)) == USB_PIPE_CONTROL)
#define usb_pipebulk(pipe)	(usb_pipetype((pipe)) == USB_PIPE_BULK)

/* Create various pipes... */
#define usb_create_pipe(dev,endpoint) \
		(((dev)->devnum << 8) | ((endpoint) << 15) | \
		(dev)->maxpacketsize)
#define usb_default_pipe(dev) ((dev)->speed << 26)
#define usb_sndctrlpipe(dev, endpoint)	((USB_PIPE_CONTROL << 30) | \
					 usb_create_pipe(dev, endpoint))
#define usb_rcvctrlpipe(dev, endpoint)	((USB_PIPE_CONTROL << 30) | \
					 usb_create_pipe(dev, endpoint) | \
					 USB_DIR_IN)
#define usb_sndisocpipe(dev, endpoint)	((USB_PIPE_ISOCHRONOUS << 30) | \
					 usb_create_pipe(dev, endpoint))
#define usb_rcvisocpipe(dev, endpoint)	((USB_PIPE_ISOCHRONOUS << 30) | \
					 usb_create_pipe(dev, endpoint) | \
					 USB_DIR_IN)
#define usb_sndbulkpipe(dev, endpoint)	((USB_PIPE_BULK << 30) | \
					 usb_create_pipe(dev, endpoint))
#define usb_rcvbulkpipe(dev, endpoint)	((USB_PIPE_BULK << 30) | \
					 usb_create_pipe(dev, endpoint) | \
					 USB_DIR_IN)
#define usb_sndintpipe(dev, endpoint)	((USB_PIPE_INTERRUPT << 30) | \
					 usb_create_pipe(dev, endpoint))
#define usb_rcvintpipe(dev, endpoint)	((USB_PIPE_INTERRUPT << 30) | \
					 usb_create_pipe(dev, endpoint) | \
					 USB_DIR_IN)
#define usb_snddefctrl(dev)		((USB_PIPE_CONTROL << 30) | \
					 usb_default_pipe(dev))
#define usb_rcvdefctrl(dev)		((USB_PIPE_CONTROL << 30) | \
					 usb_default_pipe(dev) | \
					 USB_DIR_IN)

/* Endpoint halt control/status */
#define usb_endpoint_out(ep_dir)	(((ep_dir >> 7) & 1) ^ 1)
#define usb_endpoint_halt(dev, ep, out) ((dev)->halted[out] |= (1 << (ep)))
#define usb_endpoint_running(dev, ep, out) ((dev)->halted[out] &= ~(1 << (ep)))
#define usb_endpoint_halted(dev, ep, out) ((dev)->halted[out] & (1 << (ep)))

/*-------------------------------------------------------------------*
 *                         SYNCHRONOUS CALL SUPPORT                  *
 *-------------------------------------------------------------------*/

int usb_control_msg(struct usb_device *dev, u32 pipe,
		    u8 request, u8 requesttype, u16 value, u16 index,
		    void *data, u16 size, int *actual_length, int timeout);
int usb_interrupt_msg(struct usb_device *dev, u32 pipe,
		      void *data, int len, int interval);
int usb_bulk_msg(struct usb_device *dev, u32 pipe,
		 void *data, int len, int *actual_length,int timeout);

/* wrappers around usb_control_msg() for the most common standard requests */
int usb_maxpacket(struct usb_device *dev, u32 pipe);
int usb_get_descriptor(struct usb_device *dev, u8 desctype, 
		       u8 descindex, void *buf, int size);
int usb_string(struct usb_device *dev, int index, char *buf, size_t size);

/* wrappers that also update important state inside usbcore */
int usb_set_interface(struct usb_device *dev, int ifnum, int alternate);
int usb_get_configuration_no(struct usb_device *dev, u8 *buffer, int cfgno);
int usb_get_class_descriptor(struct usb_device *dev, int ifnum,
			     u8 type, u8 id, void *buf, u32 size);
int usb_clear_halt(struct usb_device *dev, u32 pipe);

/*-------------------------------------------------------------------*
 *                       NOTIFIER CLIENT SUPPORT                     *
 *-------------------------------------------------------------------*/

/* Events from the usb core */
#define USB_DEVICE_ADD		0x0001
#define USB_DEVICE_REMOVE	0x0002
#define USB_HCD_ADD		0x0003
#define USB_HCD_REMOVE		0x0004

/**
 * usb_register_notify - register a notifier callback whenever a usb change happens
 * @nb: pointer to the notifier block for the callback events.
 *
 * These changes are either USB devices or busses being added or removed.
 */
void usb_register_notify(struct vmm_notifier_block *nb);

/**
 * usb_unregister_notify - unregister a notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * usb_register_notify() must have been previously called for this function
 * to work properly.
 */
void usb_unregister_notify(struct vmm_notifier_block *nb);

/**
 * usb_notify_add_device - Inform notify listeners about new device
 * @udev: pointer to the usb device.
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
 * Note: This should be called from Thread (or Orphan) context.
 */
void usb_notify_add_device(struct usb_device *udev);

/**
 * usb_notify_remove_device - Inform notify listeners about removed device
 * @udev: pointer to the usb device.
 *
 * Only hub drivers (including virtual root hub drivers for host
 * controllers) should ever call this.
 *
 * The usb device should be in USB_STATE_NOTATTACHED when this
 * function is called hence use usb_set_device_state() before
 * calling this function
 *
 * Note: This should be called from Thread (or Orphan) context.
 */
void usb_notify_remove_device(struct usb_device *udev);

/**
 * usb_notify_add_hcd - Inform notify listeners about new hcd
 * @udev: pointer to the usb hcd.
 */
void usb_notify_add_hcd(struct usb_hcd *hcd);

/**
 * usb_notify_remove_hcd - Inform notify listeners about removed hcd
 * @udev: pointer to the usb hcd.
 */
void usb_notify_remove_hcd(struct usb_hcd *hcd);

#endif /* __USB_CORE_H_ */
