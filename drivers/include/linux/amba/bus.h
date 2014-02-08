/*
 *  linux/include/amba/bus.h
 *
 *  This device type deals with ARM PrimeCells and anything else that
 *  presents a proper CID (0xB105F00D) at the end of the I/O register
 *  region or that is derived from a PrimeCell.
 *
 *  Copyright (C) 2003 Deep Blue Solutions Ltd, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef ASMARM_AMBA_H
#define ASMARM_AMBA_H

#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>

#define AMBA_PERIPHID_ATTR_NAME			"amba_periphid"

/* Retrive AMBA peripheral ID from device node */
static inline u32 amba_periphid(struct vmm_device *dev)
{
	u32 pid = 0x0, val;
	const u32 *periphid;
	virtual_addr_t dev_base;

	if (!dev || !dev->node) {
		return 0;
	}

	periphid = vmm_devtree_attrval(dev->node, AMBA_PERIPHID_ATTR_NAME);
	if (!periphid) {
		if (vmm_devtree_regmap(dev->node, &dev_base, 0)) {
			return 0;
		}
		val = vmm_readl((void *)(dev_base + 0xFE0));
		pid |= (val & 0xFF);
		val = vmm_readl((void *)(dev_base + 0xFE4));
		pid |= (val & 0xFF) << 8;
		val = vmm_readl((void *)(dev_base + 0xFE8));
		pid |= (val & 0xFF) << 16;
		val = vmm_readl((void *)(dev_base + 0xFEC));
		pid |= (val & 0xFF) << 24;
		vmm_devtree_regunmap(dev->node, dev_base, 0);
		vmm_devtree_setattr(dev->node, AMBA_PERIPHID_ATTR_NAME, 
			    &pid, VMM_DEVTREE_ATTRTYPE_UINT32, sizeof(pid));
	} else {
		pid = *periphid;
	}

	return pid;
}

enum amba_vendor {
	AMBA_VENDOR_ARM = 0x41,
	AMBA_VENDOR_ST = 0x80,
};

/* Some drivers don't use the struct amba_device */
#define AMBA_CONFIG_BITS(a) (((a) >> 24) & 0xff)
#define AMBA_REV_BITS(a) (((a) >> 20) & 0x0f)
#define AMBA_MANF_BITS(a) (((a) >> 12) & 0xff)
#define AMBA_PART_BITS(a) ((a) & 0xfff)

#define amba_config(d)	AMBA_CONFIG_BITS(amba_periphid(d))
#define amba_rev(d)	AMBA_REV_BITS(amba_periphid(d))
#define amba_manf(d)	AMBA_MANF_BITS(amba_periphid(d))
#define amba_part(d)	AMBA_PART_BITS(amba_periphid(d))

#endif
