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
 * @file clk-conf.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief device tree config helper APIs for clocking framework
 *
 * Adapted from linux/include/linux/clk/clk-conf.h
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 * Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * The original source is licensed under GPL.
 */

#ifndef __CLK_CONF_H__
#define __CLK_CONF_H__

#include <vmm_types.h>

struct vmm_devtree_node;

#if defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK)
int of_clk_set_defaults(struct vmm_devtree_node *node, bool clk_supplier);
#else
static inline int of_clk_set_defaults(struct vmm_devtree_node *node,
				      bool clk_supplier)
{
	return 0;
}
#endif

#endif
