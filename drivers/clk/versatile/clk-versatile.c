/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file clk-versatile.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Versatile boards clock implementation
 */
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/platform_data/clk-versatile.h>

#include "clk-icst.h"

#define VERSATILE_SYS_OSC4_OFFSET             0x1C
#define VERSATILE_SYS_LOCK_OFFSET             0x20

/*
 * Implementation of the ARM Versatile clock trees.
 */

static const struct icst_params versatile_oscvco_params = {
	.ref		= 24000000,
	.vco_max	= ICST307_VCO_MAX,
	.vco_min	= ICST307_VCO_MIN,
	.vd_min		= 4 + 8,
	.vd_max		= 511 + 8,
	.rd_min		= 1 + 2,
	.rd_max		= 127 + 2,
	.s2div		= icst307_s2div,
	.idx2s		= icst307_idx2s,
};

static const struct clk_icst_desc __initdata versatile_osc4_desc = {
	.params = &versatile_oscvco_params,
	.vco_offset = VERSATILE_SYS_OSC4_OFFSET,
	.lock_offset = VERSATILE_SYS_LOCK_OFFSET,
};

/*
 * versatile_clk_init() - set up the Versatile clock tree
 */
void __init versatile_clk_init(void *sysbase)
{
	struct clk *clk;

	/* ICST VCO clock */
	clk = icst_clk_register(NULL, &versatile_osc4_desc,
				"osc4", NULL, sysbase);

	/* FIXME: Dummy clocks to force match with device tree node names */
	clk_register_clkdev(clk, NULL, "clcd");
}
