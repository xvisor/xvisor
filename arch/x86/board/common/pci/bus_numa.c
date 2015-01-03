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
 * @file bus_numa.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 *
 * All the work under arch/x86/board/common/pci/ is a derived work from Linux's
 * PCI Framework. The following is the commit ID from which it has
 * been derived.
 *
 * commit 97bf6af1f928216fd6c5a66e8a57bfa95a659672
 * Linux 3.19-rc1
 */
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/range.h>
#include <linux/numa.h>

#include "bus_numa.h"

LIST_HEAD(pci_root_infos);

static struct pci_root_info *x86_find_pci_root_info(int bus)
{
	struct pci_root_info *info;

	list_for_each_entry(info, &pci_root_infos, list)
		if (info->busn.start == bus)
			return info;

	return NULL;
}

int x86_pci_root_bus_node(int bus)
{
	struct pci_root_info *info = x86_find_pci_root_info(bus);

	if (!info)
		return NUMA_NO_NODE;

	return info->node;
}

void x86_pci_root_bus_resources(int bus, struct list_head *resources)
{
	struct pci_root_info *info = x86_find_pci_root_info(bus);
	struct pci_root_res *root_res;
	struct pci_host_bridge_window *window;
	bool found = false;

	if (!info)
		goto default_resources;

	printk(KERN_DEBUG "PCI: root bus %02x: hardware-probed resources\n",
	       bus);

	/* already added by acpi ? */
	list_for_each_entry(window, resources, list)
		if (window->res->flags & IORESOURCE_BUS) {
			found = true;
			break;
		}

	if (!found)
		pci_add_resource(resources, &info->busn);

	list_for_each_entry(root_res, &info->resources, list) {
		struct resource *res;
		struct resource *root;

		res = &root_res->res;
		pci_add_resource(resources, res);
		if (res->flags & IORESOURCE_IO)
			root = &ioport_resource;
		else
			root = &iomem_resource;
		insert_resource(root, res);
	}
	return;

default_resources:
	/*
	 * We don't have any host bridge aperture information from the
	 * "native host bridge drivers," e.g., amd_bus or broadcom_bus,
	 * so fall back to the defaults historically used by pci_create_bus().
	 */
	printk(KERN_DEBUG "PCI: root bus %02x: using default resources\n", bus);
	pci_add_resource(resources, &ioport_resource);
	pci_add_resource(resources, &iomem_resource);
}

struct pci_root_info __init *alloc_pci_root_info(int bus_min, int bus_max,
						 int node, int link)
{
	struct pci_root_info *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);

	if (!info)
		return info;

	sprintf(info->name, "PCI Bus #%02x", bus_min);

	INIT_LIST_HEAD(&info->resources);
	info->busn.name  = info->name;
	info->busn.start = bus_min;
	info->busn.end   = bus_max;
	info->busn.flags = IORESOURCE_BUS;
	info->node = node;
	info->link = link;

	list_add_tail(&info->list, &pci_root_infos);

	return info;
}

void update_res(struct pci_root_info *info, resource_size_t start,
		resource_size_t end, unsigned long flags, int merge)
{
	struct resource *res;
	struct pci_root_res *root_res;

	if (start > end)
		return;

	if (start == MAX_RESOURCE)
		return;

	if (!merge)
		goto addit;

	/* try to merge it with old one */
	list_for_each_entry(root_res, &info->resources, list) {
		resource_size_t final_start, final_end;
		resource_size_t common_start, common_end;

		res = &root_res->res;
		if (res->flags != flags)
			continue;

		common_start = max(res->start, start);
		common_end = min(res->end, end);
		if (common_start > common_end + 1)
			continue;

		final_start = min(res->start, start);
		final_end = max(res->end, end);

		res->start = final_start;
		res->end = final_end;
		return;
	}

addit:

	/* need to add that */
	root_res = kzalloc(sizeof(*root_res), GFP_KERNEL);
	if (!root_res)
		return;

	res = &root_res->res;
	res->name = info->name;
	res->flags = flags;
	res->start = start;
	res->end = end;

	list_add_tail(&root_res->list, &info->resources);
}
