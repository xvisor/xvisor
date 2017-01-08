/*
 * net/core/ethtool.c - Ethtool ioctl handler
 * Copyright (c) 2003 Matthew Wilcox <matthew@wil.cx>
 *
 * This file is where we call all the ethtool_ops commands to get
 * the information ethtool needs.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>

/*
 * Some useful ethtool_ops methods that're device independent.
 * If we find that all drivers want to do the same thing here,
 * we can turn these into dev_() function calls.
 */

u32 ethtool_op_get_link(struct net_device *dev)
{
	return 0;
}

u32 ethtool_op_get_rx_csum(struct net_device *dev)
{
	return 0;
}
EXPORT_SYMBOL(ethtool_op_get_rx_csum);

u32 ethtool_op_get_tx_csum(struct net_device *dev)
{
	return 0;
}
EXPORT_SYMBOL(ethtool_op_get_tx_csum);

int ethtool_op_set_tx_csum(struct net_device *dev, u32 data)
{
	return 0;
}

int ethtool_op_set_tx_hw_csum(struct net_device *dev, u32 data)
{
	return 0;
}

int ethtool_op_set_tx_ipv6_csum(struct net_device *dev, u32 data)
{
	return 0;
}

u32 ethtool_op_get_sg(struct net_device *dev)
{
	return 0;
}

int ethtool_op_set_sg(struct net_device *dev, u32 data)
{
	return 0;
}

u32 ethtool_op_get_tso(struct net_device *dev)
{
	return 0;
}

int ethtool_op_set_tso(struct net_device *dev, u32 data)
{
	return 0;
}

u32 ethtool_op_get_ufo(struct net_device *dev)
{
	return 0;
}

int ethtool_op_set_ufo(struct net_device *dev, u32 data)
{
	return 0;
}

u32 ethtool_op_get_flags(struct net_device *dev)
{
	return 0;
}

int ethtool_op_set_flags(struct net_device *dev, u32 data)
{
	return 0;
}


#if 0
/* The main entry point in this file.  Called from net/core/dev.c */

int dev_ethtool(struct net *net, struct ifreq *ifr)
{
	return 0;
}
#endif

