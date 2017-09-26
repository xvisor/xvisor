/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_virtio_rpmsg.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO RPMSG Framework Interface.
 *
 * This header has been derived from linux kernel source:
 * <linux_source>/drivers/rpmsg/virtio_rpmsg_bus.c
 *
 * The original header is GPL licensed.
 */

#ifndef __VMM_VIRTIO_RPMSG_H__
#define __VMM_VIRTIO_RPMSG_H__

#include <vmm_types.h>

#define VMM_RPMSG_NAME_SIZE	32

/* The feature bitmap for virtio rpmsg */
#define VMM_VIRTIO_RPMSG_F_NS	0 /* RP supports name service notifications */

/**
 * Common header for all rpmsg messages
 * @src: source address
 * @dst: destination address
 * @reserved: reserved for future use
 * @len: length of payload (in bytes)
 * @flags: message flags
 * @data: @len bytes of message payload data
 *
 * Every message sent(/received) on the rpmsg bus begins with this header.
 */
struct vmm_rpmsg_hdr {
	u32 src;
	u32 dst;
	u32 reserved;
	u16 len;
	u16 flags;
	u8 data[0];
} __attribute__((packed));

/**
 * Dynamic name service announcement message
 * @name: name of remote service that is published
 * @addr: address of remote service that is published
 * @flags: indicates whether service is created or destroyed
 *
 * This message is sent across to publish a new service, or announce
 * about its removal.
 */
struct vmm_rpmsg_ns_msg {
	char name[VMM_RPMSG_NAME_SIZE];
	u32 addr;
	u32 flags;
} __attribute__((packed));

/**
 * Dynamic name service announcement flags
 *
 * @VMM_RPMSG_NS_CREATE: a new remote service was just created
 * @VMM_RPMSG_NS_DESTROY: a known remote service was just destroyed
 */
enum vmm_rpmsg_ns_flags {
	VMM_RPMSG_NS_CREATE	= 0,
	VMM_RPMSG_NS_DESTROY	= 1,
};

#endif /* __VMM_VIRTIO_RPMSG_H__ */
