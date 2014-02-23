/*
 * OF helpers for network devices.
 *
 * This file is released under the GPLv2
 */

#ifndef __LINUX_OF_NET_H
#define __LINUX_OF_NET_H

#include <linux/of.h>

extern int of_get_phy_mode(struct device_node *np);
extern const void *of_get_mac_address(struct device_node *np);

#endif /* __LINUX_OF_NET_H */
