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
 * Helper for the clk API to assist looking up a struct clk.
 *
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <libs/list.h>
#include <libs/stringlib.h>
#include <drv/clk.h>
#include <drv/clkdev.h>

#include <stdarg.h>

static LIST_HEAD(clocks);
static DEFINE_SPINLOCK(clocks_lock);

#if 1 /* FIXME: */

static inline int __clk_get(struct clk *clk) { return 1; }
static inline void __clk_put(struct clk *clk) { }

static inline struct clk_lookup_alloc *__clkdev_alloc(size_t size)
{
	return vmm_zalloc(size);
}

#endif 

#if defined(CONFIG_COMMON_CLK)

struct clk *of_clk_get(struct vmm_devtree_node *node, int index)
{
	struct vmm_devtree_phandle_args clkspec;
	struct clk *clk;
	int rc;

	if (index < 0)
		return NULL;

	rc = vmm_devtree_parse_phandle_with_args(node, "clocks", 
				"#clock-cells", index, &clkspec);
	if (rc)
		return NULL;

	clk = of_clk_get_from_provider(&clkspec);

	return clk;
}
VMM_EXPORT_SYMBOL(of_clk_get);

struct clk *of_clk_get_by_name(struct vmm_devtree_node *node, 
				const char *name)
{
	struct clk *clk = NULL;

	/* Walk up the tree of devices looking for a clock that matches */
	while (node) {
		int index = 0;

		/*
		 * For named clocks, first look up the name in the
		 * "clock-names" property.  If it cannot be found, then
		 * index will be an error code, and of_clk_get() will fail.
		 */
		if (name)
			index = vmm_devtree_attrval_match_string(node, 
							"clock-names", name);
		clk = of_clk_get(node, index);
		if (clk)
			break;
		else if (name && index >= 0) {
			vmm_printf("ERROR: could not get clock %s:%s(%i)\n",
				node->name, name ? name : "", index);
			return clk;
		}

		/*
		 * No matching clock found on this node.  If the parent node
		 * has a "clock-ranges" property, then we can try one of its
		 * clocks.
		 */
		node = node->parent;
		if (node && !vmm_devtree_attrval(node, "clock-ranges"))
			break;
	}

	return clk;
}
VMM_EXPORT_SYMBOL(of_clk_get_by_name);

#endif

/*
 * Find the correct struct clk for the device and connection ID.
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

struct clk *clk_get_sys(const char *dev_id, const char *con_id)
{
	struct clk_lookup *cl;

	vmm_spin_lock(&clocks_lock);
	cl = clk_find(dev_id, con_id);
	if (cl && !__clk_get(cl->clk))
		cl = NULL;
	vmm_spin_unlock(&clocks_lock);

	return cl ? cl->clk : NULL;
}
VMM_EXPORT_SYMBOL(clk_get_sys);

struct clk *clk_get(struct vmm_device *dev, const char *con_id)
{
	const char *dev_id = dev ? dev->node->name : NULL;
	struct clk *clk;

	if (dev) {
		clk = of_clk_get_by_name(dev->node, con_id);
		if (clk && __clk_get(clk))
			return clk;
	}

	return clk_get_sys(dev_id, con_id);
}
VMM_EXPORT_SYMBOL(clk_get);

void clk_put(struct clk *clk)
{
	__clk_put(clk);
}
VMM_EXPORT_SYMBOL(clk_put);

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
vclkdev_alloc(struct clk *clk, const char *con_id, const char *dev_fmt,
		  va_list ap)
{
	struct clk_lookup_alloc *cla;

	cla = __clkdev_alloc(sizeof(*cla));
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
clkdev_alloc(struct clk *clk, const char *con_id, const char *dev_fmt, ...)
{
	struct clk_lookup *cl;
	va_list ap;

	va_start(ap, dev_fmt);
	cl = vclkdev_alloc(clk, con_id, dev_fmt, ap);
	va_end(ap);

	return cl;
}
VMM_EXPORT_SYMBOL(clkdev_alloc);

int clk_add_alias(const char *alias, const char *alias_dev_name,
		       char *id, struct vmm_device *dev)
{
	struct clk *clk = clk_get(dev, id);
	struct clk_lookup *l;

	if (clk == NULL)
		return VMM_EIO;

	l = clkdev_alloc(clk, alias, alias_dev_name);
	clk_put(clk);
	if (!l)
		return VMM_ENODEV;
	clkdev_add(l);
	return 0;
}
VMM_EXPORT_SYMBOL(clk_add_alias);

void clkdev_drop(struct clk_lookup *cl)
{
	vmm_spin_lock(&clocks_lock);
	list_del(&cl->node);
	vmm_spin_unlock(&clocks_lock);
	vmm_free(cl);
}
VMM_EXPORT_SYMBOL(clkdev_drop);

int clk_register_clkdev(struct clk *clk, const char *con_id,
			const char *dev_fmt, ...)
{
	struct clk_lookup *cl;
	va_list ap;

	if (clk == NULL)
		return VMM_EIO;

	va_start(ap, dev_fmt);
	cl = vclkdev_alloc(clk, con_id, dev_fmt, ap);
	va_end(ap);

	if (!cl)
		return VMM_ENOMEM;

	clkdev_add(cl);

	return 0;
}
VMM_EXPORT_SYMBOL(clk_register_clkdev);

int clk_register_clkdevs(struct clk *clk, struct clk_lookup *cl,
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
