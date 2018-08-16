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
 * @file clkdev.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief helper APIs for clk lookup
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
#include <vmm_spinlocks.h>
#include <vmm_heap.h>
#include <vmm_devdrv.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <libs/list.h>
#include <libs/stringlib.h>
#include <drv/clk.h>
#include <drv/clkdev.h>
#include <drv/clk-provider.h>

#include <stdarg.h>
#include "clk.h"

/* asm/clkdev.h --- start --- */
#ifndef CONFIG_COMMON_CLK
static inline int __clk_get(struct clk *clk) { return 1; }
static inline void __clk_put(struct clk *clk) { }
#endif

static inline struct clk_lookup_alloc *__clkdev_alloc(size_t size)
{
	return vmm_zalloc(size);
}
/* asm/clkdev.h --- finish --- */

static LIST_HEAD(clocks);
static DEFINE_SPINLOCK(clocks_slock);

#if defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK)
static struct clk *__of_clk_get(struct vmm_devtree_node *np, int index,
			       const char *dev_id, const char *con_id)
{
	struct vmm_devtree_phandle_args clkspec;
	struct clk *clk;
	int rc;

	if (index < 0)
		return VMM_ERR_PTR(VMM_EINVALID);

	rc = vmm_devtree_parse_phandle_with_args(np, "clocks", "#clock-cells",
						 index, &clkspec);
	if (rc)
		return VMM_ERR_PTR(rc);

	clk = __of_clk_get_from_provider(&clkspec, dev_id, con_id);
	vmm_devtree_dref_node(clkspec.np);

	return clk;
}

struct clk *of_clk_get(struct vmm_devtree_node *np, int index)
{
	return __of_clk_get(np, index, np->name, NULL);
}
VMM_EXPORT_SYMBOL(of_clk_get);

static struct clk *__of_clk_get_by_name(struct vmm_devtree_node *np,
					const char *dev_id,
					const char *name)
{
	struct clk *clk = VMM_ERR_PTR(VMM_ENOENT);

	/* Walk up the tree of devices looking for a clock that matches */
	while (np) {
		int index = 0;

		/*
		 * For named clocks, first look up the name in the
		 * "clock-names" property.  If it cannot be found, then
		 * index will be an error code, and of_clk_get() will fail.
		 */
		if (name)
			index = vmm_devtree_match_string(np, "clock-names", name);
		clk = __of_clk_get(np, index, dev_id, name);
		if (!VMM_IS_ERR(clk)) {
			break;
		} else if (name && index >= 0) {
			if (VMM_PTR_ERR(clk) != VMM_EPROBE_DEFER)
				vmm_lerror(__func__, "could not get clock %pOF:%s(%i)\n",
					np, name ? name : "", index);
			return clk;
		}

		/*
		 * No matching clock found on this node.  If the parent node
		 * has a "clock-ranges" property, then we can try one of its
		 * clocks.
		 */
		np = np->parent;
		if (np && !vmm_devtree_getattr(np, "clock-ranges"))
			break;
	}

	return clk;
}

/**
 * of_clk_get_by_name() - Parse and lookup a clock referenced by a device node
 * @np: pointer to clock consumer node
 * @name: name of consumer's clock input, or NULL for the first clock reference
 *
 * This function parses the clocks and clock-names properties,
 * and uses them to look up the struct clk from the registered list of clock
 * providers.
 */
struct clk *of_clk_get_by_name(struct vmm_devtree_node *np, const char *name)
{
	if (!np)
		return VMM_ERR_PTR(VMM_ENOENT);

	return __of_clk_get_by_name(np, np->name, name);
}
VMM_EXPORT_SYMBOL(of_clk_get_by_name);

#else /* defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK) */

static struct clk *__of_clk_get_by_name(struct vmm_devtree_node *np,
					const char *dev_id,
					const char *name)
{
	return VMM_ERR_PTR(VMM_ENOENT);
}
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
			if (!dev_id || strcmp(p->dev_id, dev_id))
				continue;
			match += 2;
		}
		if (p->con_id) {
			if (!con_id || strcmp(p->con_id, con_id))
				continue;
			match += 1;
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
	struct clk *clk = NULL;

	vmm_spin_lock(&clocks_slock);

	cl = clk_find(dev_id, con_id);
	if (!cl)
		goto out;

	clk = __clk_create_clk(cl->clk_hw, dev_id, con_id);
	if (VMM_IS_ERR(clk))
		goto out;

	if (!__clk_get(clk)) {
		__clk_free_clk(clk);
		cl = NULL;
		goto out;
	}

out:
	vmm_spin_unlock(&clocks_slock);

	return cl ? clk : VMM_ERR_PTR(VMM_ENOENT);
}
VMM_EXPORT_SYMBOL(clk_get_sys);

struct clk *clk_get(struct vmm_device *dev, const char *con_id)
{
	const char *dev_id = dev ? dev->name : NULL;
	struct clk *clk;

	if (dev) {
		clk = __of_clk_get_by_name(dev->of_node, dev_id, con_id);
		if (!VMM_IS_ERR(clk) || VMM_PTR_ERR(clk) == VMM_EPROBE_DEFER)
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

static void __clkdev_add(struct clk_lookup *cl)
{
	vmm_spin_lock(&clocks_slock);
	list_add_tail(&cl->node, &clocks);
	vmm_spin_unlock(&clocks_slock);
}

void clkdev_add(struct clk_lookup *cl)
{
	if (!cl->clk_hw)
		cl->clk_hw = __clk_get_hw(cl->clk);
	__clkdev_add(cl);
}
VMM_EXPORT_SYMBOL(clkdev_add);

void clkdev_add_table(struct clk_lookup *cl, size_t num)
{
	vmm_spin_lock(&clocks_slock);
	while (num--) {
		cl->clk_hw = __clk_get_hw(cl->clk);
		list_add_tail(&cl->node, &clocks);
		cl++;
	}
	vmm_spin_unlock(&clocks_slock);
}

#define MAX_DEV_ID	20
#define MAX_CON_ID	16

struct clk_lookup_alloc {
	struct clk_lookup cl;
	char	dev_id[MAX_DEV_ID];
	char	con_id[MAX_CON_ID];
};

static struct clk_lookup *
vclkdev_alloc(struct clk_hw *hw, const char *con_id, const char *dev_fmt,
	va_list ap)
{
	struct clk_lookup_alloc *cla;

	cla = __clkdev_alloc(sizeof(*cla));
	if (!cla)
		return NULL;

	cla->cl.clk_hw = hw;
	if (con_id) {
		strlcpy(cla->con_id, con_id, sizeof(cla->con_id));
		cla->cl.con_id = cla->con_id;
	}

	if (dev_fmt) {
		__vmm_snprintf(cla->dev_id, sizeof(cla->dev_id), dev_fmt, ap);
		cla->cl.dev_id = cla->dev_id;
	}

	return &cla->cl;
}

static struct clk_lookup *
vclkdev_create(struct clk_hw *hw, const char *con_id, const char *dev_fmt,
	va_list ap)
{
	struct clk_lookup *cl;

	cl = vclkdev_alloc(hw, con_id, dev_fmt, ap);
	if (cl)
		__clkdev_add(cl);

	return cl;
}

struct clk_lookup *
clkdev_alloc(struct clk *clk, const char *con_id, const char *dev_fmt, ...)
{
	struct clk_lookup *cl;
	va_list ap;

	va_start(ap, dev_fmt);
	cl = vclkdev_alloc(__clk_get_hw(clk), con_id, dev_fmt, ap);
	va_end(ap);

	return cl;
}
VMM_EXPORT_SYMBOL(clkdev_alloc);

struct clk_lookup *
clkdev_hw_alloc(struct clk_hw *hw, const char *con_id, const char *dev_fmt, ...)
{
	struct clk_lookup *cl;
	va_list ap;

	va_start(ap, dev_fmt);
	cl = vclkdev_alloc(hw, con_id, dev_fmt, ap);
	va_end(ap);

	return cl;
}
VMM_EXPORT_SYMBOL(clkdev_hw_alloc);

/**
 * clkdev_create - allocate and add a clkdev lookup structure
 * @clk: struct clk to associate with all clk_lookups
 * @con_id: connection ID string on device
 * @dev_fmt: format string describing device name
 *
 * Returns a clk_lookup structure, which can be later unregistered and
 * freed.
 */
struct clk_lookup *clkdev_create(struct clk *clk, const char *con_id,
	const char *dev_fmt, ...)
{
	struct clk_lookup *cl;
	va_list ap;

	va_start(ap, dev_fmt);
	cl = vclkdev_create(__clk_get_hw(clk), con_id, dev_fmt, ap);
	va_end(ap);

	return cl;
}
VMM_EXPORT_SYMBOL(clkdev_create);

/**
 * clkdev_hw_create - allocate and add a clkdev lookup structure
 * @hw: struct clk_hw to associate with all clk_lookups
 * @con_id: connection ID string on device
 * @dev_fmt: format string describing device name
 *
 * Returns a clk_lookup structure, which can be later unregistered and
 * freed.
 */
struct clk_lookup *clkdev_hw_create(struct clk_hw *hw, const char *con_id,
	const char *dev_fmt, ...)
{
	struct clk_lookup *cl;
	va_list ap;

	va_start(ap, dev_fmt);
	cl = vclkdev_create(hw, con_id, dev_fmt, ap);
	va_end(ap);

	return cl;
}
VMM_EXPORT_SYMBOL(clkdev_hw_create);

int clk_add_alias(const char *alias, const char *alias_dev_name,
	const char *con_id, struct vmm_device *dev)
{
	struct clk *r = clk_get(dev, con_id);
	struct clk_lookup *l;

	if (VMM_IS_ERR(r))
		return VMM_PTR_ERR(r);

	l = clkdev_create(r, alias, alias_dev_name ? "%s" : NULL,
			  alias_dev_name);
	clk_put(r);

	return l ? 0 : VMM_ENODEV;
}
VMM_EXPORT_SYMBOL(clk_add_alias);

/*
 * clkdev_drop - remove a clock dynamically allocated
 */
void clkdev_drop(struct clk_lookup *cl)
{
	vmm_spin_lock(&clocks_slock);
	list_del(&cl->node);
	vmm_spin_unlock(&clocks_slock);
	vmm_free(cl);
}
VMM_EXPORT_SYMBOL(clkdev_drop);

static struct clk_lookup *__clk_register_clkdev(struct clk_hw *hw,
						const char *con_id,
						const char *dev_id, ...)
{
	struct clk_lookup *cl;
	va_list ap;

	va_start(ap, dev_id);
	cl = vclkdev_create(hw, con_id, dev_id, ap);
	va_end(ap);

	return cl;
}

/**
 * clk_register_clkdev - register one clock lookup for a struct clk
 * @clk: struct clk to associate with all clk_lookups
 * @con_id: connection ID string on device
 * @dev_id: string describing device name
 *
 * con_id or dev_id may be NULL as a wildcard, just as in the rest of
 * clkdev.
 *
 * To make things easier for mass registration, we detect error clks
 * from a previous clk_register() call, and return the error code for
 * those.  This is to permit this function to be called immediately
 * after clk_register().
 */
int clk_register_clkdev(struct clk *clk, const char *con_id,
	const char *dev_id)
{
	struct clk_lookup *cl;

	if (VMM_IS_ERR(clk))
		return VMM_PTR_ERR(clk);

	/*
	 * Since dev_id can be NULL, and NULL is handled specially, we must
	 * pass it as either a NULL format string, or with "%s".
	 */
	if (dev_id)
		cl = __clk_register_clkdev(__clk_get_hw(clk), con_id, "%s",
					   dev_id);
	else
		cl = __clk_register_clkdev(__clk_get_hw(clk), con_id, NULL);

	return cl ? 0 : VMM_ENOMEM;
}
VMM_EXPORT_SYMBOL(clk_register_clkdev);

/**
 * clk_hw_register_clkdev - register one clock lookup for a struct clk_hw
 * @hw: struct clk_hw to associate with all clk_lookups
 * @con_id: connection ID string on device
 * @dev_id: format string describing device name
 *
 * con_id or dev_id may be NULL as a wildcard, just as in the rest of
 * clkdev.
 *
 * To make things easier for mass registration, we detect error clk_hws
 * from a previous clk_hw_register_*() call, and return the error code for
 * those.  This is to permit this function to be called immediately
 * after clk_hw_register_*().
 */
int clk_hw_register_clkdev(struct clk_hw *hw, const char *con_id,
	const char *dev_id)
{
	struct clk_lookup *cl;

	if (VMM_IS_ERR(hw))
		return VMM_PTR_ERR(hw);

	/*
	 * Since dev_id can be NULL, and NULL is handled specially, we must
	 * pass it as either a NULL format string, or with "%s".
	 */
	if (dev_id)
		cl = __clk_register_clkdev(hw, con_id, "%s", dev_id);
	else
		cl = __clk_register_clkdev(hw, con_id, NULL);

	return cl ? 0 : VMM_ENOMEM;
}
VMM_EXPORT_SYMBOL(clk_hw_register_clkdev);
