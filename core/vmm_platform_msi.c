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
#include <libs/idr.h>
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

/* The devid allocator */
static DEFINE_IDA(platform_msi_devid_ida);

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

static struct vmm_platform_msi_priv_data *
platform_msi_alloc_priv_data(struct vmm_device *dev, unsigned int nvec,
			     vmm_irq_write_msi_msg_t write_msi_msg)
{
	struct vmm_platform_msi_priv_data *datap;

	/*
	 * Limit the number of interrupts to 256 per device. Should we
	 * need to bump this up, DEV_ID_SHIFT should be adjusted
	 * accordingly (which would impact the max number of MSI
	 * capable devices).
	 */
	if (!dev->msi_domain || !write_msi_msg || !nvec || nvec > MAX_DEV_MSIS)
		return VMM_ERR_PTR(VMM_EINVALID);

	if (dev->msi_domain->type != VMM_MSI_DOMAIN_PLATFORM) {
		vmm_printf("%s: Incompatible msi_domain, giving up\n",
			   dev->name);
		return VMM_ERR_PTR(VMM_EINVALID);
	}

	/* Already had a helping of MSI? Greed... */
	if (!list_empty(dev_to_msi_list(dev)))
		return VMM_ERR_PTR(VMM_EBUSY);

	datap = vmm_zalloc(sizeof(*datap));
	if (!datap)
		return VMM_ERR_PTR(VMM_ENOMEM);

	datap->devid = ida_simple_get(&platform_msi_devid_ida,
				      0, 1 << DEV_ID_SHIFT, 0x0);
	if (datap->devid < 0) {
		int err = datap->devid;
		vmm_free(datap);
		return VMM_ERR_PTR(err);
	}

	datap->write_msg = write_msi_msg;
	datap->dev = dev;

	return datap;
}

static void platform_msi_free_priv_data(struct vmm_platform_msi_priv_data *d)
{
	ida_simple_remove(&platform_msi_devid_ida, d->devid);
	vmm_free(d);
}

static void platform_msi_free_descs(struct vmm_device *dev, int base, int nvec)
{
	struct vmm_msi_desc *desc, *tmp;

	list_for_each_entry_safe(desc, tmp, dev_to_msi_list(dev), list) {
		if (desc->platform.msi_index >= base &&
		    desc->platform.msi_index < (base + nvec)) {
			list_del(&desc->list);
			vmm_free_msi_entry(desc);
		}
	}
}

static int platform_msi_alloc_descs_with_irq(struct vmm_device *dev,
					     int hirq, int nvec,
					     struct vmm_platform_msi_priv_data *data)

{
	struct vmm_msi_desc *desc;
	int i, base = 0;

	if (!list_empty(dev_to_msi_list(dev))) {
		desc = list_last_entry(dev_to_msi_list(dev),
				       struct vmm_msi_desc, list);
		base = desc->platform.msi_index + 1;
	}

	for (i = 0; i < nvec; i++) {
		desc = vmm_alloc_msi_entry(dev);
		if (!desc)
			break;

		desc->platform.msi_priv_data = data;
		desc->platform.msi_index = base + i;
		desc->nvec_used = 1;
		desc->hirq = hirq ? hirq + i : 0;

		list_add_tail(&desc->list, dev_to_msi_list(dev));
	}

	if (i != nvec) {
		/* Clean up the mess */
		platform_msi_free_descs(dev, base, nvec);

		return VMM_ENOMEM;
	}

	return 0;
}

static int platform_msi_alloc_descs(struct vmm_device *dev, int nvec,
				    struct vmm_platform_msi_priv_data *data)

{
	return platform_msi_alloc_descs_with_irq(dev, 0, nvec, data);
}

int vmm_platform_msi_domain_alloc_irqs(struct vmm_device *dev,
				       unsigned int nvec,
				       vmm_irq_write_msi_msg_t write_msi_msg)
{
	struct vmm_platform_msi_priv_data *priv_data;
	int err;

	priv_data = platform_msi_alloc_priv_data(dev, nvec, write_msi_msg);
	if (VMM_IS_ERR(priv_data))
		return VMM_PTR_ERR(priv_data);

	err = platform_msi_alloc_descs(dev, nvec, priv_data);
	if (err)
		goto out_free_priv_data;

	err = vmm_msi_domain_alloc_irqs(dev->msi_domain, dev, nvec);
	if (err)
		goto out_free_desc;

	return 0;

out_free_desc:
	platform_msi_free_descs(dev, 0, nvec);
out_free_priv_data:
	platform_msi_free_priv_data(priv_data);

	return err;
}

void vmm_platform_msi_domain_free_irqs(struct vmm_device *dev)
{
	if (!list_empty(dev_to_msi_list(dev))) {
		struct vmm_msi_desc *desc;

		desc = first_msi_entry(dev);
		platform_msi_free_priv_data(desc->platform.msi_priv_data);
	}

	vmm_msi_domain_free_irqs(dev->msi_domain, dev);
	platform_msi_free_descs(dev, 0, MAX_DEV_MSIS);
}
