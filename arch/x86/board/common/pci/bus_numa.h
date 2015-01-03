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
 * @file bus_numa.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 *
 * All the work under arch/x86/board/common/pci/ is a derived work from Linux's
 * PCI Framework. The following is the commit ID from which it has
 * been derived.
 *
 * commit 97bf6af1f928216fd6c5a66e8a57bfa95a659672
 * Linux 3.19-rc1
 */
#ifndef __BUS_NUMA_H
#define __BUS_NUMA_H
/*
 * sub bus (transparent) will use entres from 3 to store extra from
 * root, so need to make sure we have enough slot there.
 */
struct pci_root_res {
	struct list_head list;
	struct resource res;
};

struct pci_root_info {
	struct list_head list;
	char name[12];
	struct list_head resources;
	struct resource busn;
	int node;
	int link;
};

extern struct list_head pci_root_infos;
struct pci_root_info *alloc_pci_root_info(int bus_min, int bus_max,
						int node, int link);
extern void update_res(struct pci_root_info *info, resource_size_t start,
		      resource_size_t end, unsigned long flags, int merge);
#endif
