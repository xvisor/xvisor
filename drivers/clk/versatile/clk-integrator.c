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
 * @file clk-integrator.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Integrator/AP and Integrator/CP boards clock implementation
 *
 * Adapted from linux/drivers/clk/versatile/clk-integrator.c
 *
 * Clock driver for the ARM Integrator/AP and Integrator/CP boards
 * Copyright (C) 2012 Linus Walleij
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_host_aspace.h>
#include <drv/clk.h>
#include <drv/clkdev.h>
#include <drv/clk-provider.h>
#include <drv/platform_data/clk-integrator.h>

#include "clk-icst.h"

#define INTEGRATOR_HDR_BASE             0x10000000
#define INTEGRATOR_HDR_LOCK_OFFSET      0x14

/*
 * Implementation of the ARM Integrator/AP and Integrator/CP clock tree.
 * Inspired by portions of:
 * plat-versatile/clock.c and plat-versatile/include/plat/clock.h
 */

static const struct icst_params cp_auxvco_params = {
	.ref		= 24000000,
	.vco_max	= ICST525_VCO_MAX_5V,
	.vco_min	= ICST525_VCO_MIN,
	.vd_min 	= 8,
	.vd_max 	= 263,
	.rd_min 	= 3,
	.rd_max 	= 65,
	.s2div		= icst525_s2div,
	.idx2s		= icst525_idx2s,
};

static const struct clk_icst_desc __initdata cp_icst_desc = {
	.params = &cp_auxvco_params,
	.vco_offset = 0x1c,
	.lock_offset = INTEGRATOR_HDR_LOCK_OFFSET,
};

/*
 * integrator_clk_init() - set up the integrator clock tree
 * @is_cp: pass true if it's the Integrator/CP else AP is assumed
 */
void __init integrator_clk_init(bool is_cp)
{
	struct clk *clk;
	virtual_addr_t hdr_base;

	/* Map HDR base */
	hdr_base = vmm_host_iomap(INTEGRATOR_HDR_BASE, VMM_PAGE_SIZE);

	/* APB clock dummy */
	clk = clk_register_fixed_rate(NULL, "apb_pclk", NULL, CLK_IS_ROOT, 0);
	clk_register_clkdev(clk, "apb_pclk", NULL);

	/* UART reference clock */
	clk = clk_register_fixed_rate(NULL, "uartclk", NULL, CLK_IS_ROOT,
				14745600);
	clk_register_clkdev(clk, NULL, "uart0");
	clk_register_clkdev(clk, NULL, "uart1");
	if (is_cp)
		clk_register_clkdev(clk, NULL, "mmci");

	/* 24 MHz clock */
	clk = clk_register_fixed_rate(NULL, "clk24mhz", NULL, CLK_IS_ROOT,
				24000000);
	clk_register_clkdev(clk, NULL, "kmi0");
	clk_register_clkdev(clk, NULL, "kmi1");
	if (!is_cp)
		clk_register_clkdev(clk, NULL, "ap_timer");

	if (!is_cp)
		return;

	/* 1 MHz clock */
	clk = clk_register_fixed_rate(NULL, "clk1mhz", NULL, CLK_IS_ROOT,
				1000000);
	clk_register_clkdev(clk, NULL, "sp804");

	/* ICST VCO clock used on the Integrator/CP CLCD */
	clk = icst_clk_register(NULL, &cp_icst_desc, (void *)hdr_base);
	clk_register_clkdev(clk, NULL, "clcd");
}
