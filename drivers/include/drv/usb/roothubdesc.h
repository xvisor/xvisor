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
 * @file roothubdesc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for virtual Root Hub.
 *
 * This header is largely adapted from u-boot sources:
 * <u-boot>/include/usbroothubdes.h
 *
 * USB virtual root hub descriptors
 *
 * (C) Copyright 2014
 * Stephen Warren swarren@wwwdotorg.org
 *
 * Based on ohci-hcd.c
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __USB_ROOTHUBDESC_H__
#define __USB_ROOTHUBDESC_H__

#include <vmm_types.h>

/* Device descriptor */
static u8 root_hub_dev_desc[] = {
	0x12,		/* u8  bLength; */
	0x01,		/* u8  bDescriptorType; Device */
	0x10,		/* u16 bcdUSB; v1.1 */
	0x01,
	0x09,		/* u8  bDeviceClass; HUB_CLASSCODE */
	0x00,		/* u8  bDeviceSubClass; */
	0x00,		/* u8  bDeviceProtocol; */
	0x08,		/* u8  bMaxPacketSize0; 8 Bytes */
	0x00,		/* u16 idVendor; */
	0x00,
	0x00,		/* u16 idProduct; */
	0x00,
	0x00,		/* u16 bcdDevice; */
	0x00,
	0x01,		/* u8  iManufacturer; */
	0x02,		/* u8  iProduct; */
	0x03,		/* u8  iSerialNumber; */
	0x01,		/* u8  bNumConfigurations; */
};

/* Configuration descriptor */
static u8 root_hub_config_desc[] = {
	0x09,		/* u8  bLength; */
	0x02,		/* u8  bDescriptorType; Configuration */
	0x19,		/* u16 wTotalLength; */
	0x00,
	0x01,		/* u8  bNumInterfaces; */
	0x01,		/* u8  bConfigurationValue; */
	0x00,		/* u8  iConfiguration; */
	0x40,		/* u8  bmAttributes;
			 *       Bit 7: Bus-powered
			 *       6: Self-powered,
			 *       5 Remote-wakwup,
			 *       4..0: resvd
			 */
	0x00,		/* u8  MaxPower; */
	/* interface */
	0x09,		/* u8  if_bLength; */
	0x04,		/* u8  if_bDescriptorType; Interface */
	0x00,		/* u8  if_bInterfaceNumber; */
	0x00,		/* u8  if_bAlternateSetting; */
	0x01,		/* u8  if_bNumEndpoints; */
	0x09,		/* u8  if_bInterfaceClass; HUB_CLASSCODE */
	0x00,		/* u8  if_bInterfaceSubClass; */
	0x00,		/* u8  if_bInterfaceProtocol; */
	0x00,		/* u8  if_iInterface; */
	/* endpoint */
	0x07,		/* u8  ep_bLength; */
	0x05,		/* u8  ep_bDescriptorType; Endpoint */
	0x81,		/* u8  ep_bEndpointAddress; IN Endpoint 1 */
	0x03,		/* u8  ep_bmAttributes; Interrupt */
	0x02,		/* u16 ep_wMaxPacketSize; ((MAX_ROOT_PORTS + 1) / 8 */
	0x00,
	0xff,		/* u8  ep_bInterval; 255 ms */
};

#ifdef WANT_USB_ROOT_HUB_HUB_DESC
static u8 root_hub_hub_desc[] = {
	0x09,		/* u8  bLength; */
	0x29,		/* u8  bDescriptorType; Hub-descriptor */
	0x02,		/* u8  bNbrPorts; */
	0x00,		/* u16 wHubCharacteristics; */
	0x00,
	0x01,		/* u8  bPwrOn2pwrGood; 2ms */
	0x00,		/* u8  bHubContrCurrent; 0 mA */
	0x00,		/* u8  DeviceRemovable; *** 7 Ports max *** */
	0xff,		/* u8  PortPwrCtrlMask; *** 7 ports max *** */
};
#endif

static u8 root_hub_str_index0[] = {
	0x04,		/* u8  bLength; */
	0x03,		/* u8  bDescriptorType; String-descriptor */
	0x09,		/* u8  lang ID */
	0x04,		/* u8  lang ID */
};

static u8 root_hub_str_index1[] = {
	14,		/* u8  bLength; */
	0x03,		/* u8  bDescriptorType; String-descriptor */
	'X',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'v',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'i',		/* u8  Unicode */
	0,		/* u8  Unicode */
	's',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'o',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'r',		/* u8  Unicode */
	0,		/* u8  Unicode */
};

static u8 root_hub_str_index2[] = {
	32,		/* u8  bLength; */
	0x03,		/* u8  bDescriptorType; String-descriptor */
	'X',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'v',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'i',		/* u8  Unicode */
	0,		/* u8  Unicode */
	's',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'o',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'r',		/* u8  Unicode */
	0,		/* u8  Unicode */
	' ',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'R',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'o',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'o',		/* u8  Unicode */
	0,		/* u8  Unicode */
	't',		/* u8  Unicode */
	0,		/* u8  Unicode */
	' ',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'H',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'u',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'b',		/* u8  Unicode */
	0,		/* u8  Unicode */
};

static u8 root_hub_str_index3[] = {
	10,		/* u8  bLength; */
	0x03,		/* u8  bDescriptorType; String-descriptor */
	'0',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'0',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'0',		/* u8  Unicode */
	0,		/* u8  Unicode */
	'0',		/* u8  Unicode */
	0,		/* u8  Unicode */
};

#endif
