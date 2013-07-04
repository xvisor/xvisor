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
 * @file message.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of usb device message APIs
 */

#include <vmm_error.h>
#include <vmm_modules.h>
#include <drv/usb.h>
#include <drv/usb/hcd.h>
#include <drv/usb/hub.h>

int usb_control_msg(struct usb_device *dev, u32 pipe,
		    u8 request, u8 requesttype, u16 value, u16 index,
		    void *data, u16 size, int timeout)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_interrupt_msg(struct usb_device *usb_dev, u32 pipe,
		      void *data, int len, int *actual_length, int timeout)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_bulk_msg(struct usb_device *usb_dev, u32 pipe,
		 void *data, int len, int *actual_length, int timeout)
{
	/* FIXME: */
	return VMM_EFAIL;
}

/*
 * returns the max packet size, depending on the pipe direction and
 * the configurations values
 */
int usb_maxpacket(struct usb_device *dev, u32 pipe)
{
	/* direction is out -> use emaxpacket out */
	if ((pipe & USB_DIR_IN) == 0)
		return dev->epmaxpacketout[((pipe>>15) & 0xf)];
	else
		return dev->epmaxpacketin[((pipe>>15) & 0xf)];
}

int usb_get_descriptor(struct usb_device *dev, u8 desctype, 
		       u8 descindex, void *buf, int size)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_get_status(struct usb_device *dev, int type, int target, void *data)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_string(struct usb_device *dev, int index, char *buf, size_t size)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_set_protocol(struct usb_device *dev, int ifnum, int protocol)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_set_idle(struct usb_device *dev, int ifnum, 
		 int duration, int report_id)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_disable_asynch(int disable)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_get_configuration_no(struct usb_device *dev, u8 *buffer, int cfgno)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_get_report(struct usb_device *dev, int ifnum, 
		   u8 type, u8 id, void *buf, u32 size)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_get_class_descriptor(struct usb_device *dev, int ifnum,
			     u8 type, u8 id, void *buf, u32 size)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_clear_halt(struct usb_device *dev, u32 pipe)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_reset_configuration(struct usb_device *dev)
{
	/* FIXME: */
	return VMM_EFAIL;
}

int usb_set_interface(struct usb_device *dev, int ifnum, int alternate)
{
	/* FIXME: */
	return VMM_EFAIL;
}

void usb_reset_endpoint(struct usb_device *dev, u32 epaddr)
{
	/* FIXME: */
}

