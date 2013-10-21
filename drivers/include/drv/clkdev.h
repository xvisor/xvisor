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
 * @file clkdev.h
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
#ifndef __CLKDEV_H_
#define __CLKDEV_H_

struct clk;
struct vmm_device;

#include <vmm_types.h>
#include <vmm_devtree.h>

#include <libs/list.h>

struct clk_lookup {
	struct list_head node;
	const char *dev_id;
	const char *con_id;
	struct clk *clk;
};

#define CLKDEV_INIT(d, n, c)	\
	{			\
		.dev_id = d,	\
		.con_id = n,	\
		.clk = c,	\
	}

struct clk_lookup *clkdev_alloc(struct clk *clk,
				const char *con_id, const char *dev_fmt, ...);

/**
 * clkdev_add - add a clock dynamically allocated
 */
void clkdev_add(struct clk_lookup *cl);

/**
 * clkdev_drop - remove a clock dynamically allocated
 */
void clkdev_drop(struct clk_lookup *cl);

/**
 * clkdev_add - add multiple clocks dynamically allocated
 */
void clkdev_add_table(struct clk_lookup *, size_t);

/**
 * clk_register_clkdev - register one clock lookup for a struct clk
 * @clk: struct clk to associate with all clk_lookups
 * @con_id: connection ID string on device
 * @dev_id: format string describing device name
 *
 * con_id or dev_id may be NULL as a wildcard, just as in the rest of
 * clkdev.
 *
 * To make things easier for mass registration, we detect error clks
 * from a previous clk_register() call, and return the error code for
 * those.  This is to permit this function to be called immediately
 * after clk_register().
 */
int clk_register_clkdev(struct clk *, const char *, const char *, ...);

/**
 * clk_register_clkdevs - register a set of clk_lookup for a struct clk
 * @clk: struct clk to associate with all clk_lookups
 * @cl: array of clk_lookup structures with con_id and dev_id pre-initialized
 * @num: number of clk_lookup structures to register
 *
 * To make things easier for mass registration, we detect error clks
 * from a previous clk_register() call, and return the error code for
 * those.  This is to permit this function to be called immediately
 * after clk_register().
 */
int clk_register_clkdevs(struct clk *, struct clk_lookup *, size_t);

#endif
