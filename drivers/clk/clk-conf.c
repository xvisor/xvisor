/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWid
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski for Xvisor port.
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 * Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @file clk-conf.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Device tree clock configuration helper.
 */

#include <vmm_modules.h>
#include <vmm_types.h>
#include <vmm_devtree.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <drv/clk.h>

static int __set_clk_parents(struct vmm_devtree_node *node, bool clk_supplier)
{
	struct vmm_devtree_phandle_args clkspec;
	int index, rc, num_parents;
	struct clk *clk, *pclk;
	const char *list_name = "assigned-clock-parents";
	const char *cells_name = "assigned-clock-parents";

	num_parents = vmm_devtree_count_phandle_with_args(node, list_name,
							  cells_name);
	if (num_parents == VMM_EINVALID)
		vmm_printf("clk: invalid value of clock-parents property at "
			   "%s\n", node->name);

	for (index = 0; index < num_parents; index++) {
		rc = vmm_devtree_parse_phandle_with_args(node, list_name,
							 cells_name, index,
							 &clkspec);
		if (rc < 0) {
			/* skip empty (null) phandles */
			if (rc == VMM_ENOENT)
				continue;
			else
				return rc;
		}
		if (clkspec.np == node && !clk_supplier)
			return 0;
		pclk = of_clk_get_from_provider(&clkspec);
		if (VMM_IS_ERR_VALUE(pclk)) {
			vmm_printf("clk: couldn't get parent clock %d for "
				   "%s\n", index, node->name);
			return VMM_EFAIL;
		}

		rc = vmm_devtree_parse_phandle_with_args(node,
							 "assigned-clocks",
							 cells_name, index,
							 &clkspec);
		if (rc < 0)
			goto err;
		if (clkspec.np == node && !clk_supplier) {
			rc = 0;
			goto err;
		}
		clk = of_clk_get_from_provider(&clkspec);
		if (VMM_IS_ERR_VALUE(clk)) {
			vmm_printf("clk: couldn't get parent clock %d for "
				   "%s\n", index, node->name);
			rc = VMM_EFAIL;
			goto err;
		}

		rc = clk_set_parent(clk, pclk);
		if (rc < 0)
			vmm_printf("clk: failed to reparent %s to %s: %d\n",
				   __clk_get_name(clk), __clk_get_name(pclk),
				   rc);
		clk_put(clk);
		clk_put(pclk);
	}
	return 0;
err:
	clk_put(pclk);
	return rc;
}

static int __set_clk_rates(struct vmm_devtree_node *node, bool clk_supplier)
{
	struct vmm_devtree_phandle_args clkspec;
	int rate_idx = 0;
	int rc, index = 0;
	struct clk *clk;
	u32 rate;

	while (1) {
		rc = vmm_devtree_read_u32_atindex(node, "assigned-clock-rates",
						  &rate, rate_idx);
		if (VMM_OK != rc)
			break;
		++rate_idx;
		if (!rate)
			continue;
		rc = vmm_devtree_parse_phandle_with_args(node,
							 "assigned-clocks",
							 "#clock-cells",
							 index, &clkspec);
		if (rc < 0) {
			/* skip empty (null) phandles */
			if (rc == VMM_ENOENT)
				continue;
			else
				return rc;
		}
		if (clkspec.np == node && !clk_supplier)
			return 0;

		clk = of_clk_get_from_provider(&clkspec);
		if (VMM_IS_ERR_VALUE(clk)) {
			vmm_printf("clk: couldn't get clock %d for %s\n",
				   index, node->name);
			return VMM_PTR_ERR(clk);
		}

		rc = clk_set_rate(clk, rate);
		if (rc < 0)
			vmm_printf("clk: couldn't set %s clock rate: %d\n",
				   __clk_get_name(clk), rc);
		clk_put(clk);
		index++;
	}
	return 0;
}

/**
 * of_clk_set_defaults() - parse and set assigned clocks configuration
 * @node: device node to apply clock settings for
 * @clk_supplier: true if clocks supplied by @node should also be considered
 *
 * This function parses 'assigned-{clocks/clock-parents/clock-rates}' properties
 * and sets any specified clock parents and rates. The @clk_supplier argument
 * should be set to true if @node may be also a clock supplier of any clock
 * listed in its 'assigned-clocks' or 'assigned-clock-parents' properties.
 * If @clk_supplier is false the function exits returnning 0 as soon as it
 * determines the @node is also a supplier of any of the clocks.
 */
int of_clk_set_defaults(struct vmm_devtree_node *node, bool clk_supplier)
{
	int rc;

	if (!node)
		return 0;

	rc = __set_clk_parents(node, clk_supplier);
	if (rc < 0)
		return rc;

	return __set_clk_rates(node, clk_supplier);
}
VMM_EXPORT_SYMBOL_GPL(of_clk_set_defaults);
