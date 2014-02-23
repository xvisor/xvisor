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
 * @file clk-vexpress-osc.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM VExpress board OSC clocks implementation
 *
 * Adapted from linux/drivers/clk/versatile/clk-vexpress-osc.c
 *
 * Copyright (C) 2012 ARM Limited
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_compiler.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <drv/clkdev.h>
#include <drv/clk-provider.h>
#include <drv/vexpress.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)	vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

struct vexpress_osc {
	struct vexpress_config_func *func;
	struct clk_hw hw;
	unsigned long rate_min;
	unsigned long rate_max;
};

#define to_vexpress_osc(osc) container_of(osc, struct vexpress_osc, hw)

static unsigned long vexpress_osc_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct vexpress_osc *osc = to_vexpress_osc(hw);
	u32 rate;

	vexpress_config_read(osc->func, 0, &rate);

	return rate;
}

static long vexpress_osc_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct vexpress_osc *osc = to_vexpress_osc(hw);

	if (WARN_ON(osc->rate_min && rate < osc->rate_min))
		rate = osc->rate_min;

	if (WARN_ON(osc->rate_max && rate > osc->rate_max))
		rate = osc->rate_max;

	return rate;
}

static int vexpress_osc_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct vexpress_osc *osc = to_vexpress_osc(hw);

	return vexpress_config_write(osc->func, 0, rate);
}

static struct clk_ops vexpress_osc_ops = {
	.recalc_rate = vexpress_osc_recalc_rate,
	.round_rate = vexpress_osc_round_rate,
	.set_rate = vexpress_osc_set_rate,
};

struct clk * __init vexpress_osc_setup(struct vmm_device *dev)
{
	struct clk_init_data init;
	struct vexpress_osc *osc = vmm_zalloc(sizeof(*osc));

	if (!osc)
		return NULL;

	osc->func = vexpress_config_func_get_by_dev(dev);
	if (!osc->func) {
		vmm_free(osc);
		return NULL;
	}

	init.name = dev->name;
	init.ops = &vexpress_osc_ops;
	init.flags = CLK_IS_ROOT;
	init.num_parents = 0;
	osc->hw.init = &init;

	return clk_register(NULL, &osc->hw);
}

void __init vexpress_osc_of_setup(struct vmm_devtree_node *node)
{
	struct clk_init_data init;
	struct vexpress_osc *osc;
	struct clk *clk;
	u32 range[2];

	osc = vmm_zalloc(sizeof(*osc));
	if (!osc)
		goto error;

	osc->func = vexpress_config_func_get_by_node(node);
	if (!osc->func) {
		vmm_printf("%s: Failed to obtain config func for node '%s'!\n",
			   __func__, node->name);
		goto error;
	}

	if (vmm_devtree_read_u32_array(node, "freq-range", range,
				array_size(range)) == VMM_OK) {
		osc->rate_min = range[0];
		osc->rate_max = range[1];
	}

	init.name = NULL;
	vmm_devtree_read_string(node, "clock-output-names", &init.name);
	if (!init.name)
		init.name = node->name;

	init.ops = &vexpress_osc_ops;
	init.flags = CLK_IS_ROOT;
	init.num_parents = 0;

	osc->hw.init = &init;

	clk = clk_register(NULL, &osc->hw);
	if (!clk) {
		vmm_printf("%s: Failed to register clock '%s'!\n",
			   __func__, init.name);
		goto error;
	}

	of_clk_add_provider(node, of_clk_src_simple_get, clk);

	DPRINTF("%s: Registered clock '%s'\n", __func__, init.name);

	return;

error:
	if (osc->func)
		vexpress_config_func_put(osc->func);
	vmm_free(osc);
}
CLK_OF_DECLARE(vexpress_soc, "arm,vexpress-osc", vexpress_osc_of_setup);
