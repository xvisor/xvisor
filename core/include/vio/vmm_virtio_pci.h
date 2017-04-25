/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file vmm_virtio_pci.h
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief VirtIO PCI Transport Interface
 */

#ifndef __VMM_VIRTIO_PCI_H__
#define __VMM_VIRTIO_PCI_H__

/* from Linux's linux/virtio_pci.h */

/* A 32-bit r/o bitmask of the features supported by the host */
#define VMM_VIRTIO_PCI_HOST_FEATURES		0

/* A 32-bit r/w bitmask of features activated by the guest */
#define VMM_VIRTIO_PCI_GUEST_FEATURES		4

/* A 32-bit r/w PFN for the currently selected queue */
#define VMM_VIRTIO_PCI_QUEUE_PFN		8

/* A 16-bit r/o queue size for the currently selected queue */
#define VMM_VIRTIO_PCI_QUEUE_NUM		12

/* A 16-bit r/w queue selector */
#define VMM_VIRTIO_PCI_QUEUE_SEL		14

/* A 16-bit r/w queue notifier */
#define VMM_VIRTIO_PCI_QUEUE_NOTIFY		16

/* An 8-bit device status register.  */
#define VMM_VIRTIO_PCI_STATUS			18

/* An 8-bit r/o interrupt status register.  Reading the value will return the
 * current contents of the ISR and will also clear it.  This is effectively
 * a read-and-acknowledge. */
#define VMM_VIRTIO_PCI_ISR			19

#define VMM_VIRTIO_PCI_REGION_SIZE		(VMM_VIRTIO_PCI_ISR)

/* The remaining space is defined by each driver as the per-driver
 * configuration space */
#define VMM_VIRTIO_PCI_CONFIG			(20)

/* How many bits to shift physical queue address written to QUEUE_PFN.
 * 12 is historical, and due to x86 page size. */
#define VMM_VIRTIO_PCI_QUEUE_ADDR_SHIFT		12

/* Flags track per-device state like workarounds for quirks in older guests. */
#define VMM_VIRTIO_PCI_FLAG_BUS_MASTER_BUG	(1 << 0)

#define VMM_VIRTIO_PCI_INT_VRING		(1 << 0)
#define VMM_VIRTIO_PCI_INT_CONFIG		(1 << 1)

#define VMM_VIRTIO_PCI_QUEUE_MAX		64
#define VMM_VIRTIO_PCI_MAX_VQ			3
#define VMM_VIRTIO_PCI_MAX_CONFIG		1
#define VMM_VIRTIO_PCI_IO_SIZE			VMM_VIRTIO_PCI_REGION_SIZE
#define VMM_VIRTIO_PCI_PAGE_SIZE		(0x1UL << VMM_VIRTIO_PCI_QUEUE_ADDR_SHIFT)

struct vmm_virtio_pci_config {
	u32     host_features;
	u32	guest_features;
	u32	queue_pfn;
	u16	queue_num;
	u16	queue_sel;
	u16	queue_notify;
	u8	status;
	u8	interrupt_state;
} __attribute__((packed));

#define VMM_VIRTIO_PCI_O_CONFIG			0
#define VMM_VIRTIO_PCI_O_MSIX			1

#endif /* __VMM_VIRTIO_PCI_H__ */
