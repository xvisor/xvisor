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
#include <vmm_mutex.h>
#include <vmm_notifier.h>
#include <vmm_stdio.h>
#include <vmm_iommu.h>
#include <libs/bitops.h>
#include <libs/stringlib.h>

#undef DEBUG

#ifdef DEBUG
#define pr_debug(msg...)		vmm_printf(msg)
#else
#define pr_debug(msg...)
#endif

struct vmm_iommu_group {
	char *name;
	struct vmm_iommu_controller *ctrl;
	struct dlist head;

	struct xref ref_count;
	struct vmm_mutex mutex;
	struct vmm_iommu_domain *domain;
	struct dlist devices;
	struct vmm_blocking_notifier_chain notifier;
	void *iommu_data;
	void (*iommu_data_release)(void *iommu_data);
};

struct vmm_iommu_device {
	struct dlist list;
	struct vmm_device *dev;
};

/* =============== IOMMU Controller APIs =============== */

static struct vmm_class iommuctrl_class = {
	.name = VMM_IOMMU_CONTROLLER_CLASS_NAME,
};

int vmm_iommu_controller_register(struct vmm_iommu_controller *ctrl)
{
	if (!ctrl) {
		return VMM_EINVALID;
	}

	vmm_devdrv_initialize_device(&ctrl->dev);
	if (strlcpy(ctrl->dev.name, ctrl->name, sizeof(ctrl->dev.name)) >=
	    sizeof(ctrl->dev.name)) {
		return VMM_EOVERFLOW;
	}
	ctrl->dev.class = &iommuctrl_class;
	vmm_devdrv_set_data(&ctrl->dev, ctrl);

	INIT_MUTEX(&ctrl->groups_lock);
	INIT_LIST_HEAD(&ctrl->groups);
	INIT_MUTEX(&ctrl->domains_lock);
	INIT_LIST_HEAD(&ctrl->domains);

	return vmm_devdrv_register_device(&ctrl->dev);
}

int vmm_iommu_controller_unregister(struct vmm_iommu_controller *ctrl)
{
	if (!ctrl) {
		return VMM_EFAIL;
	}

	return vmm_devdrv_unregister_device(&ctrl->dev);
}

struct vmm_iommu_controller *vmm_iommu_controller_find(const char *name)
{
	struct vmm_device *dev;

	dev = vmm_devdrv_class_find_device_by_name(&iommuctrl_class, name);
	if (!dev) {
		return NULL;
	}

	return vmm_devdrv_get_data(dev);
}

struct iommu_controller_iterate_priv {
	void *data;
	int (*fn)(struct vmm_iommu_controller *, void *);
};

static int iommu_controller_iterate(struct vmm_device *dev, void *data)
{
	struct iommu_controller_iterate_priv *p = data;
	struct vmm_iommu_controller *ctrl = vmm_devdrv_get_data(dev);

	return p->fn(ctrl, p->data);
}

int vmm_iommu_controller_iterate(struct vmm_iommu_controller *start,
		void *data, int (*fn)(struct vmm_iommu_controller *, void *))
{
	struct vmm_device *st = (start) ? &start->dev : NULL;
	struct iommu_controller_iterate_priv p;

	if (!fn) {
		return VMM_EINVALID;
	}

	p.data = data;
	p.fn = fn;

	return vmm_devdrv_class_device_iterate(&iommuctrl_class, st,
						&p, iommu_controller_iterate);
}

u32 vmm_iommu_controller_count(void)
{
	return vmm_devdrv_class_device_count(&iommuctrl_class);
}

int vmm_iommu_controller_for_each_group(struct vmm_iommu_controller *ctrl,
		void *data, int (*fn)(struct vmm_iommu_group *, void *))
{
	struct vmm_iommu_group *group;
	int ret = 0;

	if (!ctrl || !fn)
		return VMM_EINVALID;

	vmm_mutex_lock(&ctrl->groups_lock);

	list_for_each_entry(group, &ctrl->groups, head) {
		ret = fn(group, data);
		if (ret)
			break;
	}

	vmm_mutex_unlock(&ctrl->groups_lock);

	return ret;
}

static int iommu_controller_group_count_iter(struct vmm_iommu_group *group,
					     void *data)
{
	(*((u32 *)data))++;

	return VMM_OK;
}

u32 vmm_iommu_controller_group_count(struct vmm_iommu_controller *ctrl)
{
	u32 ret = 0;

	if (!ctrl)
		return 0;

	vmm_iommu_controller_for_each_group(ctrl, &ret,
					iommu_controller_group_count_iter);

	return ret;
}

int vmm_iommu_controller_for_each_domain(struct vmm_iommu_controller *ctrl,
		void *data, int (*fn)(struct vmm_iommu_domain *, void *))
{
	struct vmm_iommu_domain *domain;
	int ret = 0;

	if (!ctrl || !fn)
		return VMM_EINVALID;

	vmm_mutex_lock(&ctrl->domains_lock);

	list_for_each_entry(domain, &ctrl->domains, head) {
		ret = fn(domain, data);
		if (ret)
			break;
	}

	vmm_mutex_unlock(&ctrl->domains_lock);

	return ret;
}

static int iommu_controller_domain_count_iter(struct vmm_iommu_domain *domain,
					      void *data)
{
	(*((u32 *)data))++;

	return VMM_OK;
}

u32 vmm_iommu_controller_domain_count(struct vmm_iommu_controller *ctrl)
{
	u32 ret = 0;

	if (!ctrl)
		return 0;

	vmm_iommu_controller_for_each_domain(ctrl, &ret,
					iommu_controller_domain_count_iter);

	return ret;
}

/* =============== IOMMU Group APIs =============== */

struct vmm_iommu_group *vmm_iommu_group_alloc(const char *name,
					struct vmm_iommu_controller *ctrl)
{
	struct vmm_iommu_group *group;

	if (!name || !ctrl) {
		return VMM_ERR_PTR(VMM_EINVALID);
	}

	group = vmm_zalloc(sizeof(*group));
	if (!group) {
		return VMM_ERR_PTR(VMM_ENOMEM);
	}

	group->name = vmm_zalloc(strlen(name) + 1);
	if (!group->name) {
		vmm_free(group);
		return VMM_ERR_PTR(VMM_ENOMEM);
	}
	strcpy(group->name, name);
	group->name[strlen(name)] = '\0';
	group->ctrl = ctrl;
	INIT_LIST_HEAD(&group->head);

	xref_init(&group->ref_count);
	INIT_MUTEX(&group->mutex);
	INIT_LIST_HEAD(&group->devices);
	group->domain = NULL;
	BLOCKING_INIT_NOTIFIER_CHAIN(&group->notifier);

	vmm_mutex_lock(&ctrl->groups_lock);
	list_add_tail(&group->head, &ctrl->groups);
	vmm_mutex_unlock(&ctrl->groups_lock);

	return group;
}

struct vmm_iommu_group *vmm_iommu_group_get(struct vmm_device *dev)
{
	struct vmm_iommu_group *group = dev->iommu_group;

	if (group)
		xref_get(&group->ref_count);

	return group;
}

static void __iommu_group_free(struct xref *ref)
{
	struct vmm_iommu_group *group =
			container_of(ref, struct vmm_iommu_group, ref_count);

	vmm_mutex_lock(&group->ctrl->groups_lock);
	list_del(&group->head);
	vmm_mutex_unlock(&group->ctrl->groups_lock);

	vmm_free(group->name);

	if (group->iommu_data_release)
		group->iommu_data_release(group->iommu_data);

	vmm_free(group);
}

void vmm_iommu_group_free(struct vmm_iommu_group *group)
{
	if (group) {
		xref_put(&group->ref_count, __iommu_group_free);
	}
}

void *vmm_iommu_group_get_iommudata(struct vmm_iommu_group *group)
{
	return (group) ? group->iommu_data : NULL;
}

void vmm_iommu_group_set_iommudata(struct vmm_iommu_group *group,
				   void *iommu_data,
				   void (*release)(void *iommu_data))
{
	if (!group)
		return;

	group->iommu_data = iommu_data;
	group->iommu_data_release = release;
}

int vmm_iommu_group_add_device(struct vmm_iommu_group *group,
				struct vmm_device *dev)
{
	struct vmm_iommu_device *device;

	if (!group || !dev)
		return VMM_EINVALID;

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
	xref_get(&group->ref_count);
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

	if (!group)
		return;

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

	vmm_free(device);
	dev->iommu_group = NULL;

	vmm_iommu_group_put(group);
}

int vmm_iommu_group_for_each_dev(struct vmm_iommu_group *group, void *data,
				 int (*fn)(struct vmm_device *, void *))
{
	struct vmm_iommu_device *device;
	int ret = 0;

	if (!group || !fn)
		return VMM_EINVALID;

	vmm_mutex_lock(&group->mutex);

	list_for_each_entry(device, &group->devices, list) {
		ret = fn(device->dev, data);
		if (ret)
			break;
	}

	vmm_mutex_unlock(&group->mutex);

	return ret;
}

int vmm_iommu_group_register_notifier(struct vmm_iommu_group *group,
				      struct vmm_notifier_block *nb)
{
	if (!group)
		return VMM_EINVALID;

	return vmm_blocking_notifier_register(&group->notifier, nb);
}

int vmm_iommu_group_unregister_notifier(struct vmm_iommu_group *group,
					struct vmm_notifier_block *nb)
{
	if (!group)
		return VMM_EINVALID;

	return vmm_blocking_notifier_unregister(&group->notifier, nb);
}

const char *vmm_iommu_group_name(struct vmm_iommu_group *group)
{
	return (group) ? group->name : NULL;
}

struct vmm_iommu_controller *vmm_iommu_group_controller(
					struct vmm_iommu_group *group)
{
	return (group) ? group->ctrl : NULL;
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

	if (unlikely(domain->ops->attach_dev == NULL))
		return VMM_ENODEV;

	return domain->ops->attach_dev(domain, dev);
}

static int iommu_group_do_detach_device(struct vmm_device *dev, void *data)
{
	struct vmm_iommu_domain *domain = data;

	if (unlikely(domain->ops->detach_dev == NULL))
		return VMM_ENODEV;

	domain->ops->detach_dev(domain, dev);

	return VMM_OK;
}

int vmm_iommu_group_attach_domain(struct vmm_iommu_group *group,
				  struct vmm_iommu_domain *domain)
{
	int ret = VMM_OK;

	if (!group || !domain)
		return VMM_EINVALID;

	vmm_mutex_lock(&group->mutex);

	if (group->domain == domain) {
		ret = VMM_OK;
		goto out_unlock;
	} else if (group->domain != NULL) {
		ret = VMM_EEXIST;
		goto out_unlock;
	}

	ret = vmm_iommu_group_for_each_dev(group, domain,
					 iommu_group_do_attach_device);
	if (ret)
		goto out_unlock;

	vmm_iommu_domain_ref(domain);
	group->domain = domain;

out_unlock:
	vmm_mutex_unlock(&group->mutex);

	return ret;
}

int vmm_iommu_group_detach_domain(struct vmm_iommu_group *group)
{
	int ret = VMM_OK;
	struct vmm_iommu_domain *domain;

	if (!group)
		return VMM_EINVALID;

	vmm_mutex_lock(&group->mutex);

	domain = group->domain;
	group->domain = NULL;
	if (!domain)
		goto out_unlock;

	ret = vmm_iommu_group_for_each_dev(group, domain,
				     iommu_group_do_detach_device);

out_unlock:
	vmm_mutex_unlock(&group->mutex);

	vmm_iommu_domain_dref(domain);

	return ret;
}

struct vmm_iommu_domain *vmm_iommu_group_get_domain(
					struct vmm_iommu_group *group)
{
	struct vmm_iommu_domain *domain = NULL;

	if (!group)
		return NULL;

	vmm_mutex_lock(&group->mutex);
	domain = group->domain;
	vmm_iommu_domain_ref(domain);
	vmm_mutex_unlock(&group->mutex);

	return domain;
}

/* =============== IOMMU Domain APIs =============== */

struct vmm_iommu_domain *vmm_iommu_domain_alloc(const char *name,
					struct vmm_bus *bus,
					struct vmm_iommu_controller *ctrl,
					unsigned int type)
{
	struct vmm_iommu_domain *domain;

	if (bus == NULL || bus->iommu_ops == NULL || ctrl == NULL)
		return NULL;

	if ((type != VMM_IOMMU_DOMAIN_BLOCKED) &&
	    (type != VMM_IOMMU_DOMAIN_IDENTITY) &&
	    (type != VMM_IOMMU_DOMAIN_UNMANAGED) &&
	    (type != VMM_IOMMU_DOMAIN_DMA))
		return NULL;

	domain = bus->iommu_ops->domain_alloc(type, ctrl);
	if (!domain)
		return NULL;

	if (strlcpy(domain->name, name, sizeof(domain->name)) >=
	    sizeof(domain->name)) {
		vmm_free(domain);
		return NULL;
	}

	INIT_LIST_HEAD(&domain->head);
	domain->type = type;
	domain->ctrl = ctrl;
	xref_init(&domain->ref_count);
	domain->bus = bus;
	domain->ops = bus->iommu_ops;

	vmm_mutex_lock(&ctrl->domains_lock);
	list_add_tail(&domain->head, &ctrl->domains);
	vmm_mutex_unlock(&ctrl->domains_lock);

	return domain;
}

void vmm_iommu_domain_ref(struct vmm_iommu_domain *domain)
{
	if (domain == NULL)
		return;

	xref_get(&domain->ref_count);
}

static void __iommu_domain_free(struct xref *ref)
{
	struct vmm_iommu_domain *domain =
			container_of(ref, struct vmm_iommu_domain, ref_count);

	vmm_mutex_lock(&domain->ctrl->domains_lock);
	list_del(&domain->head);
	vmm_mutex_unlock(&domain->ctrl->domains_lock);

	if (likely(domain->ops->domain_free != NULL))
		domain->ops->domain_free(domain);
}

void vmm_iommu_domain_free(struct vmm_iommu_domain *domain)
{
	if (domain) {
		xref_put(&domain->ref_count, __iommu_domain_free);
	}
}

void vmm_iommu_set_fault_handler(struct vmm_iommu_domain *domain,
				 vmm_iommu_fault_handler_t handler,
				 void *token)
{
	BUG_ON(!domain);

	domain->handler = handler;
	domain->handler_token = token;
}

physical_addr_t vmm_iommu_iova_to_phys(struct vmm_iommu_domain *domain,
				       physical_addr_t iova)
{
	if (unlikely(domain->ops->iova_to_phys == NULL))
		return 0;

	return domain->ops->iova_to_phys(domain, iova);
}

static size_t iommu_pgsize(struct vmm_iommu_domain *domain,
			   physical_addr_t addr_merge, size_t size)
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
	physical_addr_t orig_iova = iova;
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

/* =============== IOMMU Misc APIs =============== */

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

bool vmm_iommu_capable(struct vmm_bus *bus, enum vmm_iommu_cap cap)
{
	if (!bus->iommu_ops || !bus->iommu_ops->capable)
		return FALSE;

	return bus->iommu_ops->capable(cap);
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
	int ret;
	const struct vmm_devtree_nodeid *iommu_matches;

	ret = vmm_devdrv_register_class(&iommuctrl_class);
	if (ret)
		return ret;

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
