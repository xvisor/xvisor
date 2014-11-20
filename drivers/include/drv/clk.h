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
 *
 * Adapted from linux/include/linux/clk.h
 *
 *  Copyright (C) 2004 ARM Limited.
 *  Written by Deep Blue Solutions Limited.
 *  Copyright (C) 2011-2012 Linaro Ltd <mturquette@linaro.org>
 *
 * The original source is licensed under GPL.
 */
#ifndef __CLK_H__
#define __CLK_H__

#include <vmm_types.h>
#include <vmm_notifier.h>
#include <libs/list.h>

struct vmm_device;
struct vmm_devtree_node;
struct vmm_devtree_phandle_args;

struct clk;

#ifdef CONFIG_COMMON_CLK

/**
 * DOC: clk notifier callback types
 *
 * PRE_RATE_CHANGE - called immediately before the clk rate is changed,
 *     to indicate that the rate change will proceed.  Drivers must
 *     immediately terminate any operations that will be affected by the
 *     rate change.  Callbacks may either return NOTIFY_DONE, NOTIFY_OK,
 *     NOTIFY_STOP or NOTIFY_BAD.
 *
 * ABORT_RATE_CHANGE: called if the rate change failed for some reason
 *     after PRE_RATE_CHANGE.  In this case, all registered notifiers on
 *     the clk will be called with ABORT_RATE_CHANGE. Callbacks must
 *     always return NOTIFY_DONE or NOTIFY_OK.
 *
 * POST_RATE_CHANGE - called after the clk rate change has successfully
 *     completed.  Callbacks must always return NOTIFY_DONE or NOTIFY_OK.
 *
 */
#define PRE_RATE_CHANGE			BIT(0)
#define POST_RATE_CHANGE		BIT(1)
#define ABORT_RATE_CHANGE		BIT(2)

/**
 * struct clk_notifier - associate a clk with a notifier
 * @clk: struct clk * to associate the notifier with
 * @notifier_head: a blocking_notifier_head for this clk
 * @node: linked list pointers
 *
 * A list of struct clk_notifier is maintained by the notifier code.
 * An entry is created whenever code registers the first notifier on a
 * particular @clk.  Future notifiers on that @clk are added to the
 * @notifier_head.
 */
struct clk_notifier {
	struct clk				*clk;
	struct vmm_atomic_notifier_chain	notifier_head;
	struct list_head			node;
};

/**
 * struct clk_notifier_data - rate data to pass to the notifier callback
 * @clk: struct clk * being changed
 * @old_rate: previous rate of this clk
 * @new_rate: new rate of this clk
 *
 * For a pre-notifier, old_rate is the clk's rate before this rate
 * change, and new_rate is what the rate will be in the future.  For a
 * post-notifier, old_rate and new_rate are both set to the clk's
 * current rate (this was done to optimize the implementation).
 */
struct clk_notifier_data {
	struct clk		*clk;
	unsigned long		old_rate;
	unsigned long		new_rate;
};

int clk_notifier_register(struct clk *clk, struct vmm_notifier_block *nb);

int clk_notifier_unregister(struct clk *clk, struct vmm_notifier_block *nb);

#endif

int clk_prepare(struct clk *clk);

void clk_unprepare(struct clk *clk);

struct clk *clk_get(struct vmm_device *dev, const char *id);

struct clk *devm_clk_get(struct vmm_device *dev, const char *id);

int clk_enable(struct clk *clk);

void clk_disable(struct clk *clk);

unsigned long clk_get_rate(struct clk *clk);

void clk_put(struct clk *clk);

void devm_clk_put(struct vmm_device *dev, struct clk *clk);

long clk_round_rate(struct clk *clk, unsigned long rate);

int clk_set_rate(struct clk *clk, unsigned long rate);

int clk_set_parent(struct clk *clk, struct clk *parent);

struct clk *clk_get_parent(struct clk *clk);

struct clk *clk_get_sys(const char *dev_id, const char *con_id);

/* clk_prepare_enable helps cases using clk_enable in non-atomic context. */
static inline int clk_prepare_enable(struct clk *clk)
{
	int ret;

	ret = clk_prepare(clk);
	if (ret)
		return ret;
	ret = clk_enable(clk);
	if (ret)
		clk_unprepare(clk);

	return ret;
}

/* clk_disable_unprepare helps cases using clk_disable in non-atomic context. */
static inline void clk_disable_unprepare(struct clk *clk)
{
	clk_disable(clk);
	clk_unprepare(clk);
}

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

const char *__clk_get_name(struct clk *clk);
#endif
