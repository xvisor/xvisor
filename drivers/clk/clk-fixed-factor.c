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
 * @file clk-fixed-factor.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Fixed factor clock implementation
 *
 * Adapted from linux/drivers/clk/clk-fixed-factor.c
 *
 * Copyright (C) 2011 Sascha Hauer, Pengutronix <s.hauer@pengutronix.de>
 *
 * Standard functionality for the common clock API.
 *
 * The original source is licensed under GPL.
 */
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <libs/mathlib.h>
#include <drv/clk-provider.h>

/*
 * DOC: basic fixed multiplier and divider clock that cannot gate
 *
 * Traits of this clock:
 * prepare - clk_prepare only ensures that parents are prepared
 * enable - clk_enable only ensures that parents are enabled
 * rate - rate is fixed.  clk->rate = parent->rate / div * mult
 * parent - fixed parent.  No clk_set_parent support
 */

#define to_clk_fixed_factor(_hw) container_of(_hw, struct clk_fixed_factor, hw)

static unsigned long clk_factor_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clk_fixed_factor *fix = to_clk_fixed_factor(hw);
	unsigned long long int rate;

	rate = (unsigned long long int)parent_rate * fix->mult;
	rate = udiv64(rate, fix->div);
	return (unsigned long)rate;
}

static long clk_factor_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	struct clk_fixed_factor *fix = to_clk_fixed_factor(hw);

	if (__clk_get_flags(hw->clk) & CLK_SET_RATE_PARENT) {
		unsigned long best_parent;

		best_parent = udiv64(rate, fix->mult) * fix->div;
		*prate = __clk_round_rate(__clk_get_parent(hw->clk),
				best_parent);
	}

	return udiv64(*prate, fix->div) * fix->mult;
}

static int clk_factor_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	return 0;
}

struct clk_ops clk_fixed_factor_ops = {
	.round_rate = clk_factor_round_rate,
	.set_rate = clk_factor_set_rate,
	.recalc_rate = clk_factor_recalc_rate,
};
VMM_EXPORT_SYMBOL_GPL(clk_fixed_factor_ops);

struct clk *clk_register_fixed_factor(struct vmm_device *dev, const char *name,
		const char *parent_name, unsigned long flags,
		unsigned int mult, unsigned int div)
{
	struct clk_fixed_factor *fix;
	struct clk_init_data init;
	struct clk *clk;

	fix = vmm_zalloc(sizeof(*fix));
	if (!fix) {
		vmm_printf("%s: could not allocate fixed factor clk\n", __func__);
		return NULL;
	}

	/* struct clk_fixed_factor assignments */
	fix->mult = mult;
	fix->div = div;
	fix->hw.init = &init;

	init.name = name;
	init.ops = &clk_fixed_factor_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	clk = clk_register(dev, &fix->hw);

	if (!clk)
		vmm_free(fix);

	return clk;
}
VMM_EXPORT_SYMBOL_GPL(clk_register_fixed_factor);

/**
 * of_fixed_factor_clk_setup() - Setup function for simple fixed factor clock
 */
void __init of_fixed_factor_clk_setup(struct vmm_devtree_node *node)
{
	struct clk *clk;
	const void *attrval;
	const char *clk_name = node->name;
	const char *parent_name;
	u32 div, mult;

	attrval = vmm_devtree_attrval(node, "clock-div");
	if (!attrval) {
		vmm_printf("%s Fixed factor clock <%s> must have a clock-div property\n",
			__func__, node->name);
		return;
	}
	div = *((const u32 *)attrval);

	attrval = vmm_devtree_attrval(node, "clock-mult");
	if (!attrval) {
		vmm_printf("%s Fixed factor clock <%s> must have a clock-mult property\n",
			__func__, node->name);
		return;
	}
	mult = *((const u32 *)attrval);

	attrval = vmm_devtree_attrval(node, "clock-output-names");
	if (attrval)
		clk_name = attrval;

	parent_name = of_clk_get_parent_name(node, 0);

	clk = clk_register_fixed_factor(NULL, clk_name, parent_name, 0,
					mult, div);
	if (clk)
		of_clk_add_provider(node, of_clk_src_simple_get, clk);
}
VMM_EXPORT_SYMBOL_GPL(of_fixed_factor_clk_setup);

CLK_OF_DECLARE(fixed_factor_clk, "fixed-factor-clock",
		of_fixed_factor_clk_setup);

