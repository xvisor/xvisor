/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
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
 * @file vmm_clkdev.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Clock API helper headers
 *
 * Adapted from linux/include/linux/clkdev.h
 *
 *  Copyright (C) 2008 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Helper for the clk API to assist looking up a struct clk.
 */
#ifndef __CLKDEV_H
#define __CLKDEV_H

struct arch_clk;
struct vmm_device;

#include <vmm_types.h>
#include <vmm_devtree.h>

#include <libs/list.h>

struct clk_lookup {
	struct list_head node;
	const char *dev_id;
	const char *con_id;
	struct arch_clk *clk;
};

#define CLKDEV_INIT(d, n, c)	\
	{			\
		.dev_id = d,	\
		.con_id = n,	\
		.clk = c,	\
	}

struct clk_lookup *clkdev_alloc(struct arch_clk *clk,
				const char *con_id, const char *dev_fmt, ...);

void clkdev_add(struct clk_lookup *cl);
void clkdev_drop(struct clk_lookup *cl);

void clkdev_add_table(struct clk_lookup *, size_t);
int clk_add_alias(const char *, const char *, char *, struct vmm_device *);

int clk_register_clkdev(struct arch_clk *, const char *, const char *, ...);
int clk_register_clkdevs(struct arch_clk *, struct clk_lookup *, size_t);

struct arch_clk *clkdev_get(struct vmm_device *dev, const char *id);
void clkdev_put(struct arch_clk *clk);

struct arch_clk *clkdev_get_by_node(struct vmm_devtree_node *node);

#endif
