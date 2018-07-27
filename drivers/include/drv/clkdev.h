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
 * @file clkdev.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief helper APIs for clk lookup
 *
 * Adapted from linux/include/linux/clkdev.h
 *
 *  Copyright (C) 2008 Russell King.
 *
 * Helper for the clk API to assist looking up a struct clk.
 *
 * The original source is licensed under GPL.
 */
#ifndef __CLKDEV_H__
#define __CLKDEV_H__

#include <vmm_types.h>
#include <vmm_compiler.h>

struct clk;
struct clk_hw;
struct vmm_device;

struct clk_lookup {
	struct list_head	node;
	const char		*dev_id;
	const char		*con_id;
	struct clk		*clk;
	struct clk_hw		*clk_hw;
};

#define CLKDEV_INIT(d, n, c)	\
	{			\
		.dev_id = d,	\
		.con_id = n,	\
		.clk = c,	\
	}

struct clk_lookup *clkdev_alloc(struct clk *clk, const char *con_id,
	const char *dev_fmt, ...) __printf(3, 4);
struct clk_lookup *clkdev_hw_alloc(struct clk_hw *hw, const char *con_id,
	const char *dev_fmt, ...) __printf(3, 4);

void clkdev_add(struct clk_lookup *cl);
void clkdev_drop(struct clk_lookup *cl);

struct clk_lookup *clkdev_create(struct clk *clk, const char *con_id,
	const char *dev_fmt, ...) __printf(3, 4);
struct clk_lookup *clkdev_hw_create(struct clk_hw *hw, const char *con_id,
	const char *dev_fmt, ...) __printf(3, 4);

void clkdev_add_table(struct clk_lookup *, size_t);
int clk_add_alias(const char *, const char *, const char *, struct vmm_device *);

int clk_register_clkdev(struct clk *, const char *, const char *);
int clk_hw_register_clkdev(struct clk_hw *, const char *, const char *);

#ifdef CONFIG_COMMON_CLK
int __clk_get(struct clk *clk);
void __clk_put(struct clk *clk);
#endif

#endif
