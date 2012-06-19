/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file ethernet.h
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Ethernet Helper
 *
 * Portions of this file have been adapted from linux source header
 * include/linux/etherdevice.hi which is licensed under GPLv2:
 *
 * Original authors:
 * 	Ross Biro
 *	Fred N. van Kempen <waltje@uWalt.NL.Mugnet.ORG>
 *	Alan Cox <gw4pts@gw4pts.ampr.org> 
 *
 */

#include <vmm_types.h>
#include <vmm_host_io.h>
#include <vmm_timer.h>

/**
 * is_zero_ether_addr - Determine if give Ethernet address is all zeros.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is all zeroes.
 */
static inline int is_zero_ether_addr(const u8 *addr)
{
	return !(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}

/**
 * is_multicast_ether_addr - Determine if the Ethernet address is a multicast.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a multicast address.
 * By definition the broadcast address is also a multicast address.
 */
static inline int is_multicast_ether_addr(const u8 *addr)
{
	return 0x01 & addr[0];
}

/**
 * is_local_ether_addr - Determine if the Ethernet address is locally-assigned one (IEEE 802).
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a local address.
 */
static inline int is_local_ether_addr(const u8 *addr)
{
	return 0x02 & addr[0];
}

/**
 * is_broadcast_ether_addr - Determine if the Ethernet address is broadcast
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is the broadcast address.
 */
static inline int is_broadcast_ether_addr(const u8 *addr)
{
	return (addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) == 0xff;
}

/**
 * is_unicast_ether_addr - Determine if the Ethernet address is unicast
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a unicast address.
 */
static inline int is_unicast_ether_addr(const u8 *addr)
{
	return !is_multicast_ether_addr(addr);
}

/**
 * is_valid_ether_addr - Determine if the given Ethernet address is valid
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Check that the Ethernet address (MAC) is not 00:00:00:00:00:00, is not
 * a multicast address, and is not FF:FF:FF:FF:FF:FF.
 *
 * Return true if the address is valid.
 */
static inline int is_valid_ether_addr(const u8 *addr)
{
	/* FF:FF:FF:FF:FF:FF is a multicast address so we don't need to
	 * explicitly check for it here. */
	return !is_multicast_ether_addr(addr) && !is_zero_ether_addr(addr);
}

static inline void get_random_bytes(u8 *buf, int len)
{
	u64 tstamp;
	int off = 0;
	while(len > 0) {
		tstamp = vmm_timer_timestamp();
		if(len < sizeof(u64)) {
			vmm_memcpy(buf + off, &tstamp, len);
			off += len;
			len -= len;
		} else {
			vmm_memcpy(buf + off, &tstamp, sizeof(u64));
			off += sizeof(u64);
			len -= sizeof(u64);
		}
	}
}

/**
 * random_ether_addr - Generate software assigned random Ethernet address
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Generate a random Ethernet address (MAC) that is not multicast
 * and has the local assigned bit set.
 */
static inline void random_ether_addr(u8 *addr)
{
	get_random_bytes (addr, 6);
	addr [0] &= 0xfe;	/* clear multicast bit */
	addr [0] |= 0x02;	/* set local assignment bit (IEEE802) */
}

/**
 * compare_ether_addr - Compare two Ethernet addresses
 * @addr1: Pointer to a six-byte array containing the Ethernet address
 * @addr2: Pointer other six-byte array containing the Ethernet address
 *
 * Compare two ethernet addresses, returns 0 if equal
 */
static inline unsigned compare_ether_addr(const u8 *addr1, const u8 *addr2)
{
	const u16 *a = (const u16 *) addr1;
	const u16 *b = (const u16 *) addr2;

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) != 0;
}

#define ether_srcmac(ether_hdr)	((u8 *)(ether_hdr)+6)
#define ether_dstmac(ether_hdr)	((u8 *)(ether_hdr)+0)
#define ether_type(ether_hdr)	vmm_be16_to_cpu(*(u16 *)((u8 *)(ether_hdr)+12))

