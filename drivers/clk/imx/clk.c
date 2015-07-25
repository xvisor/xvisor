/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/clk.c
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
 * @file clk.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX clock management function helpers
 */

#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include "clk.h"

DEFINE_SPINLOCK(imx_ccm_lock);

void __init imx_check_clocks(struct clk *clks[], unsigned int count)
{
	unsigned i;

	for (i = 0; i < count; i++) {
		if (IS_ERR(clks[i])) {
			pr_err("i.MX clk %u: register failed with %ld\n",
			       i, PTR_ERR(clks[i]));
		}
	}
}

static struct clk * __init imx_obtain_fixed_clock_from_dt(const char *name)
{
	unsigned int len = 0;
	struct vmm_devtree_phandle_args phandle;
	struct clk *clk = ERR_PTR(-ENODEV);
	char *path;

	len = strlen(name) + strlen("/clocks/") + 1;
	if (NULL == (path = kmalloc(GFP_KERNEL, len)))
	{
		vmm_printf("Failed to allocate fixed clock \"%s\" path "
			   "string\n", name);
		return NULL;
	}

	sprintf(path, "/clocks/%s", name);

	phandle.np = vmm_devtree_getnode(path);
	kfree(path);

	if (phandle.np) {
		clk = of_clk_get_from_provider(&phandle);
		of_node_put(phandle.np);
	}
	return clk;
}

struct clk * __init imx_obtain_fixed_clock(
			const char *name, unsigned long rate)
{
	struct clk *clk;

	clk = imx_obtain_fixed_clock_from_dt(name);
	if (IS_ERR(clk))
		clk = imx_clk_fixed(name, rate);
	return clk;
}

/*
 * This fixups the register CCM_CSCMR1 write value.
 * The write/read/divider values of the aclk_podf field
 * of that register have the relationship described by
 * the following table:
 *
 * write value       read value        divider
 * 3b'000            3b'110            7
 * 3b'001            3b'111            8
 * 3b'010            3b'100            5
 * 3b'011            3b'101            6
 * 3b'100            3b'010            3
 * 3b'101            3b'011            4
 * 3b'110            3b'000            1
 * 3b'111            3b'001            2(default)
 *
 * That's why we do the xor operation below.
 */
#define CSCMR1_FIXUP	0x00600000

void imx_cscmr1_fixup(u32 *val)
{
	*val ^= CSCMR1_FIXUP;
	return;
}
