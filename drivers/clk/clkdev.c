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
 * @file clkdev.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Clock API helper functions
 *
 * Adapted from linux/drivers/clk/clkdev.c
 *
 *  Copyright (C) 2008 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Helper for the clk API to assist looking up a struct arch_clk.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <arch_clk.h>
#include <libs/list.h>
#include <libs/stringlib.h>
#include <drv/clkdev.h>

#include <stdarg.h>

static LIST_HEAD(clocks);
static DEFINE_SPINLOCK(clocks_lock);

/*
 * Find the correct struct arch_clk for the device and connection ID.
 * We do slightly fuzzy matching here:
 *  An entry with a NULL ID is assumed to be a wildcard.
 *  If an entry has a device ID, it must match
 *  If an entry has a connection ID, it must match
 * Then we take the most specific entry - with the following
 * order of precedence: dev+con > dev only > con only.
 */
static struct clk_lookup *clk_find(const char *dev_id, const char *con_id)
{
	struct clk_lookup *p, *cl = NULL;
	int match, best_found = 0, best_possible = 0;

	if (dev_id)
		best_possible += 2;
	if (con_id)
		best_possible += 1;

	list_for_each_entry(p, &clocks, node) {
		match = 0;
		if (p->dev_id) {
			if (!dev_id || strcmp(p->dev_id, dev_id)) {
				continue;
			}
			match += 2;
		}
		if (p->con_id) {
			if (!con_id || strcmp(p->con_id, con_id)) {
				if (!match)
					continue;
			} else {
				match += 1;
			}
		}

		if (match > best_found) {
			cl = p;
			if (match != best_possible)
				best_found = match;
			else
				break;
		}
	}

	return cl;
}

struct arch_clk *clkdev_get_sys(const char *dev_id, const char *con_id)
{
	struct clk_lookup *cl;

	vmm_spin_lock(&clocks_lock);
	cl = clk_find(dev_id, con_id);
	vmm_spin_unlock(&clocks_lock);

	return cl ? cl->clk : NULL;
}
VMM_EXPORT_SYMBOL(clkdev_get_sys);

struct arch_clk *clkdev_get(struct vmm_device *dev, const char *con_id)
{
	const char *dev_id = dev ? dev->node->name : NULL;

	return clkdev_get_sys(dev_id, con_id);
}
VMM_EXPORT_SYMBOL(clkdev_get);

void clkdev_put(struct arch_clk *clk)
{
}
VMM_EXPORT_SYMBOL(clkdev_put);

void clkdev_add(struct clk_lookup *cl)
{
	vmm_spin_lock(&clocks_lock);
	list_add_tail(&cl->node, &clocks);
	vmm_spin_unlock(&clocks_lock);
}
VMM_EXPORT_SYMBOL(clkdev_add);

void __init clkdev_add_table(struct clk_lookup *cl, size_t num)
{
	vmm_spin_lock(&clocks_lock);
	while (num--) {
		list_add_tail(&cl->node, &clocks);
		cl++;
	}
	vmm_spin_unlock(&clocks_lock);
}
VMM_EXPORT_SYMBOL(clkdev_add_table);

#define MAX_DEV_ID	20
#define MAX_CON_ID	16

struct clk_lookup_alloc {
	struct clk_lookup cl;
	char dev_id[MAX_DEV_ID];
	char con_id[MAX_CON_ID];
};

static struct clk_lookup *__init
vmm_vclkdev_alloc(struct arch_clk *clk, const char *con_id, const char *dev_fmt,
		  va_list ap)
{
	struct clk_lookup_alloc *cla;

	cla = vmm_malloc(sizeof(*cla));
	if (!cla)
		return NULL;

	cla->cl.clk = clk;
	if (con_id) {
		strncpy(cla->con_id, con_id, sizeof(cla->con_id));
		cla->cl.con_id = cla->con_id;
	}

	if (dev_fmt) {
		vmm_snprintf(cla->dev_id, sizeof(cla->dev_id), dev_fmt, ap);
		cla->cl.dev_id = cla->dev_id;
	}

	return &cla->cl;
}

struct clk_lookup *__init
clkdev_alloc(struct arch_clk *clk, const char *con_id, const char *dev_fmt, ...)
{
	struct clk_lookup *cl;
	va_list ap;

	va_start(ap, dev_fmt);
	cl = vmm_vclkdev_alloc(clk, con_id, dev_fmt, ap);
	va_end(ap);

	return cl;
}
VMM_EXPORT_SYMBOL(clkdev_alloc);

int clk_add_alias(const char *alias, const char *alias_dev_name, char *id,
		  struct vmm_device *dev)
{
	struct arch_clk *clk = clkdev_get(dev, id);
	struct clk_lookup *l;

	if (clk == NULL)
		return VMM_EIO;

	l = clkdev_alloc(clk, alias, alias_dev_name);
	clkdev_put(clk);
	if (!l)
		return VMM_ENODEV;
	clkdev_add(l);
	return 0;
}
VMM_EXPORT_SYMBOL(clk_add_alias);

/*
 * clkdev_drop - remove a clock dynamically allocated
 */
void clkdev_drop(struct clk_lookup *cl)
{
	vmm_spin_lock(&clocks_lock);
	list_del(&cl->node);
	vmm_spin_unlock(&clocks_lock);
	vmm_free(cl);
}
VMM_EXPORT_SYMBOL(clkdev_drop);

/**
 * clk_register_clkdev - register one clock lookup for a struct arch_clk
 * @clk: struct arch_clk to associate with all clk_lookups
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
int clk_register_clkdev(struct arch_clk *clk, const char *con_id,
			const char *dev_fmt, ...)
{
	struct clk_lookup *cl;
	va_list ap;

	if (clk == NULL)
		return VMM_EIO;

	va_start(ap, dev_fmt);
	cl = vmm_vclkdev_alloc(clk, con_id, dev_fmt, ap);
	va_end(ap);

	if (!cl)
		return VMM_ENOMEM;

	clkdev_add(cl);

	return 0;
}
VMM_EXPORT_SYMBOL(clk_register_clkdev);

/**
 * clk_register_clkdevs - register a set of clk_lookup for a struct arch_clk
 * @clk: struct arch_clk to associate with all clk_lookups
 * @cl: array of clk_lookup structures with con_id and dev_id pre-initialized
 * @num: number of clk_lookup structures to register
 *
 * To make things easier for mass registration, we detect error clks
 * from a previous clk_register() call, and return the error code for
 * those.  This is to permit this function to be called immediately
 * after clk_register().
 */
int clk_register_clkdevs(struct arch_clk *clk, struct clk_lookup *cl,
			 size_t num)
{
	unsigned i;

	if (clk == NULL)
		return VMM_EIO;

	for (i = 0; i < num; i++, cl++) {
		cl->clk = clk;
		clkdev_add(cl);
	}

	return 0;
}
VMM_EXPORT_SYMBOL(clk_register_clkdevs);

struct arch_clk *clkdev_get_by_node(struct vmm_devtree_node *node)
{
	struct arch_clk *clk = NULL;

	if (node) {
		clk = clkdev_get_sys(node->name, NULL);
	}

	return clk;
}
VMM_EXPORT_SYMBOL(clkdev_get_by_node);
