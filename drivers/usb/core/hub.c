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
 * @file hub.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for USB hub device framework
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_cache.h>
#include <vmm_delay.h>
#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_mutex.h>
#include <vmm_completion.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <arch_atomic.h>

#include <libs/unaligned.h>
#include <libs/bitops.h>
#include <libs/stringlib.h>

#include <drv/usb.h>
#include <drv/usb/ch11.h>

#undef DEBUG

#if defined(DEBUG)
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define USB_BUFSIZ			512

#ifndef USB_HUB_MIN_POWER_ON_DELAY
#define USB_HUB_MIN_POWER_ON_DELAY	100
#endif

/** Representation of Hub instance */
struct usb_hub_device {
	struct dlist head;
	bool configured;
	struct usb_device *dev;
	struct usb_interface *intf;
	struct usb_hub_descriptor desc;
};

/*
 * ==================== USB Hub Worker Routines ====================
 */

/* Protected Hub work list and Hub worker thread */
static struct vmm_thread *usb_hub_worker_thread;
static LIST_HEAD(usb_hub_work_list);
static DEFINE_SPINLOCK(usb_hub_work_list_lock);
static DECLARE_COMPLETION(usb_hub_work_avail);

/* Hub work instance */
struct usb_hub_work {
	struct dlist head;
	bool freeup;
	int (*work_func)(struct usb_hub_work *);
	struct usb_device *dev;
};

static void usb_hub_init_work(struct usb_hub_work *work,
			      int (*func)(struct usb_hub_work *),
			      bool freeup,
			      struct usb_device *dev)
{
	INIT_LIST_HEAD(&work->head);
	work->work_func = func;
	work->freeup = freeup;
	work->dev = dev;
}

static struct usb_hub_work *usb_hub_alloc_work(
				int (*func)(struct usb_hub_work *),
				struct usb_device *dev)
{
	struct usb_hub_work *work;

	work = vmm_zalloc(sizeof(struct usb_hub_work));
	if (!work) {
		return NULL;
	}

	usb_hub_init_work(work, func, TRUE, dev);

	return work;
}

static void usb_hub_free_work(struct usb_hub_work *work)
{
	if (!work || !work->freeup) {
		return;
	}

	vmm_free(work);
}

static void usb_hub_queue_work(struct usb_hub_work *work)
{
	irq_flags_t flags;

	if (!work) {
		return;
	}

	if (!work->work_func) {
		usb_hub_free_work(work);
		return;
	}

	vmm_spin_lock_irqsave(&usb_hub_work_list_lock, flags);

	INIT_LIST_HEAD(&work->head);
	list_add_tail(&work->head, &usb_hub_work_list);

	vmm_spin_unlock_irqrestore(&usb_hub_work_list_lock, flags);

	vmm_completion_complete(&usb_hub_work_avail);
};

static void usb_hub_flush_all_work(void)
{
	irq_flags_t flags;
	struct dlist *l;
	struct usb_hub_work *work;

	vmm_spin_lock_irqsave(&usb_hub_work_list_lock, flags);

	while (!list_empty(&usb_hub_work_list)) {
		l = list_pop(&usb_hub_work_list);
		work = list_entry(l, struct usb_hub_work, head);

		usb_hub_free_work(work);
	}

	vmm_spin_unlock_irqrestore(&usb_hub_work_list_lock, flags);
}

static void usb_hub_flush_dev_work(struct usb_device *dev)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct usb_hub_work *work;

	while (1) {
		found = FALSE;
		work = NULL;

		vmm_spin_lock_irqsave(&usb_hub_work_list_lock, flags);

		list_for_each(l, &usb_hub_work_list) {
			work = list_entry(l, struct usb_hub_work, head);
			if (work->dev == dev) {
				list_del(&work->head);
				found = TRUE;
				break;
			}
		}

		vmm_spin_unlock_irqrestore(&usb_hub_work_list_lock, flags);

		if (!found) {
			break;
		}

		usb_hub_free_work(work);
	}
}

static int usb_hub_worker_main(void *data)
{
	int err;
	irq_flags_t flags;
	struct dlist *l;
	struct usb_hub_work *work;

	while (1) {
		vmm_completion_wait(&usb_hub_work_avail);

		work = NULL;
		vmm_spin_lock_irqsave(&usb_hub_work_list_lock, flags);
		if (!list_empty(&usb_hub_work_list)) {
			l = list_pop(&usb_hub_work_list);
			work = list_entry(l, struct usb_hub_work, head);
		}
		vmm_spin_unlock_irqrestore(&usb_hub_work_list_lock, flags);

		if (!work) {
			continue;
		}

		err = work->work_func(work);
		if (err) {
			vmm_printf("%s: Work failed (error %d)\n",
				   __func__, err);
		}

		usb_hub_free_work(work);
	}

	return VMM_OK;
}

/*
 * ==================== USB Hub Managment Routines ====================
 */

/* Protected list of all usb hubs. */
static DEFINE_MUTEX(usb_hub_list_lock);
static LIST_HEAD(usb_hub_list);

static struct usb_hub_device *usb_hub_alloc(void)
{
	struct usb_hub_device *hub;

	hub = vmm_zalloc(sizeof(struct usb_hub_device));
	if (!hub) {
		return NULL;
	}

	INIT_LIST_HEAD(&hub->head);
	hub->configured = FALSE;

	return hub;
}

static void usb_hub_add(struct usb_hub_device *hub)
{
	vmm_mutex_lock(&usb_hub_list_lock);
	usb_ref_device(hub->dev);
	list_add_tail(&hub->head, &usb_hub_list);
	vmm_mutex_unlock(&usb_hub_list_lock);
}

static void usb_hub_remove(struct usb_hub_device *hub)
{
	vmm_mutex_lock(&usb_hub_list_lock);
	list_del(&hub->head);
	usb_dref_device(hub->dev);
	vmm_mutex_unlock(&usb_hub_list_lock);
}

static struct usb_hub_device *usb_hub_find(struct usb_device *dev,
					   struct usb_interface *intf)
{
	struct dlist *l;
	struct usb_hub_device *thub, *hub = NULL;

	vmm_mutex_lock(&usb_hub_list_lock);

	list_for_each(l, &usb_hub_list) {
		thub = list_entry(l, struct usb_hub_device, head);
		if (thub->dev == dev) {
			hub = thub;
			break;
		}
	}

	vmm_mutex_unlock(&usb_hub_list_lock);

	return hub;
}

static struct usb_hub_device *usb_hub_get(int index)
{
	struct dlist *l;
	struct usb_hub_device *thub, *hub = NULL;

	if (index < 0) {
		return NULL;
	}

	vmm_mutex_lock(&usb_hub_list_lock);

	list_for_each(l, &usb_hub_list) {
		thub = list_entry(l, struct usb_hub_device, head);
		if (!index) {
			hub = thub;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&usb_hub_list_lock);

	return hub;
}

static u32 usb_hub_count(void)
{
	u32 ret = 0;
	struct dlist *l;

	vmm_mutex_lock(&usb_hub_list_lock);

	list_for_each(l, &usb_hub_list) {
		ret++;
	}

	vmm_mutex_unlock(&usb_hub_list_lock);

	return ret;
}

static void usb_hub_free(struct usb_hub_device *hub)
{
	vmm_free(hub);
}

/*
 * ==================== USB Device Helper Routines ====================
 */

static void show_string(struct usb_device *udev, char *id, char *string)
{
	if (!string)
		return;
	vmm_printf("%s: %s = %s\n", udev->dev.name, id, string);
}

static void usb_announce_device(struct usb_device *udev)
{
	vmm_printf("%s: New USB device found, idVendor=%04x, idProduct=%04x\n",
		   udev->dev.name, vmm_le16_to_cpu(udev->descriptor.idVendor),
		   vmm_le16_to_cpu(udev->descriptor.idProduct));
	show_string(udev, "Product", udev->product);
	show_string(udev, "Manufacturer", udev->manufacturer);
	show_string(udev, "SerialNumber", udev->serial);
}

static int usb_set_address(struct usb_device *dev, u32 addr)
{
	DPRINTF("%s: set address %d\n", __func__, dev->devnum);
	return usb_control_msg(dev, usb_snddefctrl(dev),
				USB_REQ_SET_ADDRESS, 0,
				addr, 0, NULL, 0, NULL, USB_CNTL_TIMEOUT);
}

static int usb_set_configuration(struct usb_device *dev, int configuration)
{
	int res;

	DPRINTF("%s: set configuration %d\n", __func__, configuration);

	/* set setup command */
	res = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				USB_REQ_SET_CONFIGURATION, 0,
				configuration, 0,
				NULL, 0, NULL, USB_CNTL_TIMEOUT);
	if (res) {
		return res;
	}
	
	dev->toggle[0] = 0;
	dev->toggle[1] = 0;

	return VMM_OK;
}

/*
 * The routine usb_set_maxpacket_ep() is extracted from the loop of routine
 * usb_set_maxpacket(), because the optimizer of GCC 4.x chokes on this routine
 * when it is inlined in 1 single routine. What happens is that the register r3
 * is used as loop-count 'i', but gets overwritten later on.
 * This is clearly a compiler bug, but it is easier to workaround it here than
 * to update the compiler (Occurs with at least several GCC 4.{1,2},x
 * CodeSourcery compilers like e.g. 2007q3, 2008q1, 2008q3 lite editions on ARM)
 *
 * NOTE: Similar behaviour was observed with GCC4.6 on ARMv5.
 */
static void usb_set_maxpacket_ep(struct usb_device *dev, int if_idx, int ep_idx)
{
	int b;
	u16 ep_wMaxPacketSize;
	struct usb_endpoint_descriptor *ep;

	ep = &dev->config.intf[if_idx].ep_desc[ep_idx];

	b = ep->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
	ep_wMaxPacketSize = get_unaligned(&ep->wMaxPacketSize);

	if ((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
					USB_ENDPOINT_XFER_CONTROL) {
		/* Control => bidirectional */
		dev->epmaxpacketout[b] = ep_wMaxPacketSize;
		dev->epmaxpacketin[b] = ep_wMaxPacketSize;
		DPRINTF("%s: ##Control EP epmaxpacketout/in[%d] = %d\n",
			__func__, b, dev->epmaxpacketin[b]);
	} else {
		if ((ep->bEndpointAddress & 0x80) == 0) {
			/* OUT Endpoint */
			if (ep_wMaxPacketSize > dev->epmaxpacketout[b]) {
				dev->epmaxpacketout[b] = ep_wMaxPacketSize;
				DPRINTF("%s: ##EP epmaxpacketout[%d] = %d\n",
					__func__, b, dev->epmaxpacketout[b]);
			}
		} else {
			/* IN Endpoint */
			if (ep_wMaxPacketSize > dev->epmaxpacketin[b]) {
				dev->epmaxpacketin[b] = ep_wMaxPacketSize;
				DPRINTF("%s: ##EP epmaxpacketin[%d] = %d\n",
					__func__, b, dev->epmaxpacketin[b]);
			}
		} /* if out */
	} /* if control */
}

/*
 * set the max packed value of all endpoints in the given configuration
 */
static int usb_set_maxpacket(struct usb_device *dev)
{
	int i, ii;

	for (i = 0; i < dev->config.desc.bNumInterfaces; i++) {
		for (ii = 0;
		     ii < dev->config.intf[i].desc.bNumEndpoints;
		     ii++) {
			usb_set_maxpacket_ep(dev, i, ii);
		}
	}

	return VMM_OK;
}

/*
 * Parse the config, located in buffer, and fills the dev->config structure.
 * Note that all little/big endian swapping are done automatically.
 */
static int usb_parse_config(struct usb_device *dev, u8 *buffer, int cfgno)
{
	u16 ep_wMaxPacketSize;
	int index, ifno, epno, curr_if_num;
	struct usb_descriptor_header *head;
	struct usb_endpoint_descriptor *ep_desc;
	struct usb_interface *ifp = NULL;

	ifno = -1;
	epno = -1;
	curr_if_num = -1;

	dev->configno = cfgno;
	head = (struct usb_descriptor_header *) &buffer[0];
	if (head->bDescriptorType != USB_DT_CONFIG) {
		vmm_printf("%s: Invalid USB_CONFIG_DESC type=0x%x\n",
			   __func__, head->bDescriptorType);
		return VMM_EINVALID;
	}
	memcpy(&dev->config, buffer, buffer[0]);
	dev->config.desc.wTotalLength =
			vmm_le16_to_cpu(dev->config.desc.wTotalLength);
	dev->config.no_of_intf = 0;

	/* Ok the first entry must be a configuration entry,
	 * now process the others */
	index = dev->config.desc.bLength;
	head = (struct usb_descriptor_header *) &buffer[index];
	while ((index + 1) < dev->config.desc.wTotalLength) {
		switch (head->bDescriptorType) {
		case USB_DT_INTERFACE:
			if (((struct usb_interface_descriptor *) \
			    &buffer[index])->bInterfaceNumber != curr_if_num) {
				/* this is a new interface, copy new desc */
				ifno = dev->config.no_of_intf;
				ifp = &dev->config.intf[ifno];
				vmm_devdrv_initialize_device(&ifp->dev);
				vmm_snprintf(ifp->dev.name,
					     sizeof(ifp->dev.name),
					     "%s-intf%d", dev->dev.name, ifno);
				ifp->dev.parent = &dev->dev;
				ifp->dev.bus = &usb_bus_type;
				ifp->dev.type = &usb_interface_type;
				dev->config.no_of_intf++;
				memcpy(&ifp->desc, &buffer[index],
					sizeof(ifp->desc));
				ifp->no_of_ep = 0;
				ifp->num_altsetting = 1;
				curr_if_num = ifp->desc.bInterfaceNumber;
			} else {
				/* found alternate setting for the interface */
				if (ifno >= 0) {
					ifp = &dev->config.intf[ifno];
					ifp->num_altsetting++;
				}
			}
			break;
		case USB_DT_ENDPOINT:
			epno = dev->config.intf[ifno].no_of_ep;
			ifp = &dev->config.intf[ifno];
			ep_desc = &ifp->ep_desc[epno];
			/* found an endpoint */
			ifp->no_of_ep++;
			memcpy(ep_desc, &buffer[index], sizeof(*ep_desc));
			ep_wMaxPacketSize =
				get_unaligned(&ep_desc->wMaxPacketSize);
			put_unaligned(vmm_le16_to_cpu(ep_wMaxPacketSize),
					&ep_desc->wMaxPacketSize);
			DPRINTF("%s: ifnum=%d ep=%d\n", __func__, ifno, epno);
			break;
		case USB_DT_SS_ENDPOINT_COMP:
			ifp = &dev->config.intf[ifno];
			memcpy(&ifp->ss_ep_comp_desc[epno], &buffer[index],
				sizeof(ifp->ss_ep_comp_desc[epno]));
			break;
		default:
			if (head->bLength == 0)
				return VMM_OK;

			DPRINTF("%s: unknown description type : 0x%x\n",
				__func__, head->bDescriptorType);

#ifdef DEBUG
			{
				unsigned char *ch = (unsigned char *)head;
				int i;

				for (i = 0; i < head->bLength; i++)
					DPRINTF("%02X ", *ch++);
				DPRINTF("\n\n\n");
			}
#endif
			break;
		};
		index += head->bLength;
		head = (struct usb_descriptor_header *)&buffer[index];
	}

	return VMM_OK;
}

/*
 * ==================== USB Hub Helper Routines ====================
 */

static int usb_hub_get_descriptor(struct usb_device *dev,
				  void *data, int size)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		USB_REQ_GET_DESCRIPTOR, USB_DIR_IN | USB_RT_HUB,
		USB_DT_HUB << 8, 0, data, size, NULL, USB_CNTL_TIMEOUT);
}

static int usb_hub_clear_port_feature(struct usb_device *dev,
				      int port, int feature)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				USB_REQ_CLEAR_FEATURE, USB_RT_PORT, feature,
				port, NULL, 0, NULL, USB_CNTL_TIMEOUT);
}

static int usb_hub_set_port_feature(struct usb_device *dev,
				    int port, int feature)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				USB_REQ_SET_FEATURE, USB_RT_PORT, feature,
				port, NULL, 0, NULL, USB_CNTL_TIMEOUT);
}

static int usb_hub_get_status(struct usb_device *dev, void *data)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_STATUS, USB_DIR_IN | USB_RT_HUB, 0, 0,
			data, sizeof(struct usb_hub_status),
			NULL, USB_CNTL_TIMEOUT);
}

static int usb_hub_get_port_status(struct usb_device *dev,
				   int port, void *data)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_STATUS, USB_DIR_IN | USB_RT_PORT, 0, port,
			data, sizeof(struct usb_hub_status),
			NULL, USB_CNTL_TIMEOUT);
}

static inline const char *usb_hub_portspeed(int portstatus)
{
	char *speed_str;
	int portmask = (USB_PORT_STAT_LOW_SPEED|USB_PORT_STAT_HIGH_SPEED);

	switch (portstatus & portmask) {
	case USB_PORT_STAT_SUPER_SPEED:
		speed_str = "5 Gb/s";
		break;
	case USB_PORT_STAT_HIGH_SPEED:
		speed_str = "480 Mb/s";
		break;
	case USB_PORT_STAT_LOW_SPEED:
		speed_str = "1.5 Mb/s";
		break;
	default:
		speed_str = "12 Mb/s";
		break;
	}

	return speed_str;
}

static int usb_hub_port_reset(struct usb_device *dev, int port,
			      unsigned short *portstat)
{
#define MAX_TRIES 5
	int err, tries;
	struct usb_port_status *portsts;
	unsigned short portstatus, portchange;

	portsts = vmm_zalloc(sizeof(struct usb_port_status));
	if (!portsts) {
		return VMM_ENOMEM;
	}

	DPRINTF("%s: resetting port %d...\n", __func__, port);
	for (tries = 0; tries < MAX_TRIES; tries++) {
		usb_hub_set_port_feature(dev, port + 1, USB_PORT_FEAT_RESET);
		vmm_mdelay(200);

		err = usb_hub_get_port_status(dev, port + 1, portsts);
		if (err < 0) {
			vmm_printf("%s: get_port_status failed error=%d\n",
				   __func__, err);
			vmm_free(portsts);
			return err;
		}
		portstatus = vmm_le16_to_cpu(portsts->wPortStatus);
		portchange = vmm_le16_to_cpu(portsts->wPortChange);

		DPRINTF("%s: portstatus 0x%x, change 0x%x, %s\n",
			__func__, portstatus, portchange,
			usb_hub_portspeed(portstatus));

		DPRINTF("%s: STAT_C_CONNECTION = %d STAT_CONNECTION = %d" \
		        " USB_PORT_STAT_ENABLE %d\n", __func__,
			(portchange & USB_PORT_STAT_C_CONNECTION) ? 1 : 0,
			(portstatus & USB_PORT_STAT_CONNECTION) ? 1 : 0,
			(portstatus & USB_PORT_STAT_ENABLE) ? 1 : 0);

		if ((portchange & USB_PORT_STAT_C_CONNECTION) ||
		    !(portstatus & USB_PORT_STAT_CONNECTION))
			return VMM_EFAIL;

		if (portstatus & USB_PORT_STAT_ENABLE)
			break;

		vmm_mdelay(200);
	}

	if (tries == MAX_TRIES) {
		vmm_printf("%s: Cannot enable port %i after %i retries, "
			   "disabling port.\n", __func__, port + 1, MAX_TRIES);
		vmm_printf("%s: Maybe the USB cable is bad?\n", __func__);
		vmm_free(portsts);
		return VMM_EFAIL;
	}

	usb_hub_clear_port_feature(dev, port + 1, USB_PORT_FEAT_C_RESET);
	*portstat = portstatus;

	vmm_free(portsts);

	return VMM_OK;
}

static void usb_hub_power_on(struct usb_hub_device *hub)
{
	int i, ret;
	u16 portstatus;
	struct usb_device *dev = hub->dev;
	u32 pgood_delay = hub->desc.bPwrOn2PwrGood * 2;
	struct usb_port_status *portsts;

	portsts = vmm_zalloc(sizeof(struct usb_port_status));
	if (!portsts) {
		DPRINTF("%s: port status alloc failed\n", __func__);
		return;
	}
	usb_ref_device(dev);

	/* Enable power to the ports:
	 * Here we Power-cycle the ports: aka,
	 * turning them off and turning on again.
	 */
	DPRINTF("%s: enabling power on all ports\n", __func__);
	for (i = 0; i < dev->maxchild; i++) {
		usb_hub_clear_port_feature(dev, i + 1, USB_PORT_FEAT_POWER);
		DPRINTF("%s: port %d returns 0x%lx\n",
			__func__, i + 1, dev->status);
	}

	/* Wait at least 2*bPwrOn2PwrGood for PP to change */
	vmm_mdelay(pgood_delay);

	for (i = 0; i < dev->maxchild; i++) {
		ret = usb_hub_get_port_status(dev, i + 1, portsts);
		if (ret < 0) {
			DPRINTF("%s: port %d get_port_status failed\n",
				__func__, i + 1);
			vmm_free(portsts);
			usb_dref_device(dev);
			return;
		}

		/* Check to confirm the state of Port Power:
		 * xHCI says "After modifying PP, s/w shall read
		 * PP and confirm that it has reached the desired state
		 * before modifying it again, undefined behavior may occur
		 * if this procedure is not followed".
		 * EHCI doesn't say anything like this, but no harm in keeping
		 * this.
		 */
		portstatus = vmm_le16_to_cpu(portsts->wPortStatus);
		if (portstatus & (USB_PORT_STAT_POWER << 1)) {
			DPRINTF("%s: port %d power change failed\n",
				__func__, i + 1);
			vmm_free(portsts);
			usb_dref_device(dev);
			return;
		}
	}

	for (i = 0; i < dev->maxchild; i++) {
		usb_hub_set_port_feature(dev, i + 1, USB_PORT_FEAT_POWER);
		DPRINTF("%s: port %d returns 0x%lx\n",
			__func__, i + 1, dev->status);
	}

	/* Wait for power to become stable */
	if (pgood_delay < USB_HUB_MIN_POWER_ON_DELAY) {
		vmm_mdelay(USB_HUB_MIN_POWER_ON_DELAY);
	} else {
		vmm_mdelay(pgood_delay);
	}

	vmm_free(portsts);
	usb_dref_device(dev);
}

static int usb_hub_configure(struct usb_device *dev,
			     struct usb_interface *intf)
{
	int i, err = VMM_OK;
	u8 *bitmap, *buffer;
	u16 hubCharacteristics;
	struct usb_hub_device *hub;
#ifdef DEBUG
	struct usb_hub_status *hubsts;
#endif
	struct usb_hub_descriptor *descriptor;

	buffer = vmm_zalloc(USB_BUFSIZ);
	if (!buffer) {
		return VMM_ENOMEM;
	}
	usb_ref_device(dev);

	/* Allocate Hub device */
	hub = usb_hub_alloc();
	if (hub == NULL) {
		err = VMM_ENOMEM;
		goto done;
	}
	hub->dev = dev;
	hub->intf = intf;

	/* Get Hub descriptor */
	if (usb_hub_get_descriptor(dev, buffer, 4) < 0) {
		DPRINTF("%s: failed to get hub descriptor, giving up 0x%lx\n",
			__func__, dev->status);
		usb_hub_free(hub);
		err = VMM_EFAIL;
		goto done;
	}
	descriptor = (struct usb_hub_descriptor *)buffer;

	/* Silence compiler warning if USB_BUFSIZ is > 256 [= sizeof(char)] */
	if (descriptor->bLength > USB_BUFSIZ) {
		DPRINTF("%s: failed to hub descriptor too long: %d\n",
			__func__, descriptor->bLength);
		usb_hub_free(hub);
		err = VMM_EINVALID;
		goto done;
	}

	if (usb_hub_get_descriptor(dev, buffer, descriptor->bLength) < 0) {
		DPRINTF("%s: failed to hub descriptor 2nd giving up 0x%lx\n",
			__func__, dev->status);
		usb_hub_free(hub);
		err = VMM_EFAIL;
		goto done;
	}
	memcpy(&hub->desc, buffer, descriptor->bLength);

	/* Adjust 16bit values */
	hubCharacteristics = get_unaligned(&descriptor->wHubCharacteristics);
	put_unaligned(vmm_le16_to_cpu(hubCharacteristics),
		      &hub->desc.wHubCharacteristics);

	/* Set the bitmap */
	/* Devices not removable by default */
	bitmap = (u8 *)&hub->desc.u.hs.DeviceRemovable[0];
	memset(bitmap, 0xff, (USB_MAXCHILDREN+1+7)/8);
	bitmap = (u8 *)&hub->desc.u.hs.PortPwrCtrlMask[0];
	memset(bitmap, 0xff, (USB_MAXCHILDREN+1+7)/8); /* PowerMask = 1B */

	for (i = 0; i < ((hub->desc.bNbrPorts + 1 + 7)/8); i++) {
		hub->desc.u.hs.DeviceRemovable[i] =
					descriptor->u.hs.DeviceRemovable[i];
	}

	for (i = 0; i < ((hub->desc.bNbrPorts + 1 + 7)/8); i++) {
		hub->desc.u.hs.PortPwrCtrlMask[i] =
					descriptor->u.hs.PortPwrCtrlMask[i];
	}

	dev->maxchild = descriptor->bNbrPorts;
	DPRINTF("%s: %d ports detected on %s\n",
		__func__, dev->maxchild, dev->dev.name);

	hubCharacteristics = get_unaligned(&hub->desc.wHubCharacteristics);
	switch (hubCharacteristics & HUB_CHAR_LPSM) {
	case 0x00:
		DPRINTF("%s: ganged power switching\n", __func__);
		break;
	case 0x01:
		DPRINTF("%s: individual port power switching\n", __func__);
		break;
	case 0x02:
	case 0x03:
		DPRINTF("%s: reserved power switching mode\n", __func__);
		break;
	}

	if (hubCharacteristics & HUB_CHAR_COMPOUND) {
		DPRINTF("%s: part of a compound device\n", __func__);
	} else {
		DPRINTF("%: standalone hub\n", __func__);
	}

	switch (hubCharacteristics & HUB_CHAR_OCPM) {
	case 0x00:
		DPRINTF("%s: global over-current protection\n", __func__);
		break;
	case 0x08:
		DPRINTF("%s: individual port over-current protection\n",
			__func__);
		break;
	case 0x10:
	case 0x18:
		DPRINTF("%s: no over-current protection\n", __func__);
		break;
	}

	DPRINTF("%s: power on to power good time: %dms\n",
		__func__, descriptor->bPwrOn2PwrGood * 2);
	DPRINTF("%s: hub controller current requirement: %dmA\n",
		__func__, descriptor->bHubContrCurrent);

	for (i = 0; i < dev->maxchild; i++) {
		DPRINTF("%s: port %d is%s removable\n", __func__, i + 1,
			hub->desc.DeviceRemovable[(i + 1) / 8] & \
			(1 << ((i + 1) % 8)) ? " not" : "");
	}

	if (sizeof(struct usb_hub_status) > USB_BUFSIZ) {
		DPRINTF("%s: failed to get Status too long: %d\n",
			__func__, descriptor->bLength);
		usb_hub_free(hub);
		err = VMM_EFAIL;
		goto done;
	}

	if (usb_hub_get_status(dev, buffer) < 0) {
		DPRINTF("%s: failed to get Status 0x%lx\n",
			__func__, dev->status);
		usb_hub_free(hub);
		err = VMM_EFAIL;
		goto done;
	}

#ifdef DEBUG
	hubsts = (struct usb_hub_status *)buffer;
#endif

	DPRINTF("%s: get_hub_status returned status 0x%x, change 0x%x\n",
		__func__,
		vmm_le16_to_cpu(hubsts->wHubStatus),
		vmm_le16_to_cpu(hubsts->wHubChange));
	DPRINTF("%s: local power source is %s\n", __func__,
		(vmm_le16_to_cpu(hubsts->wHubStatus) & HUB_STATUS_LOCAL_POWER)
		 ? "lost (inactive)" : "good");
	DPRINTF("%s: %sover-current condition exists\n", __func__,
		(vmm_le16_to_cpu(hubsts->wHubStatus) & HUB_STATUS_OVERCURRENT)
		? "" : "no ");

	/* Power-on the hub */
	usb_hub_power_on(hub);

	/* Mark hub as configured */
	hub->configured = TRUE;

	/* Add hub to gobal list */
	usb_hub_add(hub);

done:
	usb_dref_device(dev);
	vmm_free(buffer);

	return err;
}

static int usb_hub_detect_new_device(struct usb_device *parent,
				     struct usb_device *dev)
{
	u32 i, addr;
	u8 *tmpbuf;
	u16 portstatus;
	int err, port = -1;
	struct usb_device_descriptor *desc;
	enum usb_device_state state;

	usb_ref_device(dev);

	/* Sanity check on device state */
	state = usb_get_device_state(dev);
	if (state != USB_STATE_NOTATTACHED) {
		usb_dref_device(dev);
		return VMM_EINVALID;
	}

	/* Alloc buffer for temporary read/writes */
	tmpbuf = vmm_zalloc(USB_BUFSIZ);
	if (!tmpbuf) {
		usb_dref_device(dev);
		return VMM_ENOMEM;
	}

	/* We still haven't set the Address yet */
	addr = dev->devnum;
	dev->devnum = 0;

	/* This is a Windows scheme of initialization sequence, with double
	 * reset of the device (Linux uses the same sequence)
	 * Some equipment is said to work only with such init sequence; this
	 * patch is based on the work by Alan Stern:
	 * http://sourceforge.net/mailarchive/forum.php?
	 * thread_id=5729457&forum_id=5398
	 */

	/* Send 64-byte GET-DEVICE-DESCRIPTOR request.  Since the descriptor is
	 * only 18 bytes long, this will terminate with a short packet.  But if
	 * the maxpacket size is 8 or 16 the device may be waiting to transmit
	 * some more, or keeps on retransmitting the 8 byte header. */

	desc = (struct usb_device_descriptor *)tmpbuf;
	dev->descriptor.bMaxPacketSize0 = 64;	    /* Start off at 64 bytes  */
	/* Default to 64 byte max packet size */
	dev->maxpacketsize = PACKET_SIZE_64;
	dev->epmaxpacketin[0] = 64;
	dev->epmaxpacketout[0] = 64;

	err = usb_get_descriptor(dev, USB_DT_DEVICE, 0, desc, 64);
	if (err) {
		vmm_printf("%s: usb_get_descriptor() failed\n", __func__);
		dev->devnum = addr;
		goto done;
	}

	dev->descriptor.bMaxPacketSize0 = desc->bMaxPacketSize0;
	/*
	 * Fetch the device class, driver can use this info
	 * to differentiate between HUB and DEVICE.
	 */
	dev->descriptor.bDeviceClass = desc->bDeviceClass;

	/* Mark device as attached */
	usb_set_device_state(dev, USB_STATE_ATTACHED);

	/* Find the port number we're at */
	if (parent) {
		int j;
		irq_flags_t flags;

		vmm_spin_lock_irqsave(&parent->children_lock, flags);
		for (j = 0; j < parent->maxchild; j++) {
			if (parent->children[j] == dev) {
				port = j;
				break;
			}
		}
		vmm_spin_unlock_irqrestore(&parent->children_lock, flags);
		if (port < 0) {
			vmm_printf("%s: cannot locate device's port.\n");
			dev->devnum = addr;
			err = VMM_EFAIL;
			goto done;
		}

		/* reset the port for the second time */
		err = usb_hub_port_reset(parent, port, &portstatus);
		if (err) {
			vmm_printf("%s: couldn't reset port %d\n",
				   __func__, port);
			dev->devnum = addr;
			goto done;
		}
	}

	dev->epmaxpacketin[0] = dev->descriptor.bMaxPacketSize0;
	dev->epmaxpacketout[0] = dev->descriptor.bMaxPacketSize0;
	switch (dev->descriptor.bMaxPacketSize0) {
	case 8:
		dev->maxpacketsize  = PACKET_SIZE_8;
		break;
	case 16:
		dev->maxpacketsize = PACKET_SIZE_16;
		break;
	case 32:
		dev->maxpacketsize = PACKET_SIZE_32;
		break;
	case 64:
		dev->maxpacketsize = PACKET_SIZE_64;
		break;
	}

	/* Mark device as powered */
	usb_set_device_state(dev, USB_STATE_POWERED);

	err = usb_set_address(dev, addr); /* set address */
	dev->devnum = addr; /* fail or success we restore back devnum */
	if (err < 0) {
		vmm_printf("%s: device not accepting new address %d "
			   "(err=%d)\n", __func__, err);
		goto done;
	}

	vmm_mdelay(10); /* Let the SET_ADDRESS settle */

	/* Mark device as address set */
	usb_set_device_state(dev, USB_STATE_ADDRESS);

	err = usb_get_descriptor(dev, USB_DT_DEVICE, 0,
				 tmpbuf, sizeof(dev->descriptor));
	if (err) {
		vmm_printf("%s: unable to get device descriptor "
			   "(error=%d)\n", __func__, err);
		goto done;
	}
	memcpy(&dev->descriptor, tmpbuf, sizeof(dev->descriptor));

	/* Correct LE values */
	dev->descriptor.bcdUSB = vmm_le16_to_cpu(dev->descriptor.bcdUSB);
	dev->descriptor.idVendor = vmm_le16_to_cpu(dev->descriptor.idVendor);
	dev->descriptor.idProduct = vmm_le16_to_cpu(dev->descriptor.idProduct);
	dev->descriptor.bcdDevice = vmm_le16_to_cpu(dev->descriptor.bcdDevice);

	/* Only support for one config for now */
	err = usb_get_configuration_no(dev, tmpbuf, 0);
	if (err < 0) {
		vmm_printf("%s: Cannot read configuration, "
			   "skipping device %04x:%04x\n", __func__,
			   dev->descriptor.idVendor,
			   dev->descriptor.idProduct);
		goto done;
	}

	usb_parse_config(dev, tmpbuf, 0);
	usb_set_maxpacket(dev);

	/* We set the default configuration here */
	err = usb_set_configuration(dev, dev->config.desc.bConfigurationValue);
	if (err) {
		vmm_printf("%s: failed to set default configuration "
			   "error=%d\n", __func__, err);
		goto done;
	}

	/* Read device strings */
	memset(dev->manufacturer, 0, sizeof(dev->manufacturer));
	memset(dev->product, 0, sizeof(dev->product));
	memset(dev->serial, 0, sizeof(dev->serial));
	if (dev->descriptor.iManufacturer)
		usb_string(dev, dev->descriptor.iManufacturer,
			   dev->manufacturer, sizeof(dev->manufacturer));
	if (dev->descriptor.iProduct)
		usb_string(dev, dev->descriptor.iProduct,
			   dev->product, sizeof(dev->product));
	if (dev->descriptor.iSerialNumber)
		usb_string(dev, dev->descriptor.iSerialNumber,
			   dev->serial, sizeof(dev->serial));

	/* Set device state to configured */
	usb_set_device_state(dev, USB_STATE_CONFIGURED);

	/* Inform everyone about new USB device */
	usb_announce_device(dev);

	/* Register new device to devdrv */
	vmm_devdrv_register_device(&dev->dev);

	/* Register interface devices to devdrv */
	for (i = 0; i < dev->config.no_of_intf; i++) {
		vmm_devdrv_register_device(&dev->config.intf[i].dev);
	}

	/* Inform everyone about new non-root-hub device */
	if (dev->dev.parent) {
		usb_notify_add_device(dev);
	}

done:
	/* Free temporary buffer */
	vmm_free(tmpbuf);

	/* Free the device this will decrease reference count */
	usb_dref_device(dev);

	return err;
}

static void usb_recursively_disconnect(struct usb_device *dev)
{
	int i;
	irq_flags_t flags;
	struct usb_interface *intf;

	/* Disconnect the child devices first */
	vmm_spin_lock_irqsave(&dev->children_lock, flags);
	for (i = 0; i < dev->maxchild; ++i) {
		if (!dev->children[i]) {
			continue;
		}
		vmm_spin_unlock_irqrestore(&dev->children_lock, flags);
		usb_recursively_disconnect(dev->children[i]);
		vmm_spin_lock_irqsave(&dev->children_lock, flags);
	}
	dev->maxchild = 0;
	vmm_spin_unlock_irqrestore(&dev->children_lock, flags);

	/* Mark device as not attached */
	usb_set_device_state(dev, USB_STATE_NOTATTACHED);

	/* Unregister interface devices from devdrv */
	for (i = 0; i < dev->config.no_of_intf; i++) {
		intf = &dev->config.intf[i];
		vmm_devdrv_unregister_device(&intf->dev);
	}

	/* Inform everyone about removed non-root-hub devices */
	if (dev->dev.parent) {
		usb_notify_remove_device(dev);
	}

	/* Unregister from devdrv */
	vmm_devdrv_unregister_device(&dev->dev);

	/* Flush all HUB work related to this USB device */
	usb_hub_flush_dev_work(dev);

	/* Free the device */
	usb_dref_device(dev);
}

static void usb_hub_port_connect_change(struct usb_hub_device *hub,
					struct usb_port_status *portsts,
					int port)
{
	u16 portstatus, portchange;
	u16 portmask = (USB_PORT_STAT_LOW_SPEED|USB_PORT_STAT_HIGH_SPEED);
	irq_flags_t flags;
	struct usb_device *usb;
	struct usb_device *dev = hub->dev;

	usb_ref_device(dev);

	portstatus = vmm_le16_to_cpu(portsts->wPortStatus);
	portchange = vmm_le16_to_cpu(portsts->wPortChange);

	DPRINTF("%s: dev %s port %d status 0x%x, change 0x%x, %s\n",
		__func__, dev->dev.name, port + 1, portstatus, portchange,
		usb_hub_portspeed(portstatus));

	/* Clear the connection change status */
	usb_hub_clear_port_feature(dev, port + 1, USB_PORT_FEAT_C_CONNECTION);

	/* Skip if no connection change */
	if (!(portchange & USB_PORT_STAT_C_CONNECTION) &&
	    !(portchange & USB_PORT_STAT_C_ENABLE)) {
		goto done;
	}

	/* Disconnect any existing devices under this port */
	vmm_spin_lock_irqsave(&dev->children_lock, flags);
	if (dev->children[port] &&
	    !(portstatus & USB_PORT_STAT_CONNECTION)) {
		usb = dev->children[port];
		vmm_spin_unlock_irqrestore(&dev->children_lock, flags);
		usb_recursively_disconnect(usb);
		goto done;
	}
	vmm_spin_unlock_irqrestore(&dev->children_lock, flags);

	/* Wait for clear connection to finish */
	vmm_msleep(200);

	/* Reset the port */
	if (usb_hub_port_reset(dev, port, &portstatus) < 0) {
		vmm_printf("%s: cannot reset port %i!?\n",
			   __func__, port + 1);
		goto done;
	}

	/* Wait for reset to finish */
	vmm_msleep(200);

	/* Allocate a new device struct for it */
	usb = usb_alloc_device(dev, dev->hcd, port);

	/* Determine device speed */
	switch (portstatus & portmask) {
	case USB_PORT_STAT_SUPER_SPEED:
		usb->speed = USB_SPEED_SUPER;
		break;
	case USB_PORT_STAT_HIGH_SPEED:
		usb->speed = USB_SPEED_HIGH;
		break;
	case USB_PORT_STAT_LOW_SPEED:
		usb->speed = USB_SPEED_LOW;
		break;
	default:
		usb->speed = USB_SPEED_FULL;
		break;
	}

	/* Update parent childer list */
	vmm_spin_lock_irqsave(&dev->children_lock, flags);
	dev->children[port] = usb;
	vmm_spin_unlock_irqrestore(&dev->children_lock, flags);
	usb->portnum = port + 1;

	/* Run it through the hoops (find a driver, etc) */
	if (usb_hub_detect_new_device(dev, usb) < 0) {
		/* Woops, disable the port */
		usb_dref_device(usb);
		vmm_spin_lock_irqsave(&dev->children_lock, flags);
		dev->children[port] = NULL;
		vmm_spin_unlock_irqrestore(&dev->children_lock, flags);
		DPRINTF("%s: disabling port %d\n", __func__, port + 1);
		usb_hub_clear_port_feature(dev, port + 1, USB_PORT_FEAT_ENABLE);
	}

done:
	usb_dref_device(dev);
}

static int usb_hub_poll_status(struct usb_hub_device *hub)
{
	int i, err;
	u16 portstatus, portchange;
	struct usb_device *dev = hub->dev;
	struct usb_port_status __cacheline_aligned portsts;

	if (!hub->configured) {
		vmm_printf("%s: Hub not configured\n", __func__);
		return VMM_EINVALID;
	}

	usb_ref_device(dev);

	for (i = 0; i < dev->maxchild; i++) {
		err = usb_hub_get_port_status(dev, i + 1, &portsts);
		if (err < 0) {
			DPRINTF("%s: dev %s port %d get_port_status failed\n",
				__func__, i + 1, dev->dev.name);
			continue;
		}
		portstatus = vmm_le16_to_cpu(portsts.wPortStatus);
		portchange = vmm_le16_to_cpu(portsts.wPortChange);

		DPRINTF("%s: dev %s port %d status 0x%x change 0x%x\n",
			__func__, dev->dev.name, i + 1,
			portstatus, portchange);

		if (portchange & USB_PORT_STAT_C_CONNECTION) {
			DPRINTF("%s: dev %s port %d connection change\n",
				__func__, dev->dev.name, i + 1);
			usb_hub_port_connect_change(hub, &portsts, i);
		}

		if (portchange & USB_PORT_STAT_C_ENABLE) {
			DPRINTF("%s: dev %s port %d enable change\n",
				__func__, dev->dev.name, i + 1);
			usb_hub_clear_port_feature(dev, i + 1,
						USB_PORT_FEAT_C_ENABLE);
		}

		if (portstatus & USB_PORT_STAT_SUSPEND) {
			DPRINTF("%s: dev %s port %d suspend change\n",
				__func__, dev->dev.name, i + 1);
			usb_hub_clear_port_feature(dev, i + 1,
						USB_PORT_FEAT_SUSPEND);
		}

		if (portchange & USB_PORT_STAT_C_OVERCURRENT) {
			DPRINTF("%s: dev %s port %d over-current change\n",
				__func__, dev->dev.name, i + 1);
			usb_hub_clear_port_feature(dev, i + 1,
						USB_PORT_FEAT_C_OVER_CURRENT);
			usb_hub_power_on(hub);
		}

		if (portchange & USB_PORT_STAT_C_RESET) {
			DPRINTF("%s: dev %s port %d reset change\n",
				__func__, dev->dev.name, i + 1);
			usb_hub_clear_port_feature(dev, i + 1,
						USB_PORT_FEAT_C_RESET);
		}
	}

	usb_dref_device(dev);

	return VMM_OK;
}

/*
 * ==================== USB Hub Monitor Work ====================
 */

#define USB_HUB_MON_EVENT_NSECS		2000000000ULL
static struct usb_hub_work usb_hub_mon_work;
static struct vmm_timer_event usb_hub_mon_event;

static int usb_hub_mon_work_func(struct usb_hub_work *work)
{
	int i, err;
	u32 count = usb_hub_count();
	struct usb_hub_device *hub;

	for (i = 0; i < count; i++) {
		hub = usb_hub_get(i);
		if (!hub) {
			break;
		}

		err = usb_hub_poll_status(hub);
		if (err) {
			vmm_printf("%s: Hub status poll failed (error %d)\n",
				   __func__, err);
		}
	}

	vmm_timer_event_start(&usb_hub_mon_event, USB_HUB_MON_EVENT_NSECS);

	return VMM_OK;
}

static void usb_hub_mon_event_func(struct vmm_timer_event *ev)
{
	usb_hub_queue_work(&usb_hub_mon_work);
}

/*
 * ==================== USB Hub Device Driver ====================
 */

static int usb_hub_driver_probe(struct usb_interface *intf,
				const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_endpoint_descriptor *ep = &intf->ep_desc[0];

	/* Is it a hub? */
	if (intf->desc.bInterfaceClass != USB_CLASS_HUB)
		return VMM_ENODEV;

	/* Some hubs have a subclass of 1, which AFAICT according to the */
	/*  specs is not defined, but it works */
	if ((intf->desc.bInterfaceSubClass != 0) &&
	    (intf->desc.bInterfaceSubClass != 1))
		return VMM_ENODEV;

	/* Multiple endpoints? What kind of mutant ninja-hub is this? */
	if (intf->desc.bNumEndpoints != 1)
		return VMM_ENODEV;

	/* Output endpoint? Curiousier and curiousier.. */
	if (!(ep->bEndpointAddress & USB_DIR_IN))
		return VMM_ENODEV;

	/* If it's not an interrupt endpoint, we'd better punt! */
	if ((ep->bmAttributes & 3) != 3)
		return VMM_ENODEV;

	/* We found a hub */
	vmm_printf("%s: USB hub found\n", intf->dev.name);

	/* Configure the hub */
	return usb_hub_configure(dev, intf);
}

static void usb_hub_driver_disconnect(struct usb_interface *intf)
{
	struct usb_hub_device *hub =
			usb_hub_find(interface_to_usbdev(intf), intf);

	if (!hub) {
		return;
	}

	/* Remove the hub from global list */
	usb_hub_remove(hub);

	/* Free the hub */
	usb_hub_free(hub);
}

static const struct usb_device_id usb_hub_driver_id_table[] = {
    { .match_flags = USB_DEVICE_ID_MATCH_DEV_CLASS,
      .bDeviceClass = USB_CLASS_HUB},
    { .match_flags = USB_DEVICE_ID_MATCH_INT_CLASS,
      .bInterfaceClass = USB_CLASS_HUB},
    { }						/* Terminating entry */
};

static struct usb_driver usb_hub_driver = {
	.name		= "usb_hub",
	.id_table	= usb_hub_driver_id_table,
	.probe		= usb_hub_driver_probe,
	.disconnect	= usb_hub_driver_disconnect,
};

/*
 * ==================== USB Hub Notification Handling ====================
 */

static int usb_new_device_work(struct usb_hub_work *work)
{
	struct usb_device *dev = work->dev;
	struct usb_device *parent =
		(dev->dev.parent) ? to_usb_device(dev->dev.parent) : NULL;

	return usb_hub_detect_new_device(parent, dev);
}

static void usb_new_device(struct usb_device *dev)
{
	struct usb_hub_work *w;

	w = usb_hub_alloc_work(usb_new_device_work, dev);
	if (w) {
		usb_hub_queue_work(w);
	}
}

static int usb_disconnect_work(struct usb_hub_work *work)
{
	struct usb_device *dev = work->dev;

	/* Sanity check on device state */
	if (usb_get_device_state(dev) != USB_STATE_NOTATTACHED) {
		return VMM_EINVALID;
	}

	/* recursively disconnect this device and all child devices */
	usb_recursively_disconnect(dev);

	return VMM_OK;
}

static void usb_disconnect(struct usb_device *dev)
{
	struct usb_hub_work *w;

	w = usb_hub_alloc_work(usb_disconnect_work, dev);
	if (w) {
		usb_hub_queue_work(w);
	}
}

static int usb_hub_notifier_call(struct vmm_notifier_block *nb,
				 unsigned long event, void *data)
{
	int ret = NOTIFY_DONE;
	struct usb_device *dev;

	/* We only care about root hub devices */
	switch (event) {
	case USB_DEVICE_ADD:
		dev = data;
		if (!dev->dev.parent) {
			usb_new_device(dev);
			ret = NOTIFY_OK;
		}
		break;
	case USB_DEVICE_REMOVE:
		dev = data;
		if (!dev->dev.parent) {
			usb_disconnect(dev);
			ret = NOTIFY_OK;
		}
		break;
	default:
		break;
	};

	return ret;
}

static struct vmm_notifier_block usb_hub_nb = {
	.notifier_call = usb_hub_notifier_call,
};

/*
 * ==================== USB Hub Init/Exit ====================
 */

int __init usb_hub_init(void)
{
	int rc;

	/* Register hub driver */
	rc = usb_register(&usb_hub_driver);
	if (rc) {
		return rc;
	}

	/* Create hub worker thread */
	usb_hub_worker_thread = vmm_threads_create("hubd",
						   usb_hub_worker_main, NULL,
						   VMM_THREAD_DEF_PRIORITY,
						   VMM_THREAD_DEF_TIME_SLICE);
	if (!usb_hub_worker_thread) {
		return VMM_EFAIL;
	}
	vmm_threads_start(usb_hub_worker_thread);

	/* Initialize hub monitor work */
	usb_hub_init_work(&usb_hub_mon_work,
			  usb_hub_mon_work_func,
			  FALSE, NULL);
	INIT_TIMER_EVENT(&usb_hub_mon_event, usb_hub_mon_event_func, NULL);
	vmm_timer_event_start(&usb_hub_mon_event, USB_HUB_MON_EVENT_NSECS);

	/* Register event notifier */
	usb_register_notify(&usb_hub_nb);

	return VMM_OK;
}

void __exit usb_hub_exit(void)
{
	/* Unregister event notifier */
	usb_unregister_notify(&usb_hub_nb);

	/* Stop hub monitor work */
	vmm_timer_event_stop(&usb_hub_mon_event);

	/* Destroy hub worker thread */
	if (usb_hub_worker_thread) {
		vmm_threads_stop(usb_hub_worker_thread);
		vmm_threads_destroy(usb_hub_worker_thread);
	}

	/* Flush all pending work */
	usb_hub_flush_all_work();

	/* Unregister hub driver */
	usb_deregister(&usb_hub_driver);
}
