/**
 * Copyright (c) 2013 Pranav Sawargaonkar.
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
 * @file vmm_virtio_mmio.h
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO MMIO Transport Interface
 */

#ifndef __VMM_VIRTIO_MMIO_H__
#define __VMM_VIRTIO_MMIO_H__

/*
 * Control registers --> Copied from linux's virtio_mmio.h
 */

/* Magic value ("virt" string) - Read Only */
#define VMM_VIRTIO_MMIO_MAGIC_VALUE		0x000

/* Virtio device version - Read Only */
#define VMM_VIRTIO_MMIO_VERSION			0x004

/* Virtio device ID - Read Only */
#define VMM_VIRTIO_MMIO_DEVICE_ID		0x008

/* Virtio vendor ID - Read Only */
#define VMM_VIRTIO_MMIO_VENDOR_ID		0x00c

/* Bitmask of the features supported by the host
 * (32 bits per set) - Read Only */
#define VMM_VIRTIO_MMIO_HOST_FEATURES		0x010

/* Host features set selector - Write Only */
#define VMM_VIRTIO_MMIO_HOST_FEATURES_SEL	0x014

/* Bitmask of features activated by the guest
 * (32 bits per set) - Write Only */
#define VMM_VIRTIO_MMIO_GUEST_FEATURES		0x020

/* Activated features set selector - Write Only */
#define VMM_VIRTIO_MMIO_GUEST_FEATURES_SEL	0x024

/* Guest's memory page size in bytes - Write Only */
#define VMM_VIRTIO_MMIO_GUEST_PAGE_SIZE		0x028

/* Queue selector - Write Only */
#define VMM_VIRTIO_MMIO_QUEUE_SEL		0x030

/* Maximum size of the currently selected queue - Read Only */
#define VMM_VIRTIO_MMIO_QUEUE_NUM_MAX		0x034

/* Queue size for the currently selected queue - Write Only */
#define VMM_VIRTIO_MMIO_QUEUE_NUM		0x038

/* Used Ring alignment for the currently selected queue - Write Only */
#define VMM_VIRTIO_MMIO_QUEUE_ALIGN		0x03c

/* Guest's PFN for the currently selected queue - Read Write */
#define VMM_VIRTIO_MMIO_QUEUE_PFN		0x040

/* Queue notifier - Write Only */
#define VMM_VIRTIO_MMIO_QUEUE_NOTIFY		0x050

/* Interrupt status - Read Only */
#define VMM_VIRTIO_MMIO_INTERRUPT_STATUS	0x060

/* Interrupt acknowledge - Write Only */
#define VMM_VIRTIO_MMIO_INTERRUPT_ACK		0x064

/* Device status register - Read Write */
#define VMM_VIRTIO_MMIO_STATUS			0x070

/* The config space is defined by each driver as
 * the per-driver configuration space - Read Write */
#define VMM_VIRTIO_MMIO_CONFIG			0x100


/*
 * Interrupt flags (re: interrupt status & acknowledge registers)
 */

#define VMM_VIRTIO_MMIO_INT_VRING		(1 << 0)
#define VMM_VIRTIO_MMIO_INT_CONFIG		(1 << 1)

#define VMM_VIRTIO_MMIO_MAX_VQ			3
#define VMM_VIRTIO_MMIO_MAX_CONFIG		1
#define VMM_VIRTIO_MMIO_IO_SIZE			0x200

struct vmm_virtio_mmio_config {
	char    magic[4];
	u32     version;
	u32     device_id;
	u32     vendor_id;
	u32     host_features;
	u32     host_features_sel;
	u32     reserved_1[2];
	u32     guest_features;
	u32     guest_features_sel;
	u32     guest_page_size;
	u32     reserved_2;
	u32     queue_sel;
	u32     queue_num_max;
	u32     queue_num;
	u32     queue_align;
	u32     queue_pfn;
	u32     reserved_3[3];
	u32     queue_notify;
	u32     reserved_4[3];
	u32     interrupt_state;
	u32     interrupt_ack;
	u32     reserved_5[2];
	u32     status;
} __attribute__((packed));

#endif /* __VMM_VIRTIO_MMIO_H__ */
