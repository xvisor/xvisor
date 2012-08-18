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

static inline u32 amba_periphid(struct vmm_device *dev)
{
	return 0;
}

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
