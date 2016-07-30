/**
 * Copyright (C) 2016 Anup Patel.
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
 * @file vmm_platform_msi.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Platform MSI implementation.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_msi.h>
#include <libs/stringlib.h>

#define DEV_ID_SHIFT	21
#define MAX_DEV_MSIS	(1 << (32 - DEV_ID_SHIFT))

/*
 * Internal data structure containing a (made up, but unique) devid
 * and the callback to write the MSI message.
 */
struct vmm_platform_msi_priv_data {
	struct vmm_device	*dev;
	void 			*host_data;
	vmm_msi_alloc_info_t	arg;
	vmm_irq_write_msi_msg_t	write_msg;
	int			devid;
};

/*
 * Convert an msi_desc to a globaly unique identifier (per-device
 * devid + msi_desc position in the msi_list).
 */
static unsigned int platform_msi_calc_hwirq(struct vmm_msi_desc *desc)
{
	u32 devid = desc->platform.msi_priv_data->devid;

	return (devid << (32 - DEV_ID_SHIFT)) | desc->platform.msi_index;
}

static void platform_msi_set_desc(vmm_msi_alloc_info_t *arg,
				  struct vmm_msi_desc *desc)
{
	arg->desc = desc;
	arg->hwirq = platform_msi_calc_hwirq(desc);
}

static void platform_msi_write_msg(struct vmm_msi_domain *domain,
				   struct vmm_msi_desc *desc,
				   unsigned int hirq, unsigned int hwirq,
				   struct vmm_msi_msg *msg)
{
	desc->platform.msi_priv_data->write_msg(desc, msg);
}

static void platform_msi_update_dom_ops(struct vmm_msi_domain_ops *ops)
{
	if (ops->set_desc == NULL)
		ops->set_desc = platform_msi_set_desc;
	if (ops->msi_write_msg == NULL)
		ops->msi_write_msg = platform_msi_write_msg;
}

struct vmm_msi_domain *vmm_platform_msi_create_domain(
					struct vmm_devtree_node *fwnode,
					struct vmm_msi_domain_ops *ops,
					struct vmm_host_irqdomain *parent,
					unsigned long flags,
					void *data)
{
	if (!fwnode || !ops || !parent)
		return NULL;

	if (flags & VMM_MSI_FLAG_USE_DEF_DOM_OPS)
		platform_msi_update_dom_ops(ops);

	return vmm_msi_create_domain(VMM_MSI_DOMAIN_PLATFORM,
				     fwnode, ops, parent, flags, data);
}

void vmm_platform_msi_destroy_domain(struct vmm_msi_domain *domain)
{
	vmm_msi_destroy_domain(domain);
}

int vmm_platform_msi_domain_alloc_irqs(struct vmm_device *dev,
				       unsigned int nvec,
				       vmm_irq_write_msi_msg_t write_msi_msg)
{
	/* TODO: */
	return VMM_ENOTAVAIL;
}

void vmm_platform_msi_domain_free_irqs(struct vmm_device *dev)
{
	/* TODO: */
}
