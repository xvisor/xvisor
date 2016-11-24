/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file vmm_iommu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief IOMMU framework implementation for device pass-through
 *
 * The source has been largely adapted from Linux sources:
 * drivers/iommu/iommu.c
 *
 * Copyright (C) 2007-2008 Advanced Micro Devices, Inc.
 * Author: Joerg Roedel <joerg.roedel@amd.com>
 *
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_compiler.h>
#include <vmm_heap.h>
#include <vmm_spinlocks.h>
#include <vmm_mutex.h>
#include <vmm_notifier.h>
#include <vmm_devdrv.h>
#include <vmm_stdio.h>
#include <vmm_iommu.h>
#include <arch_atomic.h>
#include <libs/bitops.h>
#include <libs/stringlib.h>

#undef DEBUG

#ifdef DEBUG
#define pr_debug(msg...)		vmm_printf(msg)
#else
#define pr_debug(msg...)
#endif

struct vmm_iommu_group {
	atomic_t ref_count;
	struct dlist devices;
	struct vmm_mutex mutex;
	struct vmm_blocking_notifier_chain notifier;
	void *iommu_data;
	void (*iommu_data_release)(void *iommu_data);
	char *name;
	int id;
};

struct vmm_iommu_device {
	struct dlist list;
	struct vmm_device *dev;
	char *name;
};

static vmm_rwlock_t iommu_groups_lock;
static struct vmm_iommu_group *iommu_groups[CONFIG_IOMMU_MAX_GROUPS];
static const struct vmm_devtree_nodeid *iommu_matches;

struct vmm_iommu_group *vmm_iommu_group_alloc(void)
{
	int id;
	irq_flags_t flags;
	struct vmm_iommu_group *group;

	group = vmm_zalloc(sizeof(*group));
	if (!group) {
		return VMM_ERR_PTR(VMM_ENOMEM);
	}

	arch_atomic_write(&group->ref_count, 1);
	INIT_MUTEX(&group->mutex);
	INIT_LIST_HEAD(&group->devices);
	BLOCKING_INIT_NOTIFIER_CHAIN(&group->notifier);

	vmm_write_lock_irqsave_lite(&iommu_groups_lock, flags);
	for (id = 0; id < CONFIG_IOMMU_MAX_GROUPS; id++) {
		if (!iommu_groups[id]) {
			iommu_groups[id] = group;
			group->id = id;
			break;
		}
	}
	vmm_write_unlock_irqrestore_lite(&iommu_groups_lock, flags);

	if (id < 0 || CONFIG_IOMMU_MAX_GROUPS <= id) {
		vmm_free(group);
		return VMM_ERR_PTR(VMM_ENOSPC);
	}

	return group;
}

struct vmm_iommu_group *vmm_iommu_group_get_by_id(int id)
{
	irq_flags_t flags;
	struct vmm_iommu_group *group;

	if (id < 0 || CONFIG_IOMMU_MAX_GROUPS <= id) {
		return NULL;
	}

	vmm_read_lock_irqsave_lite(&iommu_groups_lock, flags);
	group = iommu_groups[id];
	vmm_read_unlock_irqrestore_lite(&iommu_groups_lock, flags);

	arch_atomic_inc(&group->ref_count);

	return group;
}

void *vmm_iommu_group_get_iommudata(struct vmm_iommu_group *group)
{
	return group->iommu_data;
}

void vmm_iommu_group_set_iommudata(struct vmm_iommu_group *group,
				   void *iommu_data,
				   void (*release)(void *iommu_data))
{
	group->iommu_data = iommu_data;
	group->iommu_data_release = release;
}

int vmm_iommu_group_set_name(struct vmm_iommu_group *group,
			     const char *name)
{
	if (group->name) {
		vmm_free(group->name);
		group->name = NULL;
		if (!name)
			return VMM_OK;
	}

	group->name = vmm_zalloc(strlen(name) + 1);
	if (!group->name)
		return VMM_ENOMEM;

	strcpy(group->name, name);

	return VMM_OK;
}

int vmm_iommu_group_add_device(struct vmm_iommu_group *group,
				struct vmm_device *dev)
{
	struct vmm_iommu_device *device;

	vmm_mutex_lock(&group->mutex);

	list_for_each_entry(device, &group->devices, list) {
		if (device->dev == dev) {
			vmm_mutex_unlock(&group->mutex);
			return VMM_EEXIST;
		}
	}

	device = vmm_zalloc(sizeof(*device));
	if (!device) {
		vmm_mutex_unlock(&group->mutex);
		return VMM_ENOMEM;
	}

	device->dev = dev;
	dev->iommu_group = group;
	arch_atomic_inc(&group->ref_count);
	list_add_tail(&device->list, &group->devices);

	vmm_mutex_unlock(&group->mutex);

	/* Notify any listeners about change to group. */
	vmm_blocking_notifier_call(&group->notifier,
				VMM_IOMMU_GROUP_NOTIFY_ADD_DEVICE, dev);

	return 0;
}

void vmm_iommu_group_remove_device(struct vmm_device *dev)
{
	struct vmm_iommu_group *group = dev->iommu_group;
	struct vmm_iommu_device *tmp_device, *device = NULL;

	/* Pre-notify listeners that a device is being removed. */
	vmm_blocking_notifier_call(&group->notifier,
				VMM_IOMMU_GROUP_NOTIFY_DEL_DEVICE, dev);

	vmm_mutex_lock(&group->mutex);

	list_for_each_entry(tmp_device, &group->devices, list) {
		if (tmp_device->dev == dev) {
			device = tmp_device;
			list_del(&device->list);
			break;
		}
	}

	vmm_mutex_unlock(&group->mutex);

	if (!device)
		return;

	vmm_free(device->name);
	vmm_free(device);
	dev->iommu_group = NULL;

	vmm_iommu_group_put(group);
}

int vmm_iommu_group_for_each_dev(struct vmm_iommu_group *group, void *data,
				 int (*fn)(struct vmm_device *, void *))
{
	struct vmm_iommu_device *device;
	int ret = 0;

	vmm_mutex_lock(&group->mutex);
	list_for_each_entry(device, &group->devices, list) {
		ret = fn(device->dev, data);
		if (ret)
			break;
	}
	vmm_mutex_unlock(&group->mutex);

	return ret;
}

struct vmm_iommu_group *vmm_iommu_group_get(struct vmm_device *dev)
{
	struct vmm_iommu_group *group = dev->iommu_group;

	if (group)
		arch_atomic_inc(&group->ref_count);

	return group;
}

void vmm_iommu_group_free(struct vmm_iommu_group *group)
{
	irq_flags_t flags;

	if (!group) {
		return;
	}

	if (arch_atomic_sub_return(&group->ref_count, 1)) {
		return;
	}

	vmm_write_lock_irqsave_lite(&iommu_groups_lock, flags);
	iommu_groups[group->id] = NULL;
	vmm_write_unlock_irqrestore_lite(&iommu_groups_lock, flags);

	vmm_free(group);
}

int vmm_iommu_group_register_notifier(struct vmm_iommu_group *group,
				      struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_register(&group->notifier, nb);
}

int vmm_iommu_group_unregister_notifier(struct vmm_iommu_group *group,
					struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_unregister(&group->notifier, nb);
}

int vmm_iommu_group_id(struct vmm_iommu_group *group)
{
	return group->id;
}

static int add_iommu_group(struct vmm_device *dev, void *data)
{
	struct vmm_iommu_ops *ops = data;

	if (!ops->add_device)
		return VMM_ENODEV;

	WARN_ON(dev->iommu_group);

	ops->add_device(dev);

	return 0;
}

static int iommu_bus_notifier(struct vmm_notifier_block *nb,
			      unsigned long action, void *data)
{
	struct vmm_device *dev = data;
	struct vmm_iommu_ops *ops = dev->bus->iommu_ops;
	struct vmm_iommu_group *group;
	unsigned long group_action = 0;

	/*
	 * ADD/DEL call into iommu driver ops if provided, which may
	 * result in ADD/DEL notifiers to group->notifier
	 */
	if (action == VMM_BUS_NOTIFY_ADD_DEVICE) {
		if (ops->add_device)
			return ops->add_device(dev);
	} else if (action == VMM_BUS_NOTIFY_DEL_DEVICE) {
		if (ops->remove_device && dev->iommu_group) {
			ops->remove_device(dev);
			return 0;
		}
	}

	/*
	 * Remaining BUS_NOTIFYs get filtered and republished to the
	 * group, if anyone is listening
	 */
	group = vmm_iommu_group_get(dev);
	if (!group)
		return 0;

	switch (action) {
	case VMM_BUS_NOTIFY_BIND_DRIVER:
		group_action = VMM_IOMMU_GROUP_NOTIFY_BIND_DRIVER;
		break;
	case VMM_BUS_NOTIFY_BOUND_DRIVER:
		group_action = VMM_IOMMU_GROUP_NOTIFY_BOUND_DRIVER;
		break;
	case VMM_BUS_NOTIFY_UNBIND_DRIVER:
		group_action = VMM_IOMMU_GROUP_NOTIFY_UNBIND_DRIVER;
		break;
	case VMM_BUS_NOTIFY_UNBOUND_DRIVER:
		group_action = VMM_IOMMU_GROUP_NOTIFY_UNBOUND_DRIVER;
		break;
	}

	if (group_action)
		vmm_blocking_notifier_call(&group->notifier,
					   group_action, dev);

	vmm_iommu_group_put(group);
	return 0;
}

static struct vmm_notifier_block iommu_bus_nb = {
	.notifier_call = iommu_bus_notifier,
};

static void iommu_bus_init(struct vmm_bus *bus, struct vmm_iommu_ops *ops)
{
	vmm_devdrv_bus_register_notifier(bus, &iommu_bus_nb);
	vmm_devdrv_bus_device_iterate(bus, NULL, ops, add_iommu_group);
}

int vmm_bus_set_iommu(struct vmm_bus *bus, struct vmm_iommu_ops *ops)
{
	if (bus->iommu_ops != NULL)
		return VMM_EBUSY;

	bus->iommu_ops = ops;

	/* Do IOMMU specific setup for this bus-type */
	iommu_bus_init(bus, ops);

	return VMM_OK;
}

bool vmm_iommu_present(struct vmm_bus *bus)
{
	return bus->iommu_ops != NULL;
}

void vmm_iommu_set_fault_handler(struct vmm_iommu_domain *domain,
				 vmm_iommu_fault_handler_t handler,
				 void *token)
{
	BUG_ON(!domain);

	domain->handler = handler;
	domain->handler_token = token;
}

struct vmm_iommu_domain *vmm_iommu_domain_alloc(struct vmm_bus *bus)
{
	struct vmm_iommu_domain *domain;
	int ret;

	if (bus == NULL || bus->iommu_ops == NULL)
		return NULL;

	domain = vmm_zalloc(sizeof(*domain));
	if (!domain)
		return NULL;

	domain->ops = bus->iommu_ops;

	ret = domain->ops->domain_init(domain);
	if (ret)
		goto out_free;

	return domain;

out_free:
	vmm_free(domain);

	return NULL;
}

void vmm_iommu_domain_free(struct vmm_iommu_domain *domain)
{
	if (likely(domain->ops->domain_destroy != NULL))
		domain->ops->domain_destroy(domain);

	vmm_free(domain);
}

static int iommu_attach_device(struct vmm_iommu_domain *domain,
				struct vmm_device *dev)
{
	if (unlikely(domain->ops->attach_dev == NULL))
		return VMM_ENODEV;

	return domain->ops->attach_dev(domain, dev);
}

static void iommu_detach_device(struct vmm_iommu_domain *domain,
				struct vmm_device *dev)
{
	if (unlikely(domain->ops->detach_dev == NULL))
		return;

	domain->ops->detach_dev(domain, dev);
}

/*
 * IOMMU groups are really the natrual working unit of the IOMMU, but
 * the IOMMU API works on domains and devices.  Bridge that gap by
 * iterating over the devices in a group.  Ideally we'd have a single
 * device which represents the requestor ID of the group, but we also
 * allow IOMMU drivers to create policy defined minimum sets, where
 * the physical hardware may be able to distiguish members, but we
 * wish to group them at a higher level (ex. untrusted multi-function
 * PCI devices).  Thus we attach each device.
 */
static int iommu_group_do_attach_device(struct vmm_device *dev, void *data)
{
	struct vmm_iommu_domain *domain = data;

	return iommu_attach_device(domain, dev);
}

int vmm_iommu_attach_group(struct vmm_iommu_domain *domain,
			   struct vmm_iommu_group *group)
{
	return vmm_iommu_group_for_each_dev(group, domain,
					iommu_group_do_attach_device);
}

static int iommu_group_do_detach_device(struct vmm_device *dev, void *data)
{
	struct vmm_iommu_domain *domain = data;

	iommu_detach_device(domain, dev);

	return VMM_OK;
}

void vmm_iommu_detach_group(struct vmm_iommu_domain *domain,
			    struct vmm_iommu_group *group)
{
	vmm_iommu_group_for_each_dev(group, domain,
					iommu_group_do_detach_device);
}

physical_addr_t vmm_iommu_iova_to_phys(struct vmm_iommu_domain *domain,
				       physical_addr_t iova)
{
	if (unlikely(domain->ops->iova_to_phys == NULL))
		return 0;

	return domain->ops->iova_to_phys(domain, iova);
}

int vmm_iommu_domain_has_cap(struct vmm_iommu_domain *domain,
			     unsigned long cap)
{
	if (unlikely(domain->ops->domain_has_cap == NULL))
		return 0;

	return domain->ops->domain_has_cap(domain, cap);
}

static size_t iommu_pgsize(struct vmm_iommu_domain *domain,
			   unsigned long addr_merge, size_t size)
{
	unsigned int pgsize_idx;
	size_t pgsize;

	/* Max page size that still fits into 'size' */
	pgsize_idx = __fls(size);

	/* need to consider alignment requirements ? */
	if (likely(addr_merge)) {
		/* Max page size allowed by address */
		unsigned int align_pgsize_idx = __ffs(addr_merge);
		pgsize_idx = min(pgsize_idx, align_pgsize_idx);
	}

	/* build a mask of acceptable page sizes */
	pgsize = (1UL << (pgsize_idx + 1)) - 1;

	/* throw away page sizes not supported by the hardware */
	pgsize &= domain->ops->pgsize_bitmap;

	/* make sure we're still sane */
	BUG_ON(!pgsize);

	/* pick the biggest page */
	pgsize_idx = __fls(pgsize);
	pgsize = 1UL << pgsize_idx;

	return pgsize;
}

int vmm_iommu_map(struct vmm_iommu_domain *domain, physical_addr_t iova,
		  physical_addr_t paddr, size_t size, int prot)
{
	unsigned long orig_iova = iova;
	size_t min_pagesz;
	size_t orig_size = size;
	int ret = 0;

	if (unlikely(domain->ops->unmap == NULL ||
		     domain->ops->pgsize_bitmap == 0UL))
		return VMM_ENODEV;

	/* find out the minimum page size supported */
	min_pagesz = 1 << __ffs(domain->ops->pgsize_bitmap);

	/*
	 * both the virtual address and the physical one, as well as
	 * the size of the mapping, must be aligned (at least) to the
	 * size of the smallest page supported by the hardware
	 */
	if (!is_aligned(iova | paddr | size, min_pagesz)) {
		vmm_lerror("IOMMU", "unaligned iova 0x%"PRIPADDR
			   " pa 0x%"PRIPADDR" size 0x%zx "
			   "min_pagesz 0x%zx\n", iova, paddr,
			   size, min_pagesz);
		return VMM_EINVALID;
	}

	pr_debug("IOMMU: map iova 0x%"PRIPADDR
		 " pa 0x%"PRIPADDR" size 0x%zx\n",
		 iova, paddr, size);

	while (size) {
		size_t pgsize = iommu_pgsize(domain, iova | paddr, size);

		pr_debug("IOMMU: mapping iova 0x%"PRIPADDR
			 " pa 0x%"PRIPADDR" size 0x%zx\n",
			 iova, paddr, pgsize);

		ret = domain->ops->map(domain, iova, paddr, pgsize, prot);
		if (ret)
			break;

		iova += pgsize;
		paddr += pgsize;
		size -= pgsize;
	}

	/* unroll mapping in case something went wrong */
	if (ret)
		vmm_iommu_unmap(domain, orig_iova, orig_size - size);

	return ret;
}

size_t vmm_iommu_unmap(struct vmm_iommu_domain *domain,
			physical_addr_t iova, size_t size)
{
	size_t unmapped_page, min_pagesz, unmapped = 0;

	if (unlikely(domain->ops->unmap == NULL ||
		     domain->ops->pgsize_bitmap == 0UL))
		return VMM_ENODEV;

	/* find out the minimum page size supported */
	min_pagesz = 1 << __ffs(domain->ops->pgsize_bitmap);

	/*
	 * The virtual address, as well as the size of the mapping, must be
	 * aligned (at least) to the size of the smallest page supported
	 * by the hardware
	 */
	if (!is_aligned(iova | size, min_pagesz)) {
		vmm_lerror("IOMMU", "unaligned iova 0x%"PRIPADDR
			   " size 0x%zx min_pagesz 0x%zx\n", iova,
			   size, min_pagesz);
		return VMM_EINVALID;
	}

	pr_debug("IOMMU: unmap iova 0x%"PRIPADDR" size 0x%zx\n",
		 iova, size);

	/*
	 * Keep iterating until we either unmap 'size' bytes (or more)
	 * or we hit an area that isn't mapped.
	 */
	while (unmapped < size) {
		size_t pgsize = iommu_pgsize(domain, iova, size - unmapped);

		unmapped_page = domain->ops->unmap(domain, iova, pgsize);
		if (!unmapped_page)
			break;

		pr_debug("IOMMU: unmapped iova 0x%"PRIPADDR" size 0x%zx\n",
			 iova, unmapped_page);

		iova += unmapped_page;
		unmapped += unmapped_page;
	}

	return unmapped;
}

int vmm_iommu_domain_window_enable(struct vmm_iommu_domain *domain,
				   u32 wnd_nr, physical_addr_t paddr,
				   u64 size, int prot)
{
	if (unlikely(domain->ops->domain_window_enable == NULL))
		return VMM_ENODEV;

	return domain->ops->domain_window_enable(domain, wnd_nr,
						 paddr, size, prot);
}

void vmm_iommu_domain_window_disable(struct vmm_iommu_domain *domain,
				     u32 wnd_nr)
{
	if (unlikely(domain->ops->domain_window_disable == NULL))
		return;

	return domain->ops->domain_window_disable(domain, wnd_nr);
}

int vmm_iommu_domain_get_attr(struct vmm_iommu_domain *domain,
			      enum vmm_iommu_attr attr, void *data)
{
	struct vmm_iommu_domain_geometry *geometry;
	bool *paging;
	int ret = 0;
	u32 *count;

	switch (attr) {
	case VMM_DOMAIN_ATTR_GEOMETRY:
		geometry  = data;
		*geometry = domain->geometry;

		break;
	case VMM_DOMAIN_ATTR_PAGING:
		paging  = data;
		*paging = (domain->ops->pgsize_bitmap != 0UL);
		break;
	case VMM_DOMAIN_ATTR_WINDOWS:
		count = data;

		if (domain->ops->domain_get_windows != NULL)
			*count = domain->ops->domain_get_windows(domain);
		else
			ret = VMM_ENODEV;

		break;
	default:
		if (!domain->ops->domain_get_attr)
			return VMM_EINVALID;

		ret = domain->ops->domain_get_attr(domain, attr, data);
	}

	return ret;
}

int vmm_iommu_domain_set_attr(struct vmm_iommu_domain *domain,
			      enum vmm_iommu_attr attr, void *data)
{
	int ret = 0;
	u32 *count;

	switch (attr) {
	case VMM_DOMAIN_ATTR_WINDOWS:
		count = data;

		if (domain->ops->domain_set_windows != NULL)
			ret = domain->ops->domain_set_windows(domain, *count);
		else
			ret = VMM_ENODEV;

		break;
	default:
		if (domain->ops->domain_set_attr == NULL)
			return VMM_EINVALID;

		ret = domain->ops->domain_set_attr(domain, attr, data);
	}

	return ret;
}

static void __init iommu_nidtbl_found(struct vmm_devtree_node *node,
				      const struct vmm_devtree_nodeid *match,
				      void *data)
{
	int err;
	vmm_iommu_init_t init_fn = match->data;

	if (!init_fn) {
		return;
	}

	err = init_fn(node);
#ifdef CONFIG_VERBOSE_MODE
	if (err) {
		vmm_printf("%s: Init %s node failed (error %d)\n",
			   __func__, node->name, err);
	}
#else
	(void)err;
#endif
}

int __init vmm_iommu_init(void)
{
	memset(iommu_groups, 0, sizeof(iommu_groups));

	INIT_RW_LOCK(&iommu_groups_lock);

	/* Probe all device tree nodes matching
	 * IOMMU nodeid table enteries.
	 */
	iommu_matches =  vmm_devtree_nidtbl_create_matches("iommu");
	if (iommu_matches) {
		vmm_devtree_iterate_matching(NULL,
					     iommu_matches,
					     iommu_nidtbl_found,
					     NULL);
	}

	return VMM_OK;
}
