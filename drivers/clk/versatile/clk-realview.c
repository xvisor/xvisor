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
 * @file clk-realview.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM RealView boards clock implementation
 *
 * Adapted from linux/drivers/clk/versatile/clk-realview.c
 *
 * Clock driver for the ARM RealView boards
 * Copyright (C) 2012 Linus Walleij
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <vmm_error.h>
#include <vmm_compiler.h>
#include <drv/clk.h>
#include <drv/clkdev.h>
#include <drv/clk-provider.h>
#include <drv/platform_data/clk-realview.h>

#include "clk-icst.h"

#define REALVIEW_SYS_OSC0_OFFSET             0x0C
#define REALVIEW_SYS_OSC4_OFFSET             0x1C	/* OSC1 for RealView/AB */
#define REALVIEW_SYS_LOCK_OFFSET             0x20

/*
 * Implementation of the ARM RealView clock trees.
 */

static const struct icst_params realview_oscvco_params = {
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

static const struct clk_icst_desc __initdata realview_osc0_desc = {
	.params = &realview_oscvco_params,
	.vco_offset = REALVIEW_SYS_OSC0_OFFSET,
	.lock_offset = REALVIEW_SYS_LOCK_OFFSET,
};

static const struct clk_icst_desc __initdata realview_osc4_desc = {
	.params = &realview_oscvco_params,
	.vco_offset = REALVIEW_SYS_OSC4_OFFSET,
	.lock_offset = REALVIEW_SYS_LOCK_OFFSET,
};

/*
 * realview_clk_init() - set up the RealView clock tree
 */
void __init realview_clk_init(void *sysbase, bool is_pb1176)
{
	struct clk *clk;

	/* APB clock dummy */
	clk = clk_register_fixed_rate(NULL, "apb_pclk", NULL, CLK_IS_ROOT, 0);
	clk_register_clkdev(clk, "apb_pclk", NULL);

	/* 24 MHz clock */
	clk = clk_register_fixed_rate(NULL, "clk24mhz", NULL, CLK_IS_ROOT,
				24000000);
	clk_register_clkdev(clk, NULL, "dev:uart0");
	clk_register_clkdev(clk, NULL, "dev:uart1");
	clk_register_clkdev(clk, NULL, "dev:uart2");
	clk_register_clkdev(clk, NULL, "fpga:kmi0");
	clk_register_clkdev(clk, NULL, "fpga:kmi1");
	clk_register_clkdev(clk, NULL, "fpga:mmc0");
	clk_register_clkdev(clk, NULL, "dev:ssp0");
	if (is_pb1176) {
		/*
		 * UART3 is on the dev chip in PB1176
		 * UART4 only exists in PB1176
		 */
		clk_register_clkdev(clk, NULL, "dev:uart3");
		clk_register_clkdev(clk, NULL, "dev:uart4");
	} else
		clk_register_clkdev(clk, NULL, "fpga:uart3");


	/* 1 MHz clock */
	clk = clk_register_fixed_rate(NULL, "clk1mhz", NULL, CLK_IS_ROOT,
				      1000000);
	clk_register_clkdev(clk, NULL, "sp804");

	/* ICST VCO clock */
	if (is_pb1176)
		clk = icst_clk_register(NULL, &realview_osc0_desc, sysbase);
	else
		clk = icst_clk_register(NULL, &realview_osc4_desc, sysbase);

	clk_register_clkdev(clk, NULL, "dev:clcd");
	clk_register_clkdev(clk, NULL, "issp:clcd");
}
