#ifndef _LINUX_ETHERDEVICE_H
#define _LINUX_ETHERDEVICE_H

#include <linux/if_ether.h>
#include <linux/netdevice.h>

struct net_device *alloc_etherdev_mqs(int sizeof_priv, unsigned int txqs,
				      unsigned int rxqs);

static inline int eth_validate_addr(struct net_device *dev)
{
	if (!is_valid_ether_addr(dev->dev_addr))
		return -1; 	/* Linux returns EADDRNOTAVAIL on fail */

	return 0;
}

/**
 * eth_hw_addr_random - Generate software assigned random Ethernet MAC
 * @dev: pointer to net_device structure
 * Generate a random Ethernet address (MAC) to be used by a net device.
 */
static inline void eth_hw_addr_random(struct net_device *dev)
{
	random_ether_addr(dev->dev_addr);
}

/**
 * print_mac_address_fmt - Print address in MAC address format of ":"
 * @addr - Pointer to the address to be printed in MAC address format.
 */
static inline void print_mac_address_fmt(unsigned char *addr)
{
	int i;

	for (i = 0; i < 5; i++)
		vmm_printf("%02X:", addr[i]);

	vmm_printf("%02X\n", addr[5]);
}

#endif
