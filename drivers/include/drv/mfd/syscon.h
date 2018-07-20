/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file syscon.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief System Control Driver APIs
 *
 * The source has been largely adapted from Linux
 * include/linux/mfd/syscon.h
 *
 * System Control Driver
 *
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
 * Copyright (C) 2012 Linaro Ltd.
 *
 * Author: Dong Aisheng <dong.aisheng@linaro.org>
 *
 * The original code is licensed under the GPL.
 */

#ifndef __MFD_SYSCON_H__
#define __MFD_SYSCON_H__

#include <vmm_error.h>

struct vmm_devtree_node;

#ifdef CONFIG_MFD_SYSCON
extern struct regmap *syscon_node_to_regmap(struct vmm_devtree_node *np);
extern struct regmap *syscon_regmap_lookup_by_compatible(const char *s);
extern struct regmap *syscon_regmap_lookup_by_pdevname(const char *s);
extern struct regmap *syscon_regmap_lookup_by_phandle(
					struct vmm_devtree_node *np,
					const char *property);
#else
static inline struct regmap *syscon_node_to_regmap(struct vmm_devtree_node *np)
{
	return VMM_ERR_PTR(VMM_ENOTSUPP);
}

static inline struct regmap *syscon_regmap_lookup_by_compatible(const char *s)
{
	return VMM_ERR_PTR(VMM_ENOTSUPP);
}

static inline struct regmap *syscon_regmap_lookup_by_pdevname(const char *s)
{
	return VMM_ERR_PTR(VMM_ENOTSUPP);
}

static inline struct regmap *syscon_regmap_lookup_by_phandle(
					struct vmm_devtree_node *np,
					const char *property)
{
	return VMM_ERR_PTR(VMM_ENOTSUPP);
}
#endif

#endif /* __MFD_SYSCON_H__ */
