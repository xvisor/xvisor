/*
 * Copyright (C) 2015 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Adapted from Linux kernel version 3.18 net/core/dev.c file
 * by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 *
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Ethernet-type device handling.
 *
 * Version:	@(#)eth.c	1.0.7	05/25/93
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Mark Evans, <evansmp@uhura.aston.ac.uk>
 *		Florian  La Roche, <rzsfl@rz.uni-sb.de>
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *
 * Fixes:
 *		Mr Linux	: Arp problems
 *		Alan Cox	: Generic queue tidyup (very tiny here)
 *		Alan Cox	: eth_header ntohs should be htons
 *		Alan Cox	: eth_rebuild_header missing an htons and
 *				  minor other things.
 *		Tegge		: Arp bug fixes.
 *		Florian		: Removed many unnecessary functions, code cleanup
 *				  and changes for new arp and skbuff.
 *		Alan Cox	: Redid header building to reflect new format.
 *		Alan Cox	: ARP only when compiled with CONFIG_INET
 *		Greg Page	: 802.2 and SNAP stuff.
 *		Alan Cox	: MAC layer pointers/new format.
 *		Paul Gortmaker	: eth_copy_and_sum shouldn't csum padding.
 *		Alan Cox	: Protect against forwarding explosions with
 *				  older network drivers and IFF_ALLMULTI.
 *	Christer Weinigel	: Better rebuild header message.
 *             Andrew Morton    : 26Feb01: kill ether_setup() - use netdev_boot_setup().
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * @file eth.c
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @brief Ethernet-type device handling.
 */

#include <linux/netdevice.h>
#include <linux/etherdevice.h>

/**
 * alloc_etherdev_mqs - Allocates and sets up an Ethernet device
 * @sizeof_priv: Size of additional driver-private structure to be allocated
 *	for this Ethernet device
 * @txqs: The number of TX queues this device has.
 * @rxqs: The number of RX queues this device has.
 *
 * Fill in the fields of the device structure with Ethernet-generic
 * values. Basically does everything except registering the device.
 *
 * Constructs a new net device, complete with a private data area of
 * size (sizeof_priv).  A 32-byte (not bit) alignment is enforced for
 * this private data area.
 */

struct net_device *alloc_etherdev_mqs(int sizeof_priv, unsigned int txqs,
				      unsigned int rxqs)
{
	static int eth_num = 0;
	struct net_device *ndev = NULL;

	if (1 != txqs && 1 != rxqs) {
		vmm_lwarning("Warning: Multi-queue network is not supported "
			     "yet.");
	}
	ndev = alloc_etherdev(sizeof_priv);
	if (!ndev)
		return NULL;

	ether_setup(ndev);
	snprintf(ndev->name, sizeof (ndev->name), "eth%d", eth_num++);

	return ndev;
}
EXPORT_SYMBOL(alloc_etherdev_mqs);
