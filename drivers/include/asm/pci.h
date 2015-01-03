/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file pci.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 *
 * Only the required structures have been taken from Linux.
 *
 * commit 97bf6af1f928216fd6c5a66e8a57bfa95a659672
 * Linux 3.19-rc1
 */
#ifndef __ASM_PCI_H
#define __ASM_PCI_H

#include <vmm_types.h>

/* Place holder */
extern physical_addr_t pci_mem_start;
#define PCIBIOS_MIN_IO		0x1000
#define PCIBIOS_MIN_MEM		(pci_mem_start)

struct pci_sysdata {
	int		domain;		/* PCI domain */
	int		node;		/* NUMA node */
#if 0
	struct acpi_device *companion;	/* ACPI companion device */
#endif
#ifdef CONFIG_X86_64
	void		*iommu;		/* IOMMU private data */
#endif
};

struct setup_data {
};

struct pci_setup_rom {
	struct setup_data data;
	uint16_t vendor;
	uint16_t devid;
	uint64_t pcilen;
	unsigned long segment;
	unsigned long bus;
	unsigned long device;
	unsigned long function;
	uint8_t romdata[0];
};

extern int pci_routeirq;

#endif /* __ASM_PCI_H */
