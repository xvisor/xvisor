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
 * @file clk-composite.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Composite clock implementation
 *
 * Adapted from linux/drivers/clk/clk-composite.c
 *
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <drv/clk.h>
#include <drv/clk-provider.h>

#define to_clk_composite(_hw) container_of(_hw, struct clk_composite, hw)

static u8 clk_composite_get_parent(struct clk_hw *hw)
{
	struct clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *mux_ops = composite->mux_ops;
	struct clk_hw *mux_hw = composite->mux_hw;

	mux_hw->clk = hw->clk;

	return mux_ops->get_parent(mux_hw);
}

static int clk_composite_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *mux_ops = composite->mux_ops;
	struct clk_hw *mux_hw = composite->mux_hw;

	mux_hw->clk = hw->clk;

	return mux_ops->set_parent(mux_hw, index);
}

static unsigned long clk_composite_recalc_rate(struct clk_hw *hw,
					    unsigned long parent_rate)
{
	struct clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *rate_ops = composite->rate_ops;
	struct clk_hw *rate_hw = composite->rate_hw;

	rate_hw->clk = hw->clk;

	return rate_ops->recalc_rate(rate_hw, parent_rate);
}

static long clk_composite_round_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long *prate)
{
	struct clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *rate_ops = composite->rate_ops;
	struct clk_hw *rate_hw = composite->rate_hw;

	rate_hw->clk = hw->clk;

	return rate_ops->round_rate(rate_hw, rate, prate);
}

static int clk_composite_set_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long parent_rate)
{
	struct clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *rate_ops = composite->rate_ops;
	struct clk_hw *rate_hw = composite->rate_hw;

	rate_hw->clk = hw->clk;

	return rate_ops->set_rate(rate_hw, rate, parent_rate);
}

static int clk_composite_is_enabled(struct clk_hw *hw)
{
	struct clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *gate_ops = composite->gate_ops;
	struct clk_hw *gate_hw = composite->gate_hw;

	gate_hw->clk = hw->clk;

	return gate_ops->is_enabled(gate_hw);
}

static int clk_composite_enable(struct clk_hw *hw)
{
	struct clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *gate_ops = composite->gate_ops;
	struct clk_hw *gate_hw = composite->gate_hw;

	gate_hw->clk = hw->clk;

	return gate_ops->enable(gate_hw);
}

static void clk_composite_disable(struct clk_hw *hw)
{
	struct clk_composite *composite = to_clk_composite(hw);
	const struct clk_ops *gate_ops = composite->gate_ops;
	struct clk_hw *gate_hw = composite->gate_hw;

	gate_hw->clk = hw->clk;

	gate_ops->disable(gate_hw);
}

struct clk *clk_register_composite(struct vmm_device *dev, const char *name,
			const char **parent_names, int num_parents,
			struct clk_hw *mux_hw, const struct clk_ops *mux_ops,
			struct clk_hw *rate_hw, const struct clk_ops *rate_ops,
			struct clk_hw *gate_hw, const struct clk_ops *gate_ops,
			unsigned long flags)
{
	struct clk *clk;
	struct clk_init_data init;
	struct clk_composite *composite;
	struct clk_ops *clk_composite_ops;

	composite = vmm_zalloc(sizeof(*composite));
	if (!composite) {
		vmm_printf("%s: could not allocate composite clk\n", __func__);
		return NULL;
	}

	init.name = name;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = parent_names;
	init.num_parents = num_parents;

	clk_composite_ops = &composite->ops;

	if (mux_hw && mux_ops) {
		if (!mux_ops->get_parent || !mux_ops->set_parent) {
			goto err;
		}

		composite->mux_hw = mux_hw;
		composite->mux_ops = mux_ops;
		clk_composite_ops->get_parent = clk_composite_get_parent;
		clk_composite_ops->set_parent = clk_composite_set_parent;
	}

	if (rate_hw && rate_ops) {
		if (!rate_ops->recalc_rate) {
			goto err;
		}

		/* .round_rate is a prerequisite for .set_rate */
		if (rate_ops->round_rate) {
			clk_composite_ops->round_rate = clk_composite_round_rate;
			if (rate_ops->set_rate) {
				clk_composite_ops->set_rate = clk_composite_set_rate;
			}
		} else {
			WARN(rate_ops->set_rate,
				"%s: missing round_rate op is required\n",
				__func__);
		}

		composite->rate_hw = rate_hw;
		composite->rate_ops = rate_ops;
		clk_composite_ops->recalc_rate = clk_composite_recalc_rate;
	}

	if (gate_hw && gate_ops) {
		if (!gate_ops->is_enabled || !gate_ops->enable ||
		    !gate_ops->disable) {
			goto err;
		}

		composite->gate_hw = gate_hw;
		composite->gate_ops = gate_ops;
		clk_composite_ops->is_enabled = clk_composite_is_enabled;
		clk_composite_ops->enable = clk_composite_enable;
		clk_composite_ops->disable = clk_composite_disable;
	}

	init.ops = clk_composite_ops;
	composite->hw.init = &init;

	clk = clk_register(dev, &composite->hw);
	if (!clk)
		goto err;

	if (composite->mux_hw)
		composite->mux_hw->clk = clk;

	if (composite->rate_hw)
		composite->rate_hw->clk = clk;

	if (composite->gate_hw)
		composite->gate_hw->clk = clk;

	return clk;

err:
	vmm_free(composite);
	return NULL;
}
VMM_EXPORT_SYMBOL_GPL(clk_register_composite);
