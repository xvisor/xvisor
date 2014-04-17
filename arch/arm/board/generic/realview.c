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
 * @file realview.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief realview board specific code
 */

#include <vmm_error.h>
#include <drv/realview.h>
#include <drv/clk-provider.h>
#include <drv/platform_data/clk-realview.h>

#include <generic_board.h>

#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>

#include <versatile/clcd.h>

/*
 * CLCD support.
 */

/*
 * Disable all display connectors on the interface module.
 */
static void realview_clcd_disable(struct clcd_fb *fb)
{
	realview_clcd_disable_power();
}

/*
 * Enable the relevant connector on the interface module.
 */
static void realview_clcd_enable(struct clcd_fb *fb)
{
	realview_clcd_enable_power();
}

/*
 * Detect which LCD panel is connected, and return the appropriate
 * clcd_panel structure.  Note: we do not have any information on
 * the required timings for the 8.4in panel, so we presently assume
 * VGA timings.
 */
static int realview_clcd_setup(struct clcd_fb *fb)
{
	const char *panel_name;
	unsigned long framesize;

	framesize = 1024 * 768 * 2;
	panel_name = realview_clcd_panel_name();

	fb->panel = versatile_clcd_get_panel(panel_name);
	if (!fb->panel)
		return VMM_EINVALID;

	return versatile_clcd_setup_dma(fb, framesize);
}

static struct clcd_board clcd_system_data = {
	.name		= "Realview",
	.caps		= CLCD_CAP_ALL,
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.disable	= realview_clcd_disable,
	.enable		= realview_clcd_enable,
	.setup		= realview_clcd_setup,
	.remove		= versatile_clcd_remove,
};

/*
 * Initialization functions
 */

static int __init realview_early_init(struct vmm_devtree_node *node)
{
	/* Initialize sysreg */
	realview_sysreg_of_early_init();

	/* Intialize realview clocking */
	realview_clk_init((void *)realview_system_base(), FALSE);

	/* Setup CLCD (before probing) */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,pl111");
	if (node) {
		node->system_data = &clcd_system_data;
	}

	return VMM_OK;
}

static int __init realview_final_init(struct vmm_devtree_node *node)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static struct generic_board realview_info = {
	.name		= "Realview",
	.early_init	= realview_early_init,
	.final_init	= realview_final_init,
};

GENERIC_BOARD_DECLARE(realview, "arm,realview", &realview_info);
