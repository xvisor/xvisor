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
 * @file clk-devres.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic clocking devres APIs
 *
 * Adapted from linux/drivers/clk/clk-devres.c
 *
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_devdrv.h>
#include <vmm_devres.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <drv/clk.h>

static void devm_clk_release(struct vmm_device *dev, void *res)
{
	clk_put(*(struct clk **)res);
}

struct clk *devm_clk_get(struct vmm_device *dev, const char *id)
{
	struct clk **ptr, *clk;

	ptr = vmm_devres_alloc(devm_clk_release, sizeof(*ptr));
	if (!ptr)
		return VMM_ERR_PTR(VMM_ENOMEM);

	clk = clk_get(dev, id);
	if (!VMM_IS_ERR(clk)) {
		*ptr = clk;
		vmm_devres_add(dev, ptr);
	} else {
		vmm_devres_free(ptr);
	}

	return clk;
}
VMM_EXPORT_SYMBOL(devm_clk_get);

struct clk_bulk_devres {
	struct clk_bulk_data *clks;
	int num_clks;
};

static void devm_clk_bulk_release(struct vmm_device *dev, void *res)
{
	struct clk_bulk_devres *devres = res;

	clk_bulk_put(devres->num_clks, devres->clks);
}

int devm_clk_bulk_get(struct vmm_device *dev, int num_clks,
		      struct clk_bulk_data *clks)
{
	struct clk_bulk_devres *devres;
	int ret;

	devres = vmm_devres_alloc(devm_clk_bulk_release,
				  sizeof(*devres));
	if (!devres)
		return VMM_ENOMEM;

	ret = clk_bulk_get(dev, num_clks, clks);
	if (!ret) {
		devres->clks = clks;
		devres->num_clks = num_clks;
		vmm_devres_add(dev, devres);
	} else {
		vmm_devres_free(devres);
	}

	return ret;
}
VMM_EXPORT_SYMBOL(devm_clk_bulk_get);

static int devm_clk_match(struct vmm_device *dev, void *res, void *data)
{
	struct clk **c = res;
	if (!c || !*c) {
		WARN_ON(!c || !*c);
		return 0;
	}
	return *c == data;
}

void devm_clk_put(struct vmm_device *dev, struct clk *clk)
{
	int ret;

	ret = vmm_devres_release(dev, devm_clk_release, devm_clk_match, clk);

	WARN_ON(ret);
}
VMM_EXPORT_SYMBOL(devm_clk_put);

struct clk *devm_get_clk_from_child(struct vmm_device *dev,
				    struct vmm_devtree_node *np,
				    const char *con_id)
{
	struct clk **ptr, *clk;

	ptr = vmm_devres_alloc(devm_clk_release, sizeof(*ptr));
	if (!ptr)
		return VMM_ERR_PTR(VMM_ENOMEM);

	clk = of_clk_get_by_name(np, con_id);
	if (!VMM_IS_ERR(clk)) {
		*ptr = clk;
		vmm_devres_add(dev, ptr);
	} else {
		vmm_devres_free(ptr);
	}

	return clk;
}
VMM_EXPORT_SYMBOL(devm_get_clk_from_child);
