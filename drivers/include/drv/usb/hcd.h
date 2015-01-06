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
 * @brief Interface to the USB host controller driver framework.
 */

#ifndef __USB_HCD_H_
#define __USB_HCD_H_

#include <vmm_types.h>
#include <vmm_timer.h>
#include <vmm_spinlocks.h>
#include <vmm_host_irq.h>
#include <vmm_devdrv.h>
#include <libs/list.h>

struct usb_hcd {
	/*
	 * housekeeping
	 */
	struct dlist		head;		/* hcd is-a bus */
	atomic_t		refcnt;		/* hcd reference count */
	struct vmm_device 	*dev;
	const char		*product_desc;	/* product/vendor string */
	int			speed;		/* Speed for this roothub. */
	char			irq_descr[24];	/* driver + bus # */

	u32			bus_num;
	const char		*bus_name;

	vmm_spinlock_t		devicemap_lock;
	unsigned long devicemap[128 / (8*sizeof(unsigned long))];

	/*
	 * root hub device
	 */
	struct usb_device	*root_hub;

	/*
	 * hardware info/state
	 */
	const struct hc_driver	*driver;	/* hw-specific hooks */

	/* Flags that need to be manipulated atomically because they can
	 * change while the host controller is running.  Always use
	 * set_bit() or clear_bit() to change their values.
	 */
	unsigned long		flags;
#define HCD_FLAG_HW_ACCESSIBLE		0	/* at full power */
#define HCD_FLAG_POLL_RH		2	/* poll for rh status? */
#define HCD_FLAG_POLL_PENDING		3	/* status has changed? */
#define HCD_FLAG_WAKEUP_PENDING		4	/* root hub is resuming? */
#define HCD_FLAG_RH_RUNNING		5	/* root hub is running? */
#define HCD_FLAG_DEAD			6	/* controller has died? */

	/* The flags can be tested using these macros; they are likely to
	 * be slightly faster than test_bit().
	 */
#define HCD_HW_ACCESSIBLE(hcd)	((hcd)->flags & (1U << HCD_FLAG_HW_ACCESSIBLE))
#define HCD_POLL_RH(hcd)	((hcd)->flags & (1U << HCD_FLAG_POLL_RH))
#define HCD_POLL_PENDING(hcd)	((hcd)->flags & (1U << HCD_FLAG_POLL_PENDING))
#define HCD_WAKEUP_PENDING(hcd)	((hcd)->flags & (1U << HCD_FLAG_WAKEUP_PENDING))
#define HCD_RH_RUNNING(hcd)	((hcd)->flags & (1U << HCD_FLAG_RH_RUNNING))
#define HCD_DEAD(hcd)		((hcd)->flags & (1U << HCD_FLAG_DEAD))

	/* Flags that get set only during HCD registration or removal. */
	unsigned		rh_registered:1;/* is root hub registered? */
	unsigned		rh_pollable:1;	/* may we poll the root hub? */
	unsigned		msix_enabled:1;	/* driver has MSI-X enabled? */

	/* The next flag is a stopgap, to be removed when all the HCDs
	 * support the new root-hub polling mechanism. */
	unsigned		uses_new_polling:1;
	unsigned		wireless:1;	/* Wireless USB HCD */
	unsigned		authorized_default:1;
	unsigned		has_tt:1;	/* Integrated TT in root hub */

	unsigned int		irq;		/* irq allocated */
	void 			*regs;		/* device memory/io */
	physical_addr_t		rsrc_start;	/* memory/io resource start */
	physical_size_t		rsrc_len;	/* memory/io resource length */
	unsigned		power_budget;	/* in mA, 0 = no limit */

	int			state;
#	define	__ACTIVE		0x01
#	define	__SUSPEND		0x04
#	define	__TRANSIENT		0x80

#	define	HC_STATE_HALT		0
#	define	HC_STATE_RUNNING	(__ACTIVE)
#	define	HC_STATE_QUIESCING	(__SUSPEND|__TRANSIENT|__ACTIVE)
#	define	HC_STATE_RESUMING	(__SUSPEND|__TRANSIENT)
#	define	HC_STATE_SUSPENDED	(__SUSPEND)

#define	HC_IS_RUNNING(state) ((state) & __ACTIVE)
#define	HC_IS_SUSPENDED(state) ((state) & __SUSPEND)

	/* The HC driver's private data is stored at the end of
	 * this structure.
	 */
	unsigned long hcd_priv[0]
			__attribute__ ((aligned(sizeof(s64))));
};

struct hc_driver {
	const char	*description;	/* "ehci-hcd" etc */
	const char	*product_desc;	/* product/vendor string */
	size_t		hcd_priv_size;	/* size of private data */

	int	flags;
#define	HCD_MEMORY	0x0001		/* HC regs use memory (else I/O) */
#define	HCD_LOCAL_MEM	0x0002		/* HC needs local memory */
#define	HCD_SHARED	0x0004		/* Two (or more) usb_hcds share HW */
#define	HCD_USB11	0x0010		/* USB 1.1 */
#define	HCD_USB2	0x0020		/* USB 2.0 */
#define	HCD_USB3	0x0040		/* USB 3.0 */
#define	HCD_MASK	0x0070

	/* irq handler */
	vmm_irq_return_t	(*irq) (struct usb_hcd *hcd);

	/* called to init HCD and root hub */
	int	(*reset) (struct usb_hcd *hcd);
	int	(*start) (struct usb_hcd *hcd);

	/* cleanly make HCD stop writing memory and doing I/O */
	void	(*stop) (struct usb_hcd *hcd);

	/* shutdown HCD */
	void	(*shutdown) (struct usb_hcd *hcd);

	/* manage i/o requests, device state */
	int	(*urb_enqueue)(struct usb_hcd *hcd,
				struct urb *urb);
	int	(*urb_dequeue)(struct usb_hcd *hcd,
				struct urb *urb, int status);

	/* xHCI specific functions */
		/* Called by usb_alloc_dev to alloc HC device structures */
	int	(*alloc_dev)(struct usb_hcd *, struct usb_device *);
		/* Called by usb_disconnect to free HC device structures */
	void	(*free_dev)(struct usb_hcd *, struct usb_device *);
};

/**
 * usb_hcd_priv - Get HCD driver private context
 * @hcd: pointer to the HCD representing the controller
 * 
 * Note: This can be called from any context.
 */
static inline void *usb_hcd_priv(struct usb_hcd *hcd)
{
	return &hcd->hcd_priv[0];
}

/**
 * usb_hcd_submit_urb - submit URB to HCD
 * @urb: urb that is to be submitted.
 * @status: completion status code for the URB.
 *
 * Note: This can be called from any context.
 */
int usb_hcd_submit_urb(struct urb *urb);

/**
 * usb_hcd_unlink_urb - unlink a submitted URB from HCD
 * @urb: urb that is already submitted.
 * @status: completion status code for the URB.
 *
 * Note: This can be called from any context.
 */
int usb_hcd_unlink_urb(struct urb *urb, int status);

/**
 * usb_hcd_giveback_urb - return URB from HCD to device driver
 * @hcd: host controller returning the URB
 * @urb: urb being returned to the USB device driver.
 * @status: completion status code for the URB.
 *
 * This hands the URB from HCD to its USB device driver, using its
 * completion function.  The HCD has freed all per-urb resources
 * (and is done using urb->hcpriv).  It also released all HCD locks;
 * the device driver won't cause problems if it frees, modifies,
 * or resubmits this URB.
 */
void usb_hcd_giveback_urb(struct usb_hcd *hcd, struct urb *urb, int status);

/**
 * usb_hcd_poll_rh_status - Poll for Root Hub status
 * @hcd: pointer to the HCD representing the controller
 * 
 * Root Hub interrupt transfers are polled using a timer if the
 * driver requests it; otherwise the driver is responsible for
 * calling usb_hcd_poll_rh_status() when an event occurs.
 *
 * Note: This can be called from any context.
 */
void usb_hcd_poll_rh_status(struct usb_hcd *hcd);

/**
 * usb_create_hcd - create and initialize an HCD structure
 * @driver: HC driver that will use this hcd
 * @dev: device for this HC, stored in hcd->self.controller
 * @bus_name: value to store in hcd->self.bus_name
 *
 * Allocate a struct usb_hcd, with extra space at the end for the
 * HC driver's private data.  Initialize the generic members of the
 * hcd structure.
 *
 * If memory is unavailable, returns NULL.
 *
 * Note: This can be called from any context.
 */
struct usb_hcd *usb_create_hcd(const struct hc_driver *driver,
		struct vmm_device *dev, const char *bus_name);

/**
 * usb_add_hcd - finish generic HCD structure initialization and register
 * @hcd: the usb_hcd structure to initialize
 * @irqnum: Interrupt line to allocate
 * @irqflags: Interrupt type flags
 *
 * Finish the remaining parts of generic HCD initialization: allocate the
 * buffers of consistent memory, register the bus, request the IRQ line,
 * and call the driver's reset() and start() routines.
 *
 * Note: This should be called from Thread (or Orphan) context.
 */
int usb_add_hcd(struct usb_hcd *hcd,
		unsigned int irqnum, unsigned long irqflags);

/**
 * usb_hcd_died - report abnormal shutdown of a host controller (bus glue)
 * @hcd: pointer to the HCD representing the controller
 *
 * This is called by bus glue to report a USB host controller that died
 * while operations may still have been pending.  It's called automatically
 * by the PCI glue, so only glue for non-PCI busses should need to call it.
 *
 * Note: This can be called from any context.
 */
void usb_hcd_died(struct usb_hcd *hcd);

/**
 * usb_remove_hcd - shutdown processing for generic HCDs
 * @hcd: the usb_hcd structure to remove
 *
 * Disconnects the root hub, then reverses the effects of usb_add_hcd(),
 * invoking the HCD's stop() method.
 *
 * Note: This should be called from Thread (or Orphan) context.
 */
void usb_remove_hcd(struct usb_hcd *hcd);

/**
 * usb_ref_hcd - increment reference count of generic HCD structure
 * @hcd: pointer to the HCD representing the controller
 *
 * Note: This can be called from any context.
 */
void usb_ref_hcd(struct usb_hcd *hcd);

/**
 * usb_dref_hcd - de-refernce generic HCD structure
 * @hcd: pointer to the HCD representing the controller
 *
 * Note: This can be called from any context.
 */
void usb_dref_hcd(struct usb_hcd *hcd);

/**
 * usb_hcd_shutdown - shutdown generic HCD structure
 * @hcd: pointer to the HCD representing the controller
 *
 * Note: This can be called from any context.
 */
void usb_hcd_shutdown(struct usb_hcd *hcd);

/* The D0/D1 toggle bits ... USE WITH CAUTION (they're almost hcd-internal) */
#define usb_gettoggle(dev, ep, out) (((dev)->toggle[out] >> (ep)) & 1)
#define	usb_dotoggle(dev, ep, out)  ((dev)->toggle[out] ^= (1 << (ep)))
#define usb_settoggle(dev, ep, out, bit) \
		((dev)->toggle[out] = ((dev)->toggle[out] & ~(1 << (ep))) | \
		 ((bit) << (ep)))

/** 
 * usb_hcd_init - Initialize generic HCD framework
 *
 * Note: This function is called at module init time and should 
 * not be called directly.
 */
int usb_hcd_init(void);

/** 
 * usb_hcd_exit - Exit generic HCD framework
 *
 * Note: This function is called at module exit time and should 
 * not be called directly.
 */
void usb_hcd_exit(void);

#endif /* __USB_HCD_H_ */
