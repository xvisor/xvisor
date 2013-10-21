/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file clk.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for clocking framework
 */
#ifndef __CLK_H__
#define __CLK_H__

#include <vmm_types.h>

struct vmm_device;
struct vmm_devtree_node;
struct vmm_devtree_phandle_args;

struct clk;

int clk_prepare(struct clk *clk);

void clk_unprepare(struct clk *clk);

struct clk *clk_get(struct vmm_device *dev, const char *id);

int clk_enable(struct clk *clk);

void clk_disable(struct clk *clk);

unsigned long clk_get_rate(struct clk *clk);

void clk_put(struct clk *clk);

long clk_round_rate(struct clk *clk, unsigned long rate);

int clk_set_rate(struct clk *clk, unsigned long rate);

int clk_set_parent(struct clk *clk, struct clk *parent);

struct clk *clk_get_parent(struct clk *clk);

struct clk *clk_get_sys(const char *dev_id, const char *con_id);

int clk_add_alias(const char *alias, const char *alias_dev_name,
		  char *id, struct vmm_device *dev);

#if defined(CONFIG_COMMON_CLK)

struct clk *of_clk_get(struct vmm_devtree_node *np, int index);

struct clk *of_clk_get_by_name(struct vmm_devtree_node *np, const char *name);

struct clk *of_clk_get_from_provider(
				struct vmm_devtree_phandle_args *clkspec);

#else
static inline
struct clk *of_clk_get(struct vmm_devtree_node *np, int index)
{
	/* Nothing to do here if common clock framework not available */
	return NULL;
}

static inline
struct clk *of_clk_get_by_name(struct vmm_devtree_node *np, const char *name)
{
	/* Nothing to do here if common clock framework not available */
	return NULL;
}
#endif

#endif
