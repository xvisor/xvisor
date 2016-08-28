/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file foundation-v8.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Foundation v8 board specific code
 */

#include <vmm_error.h>
#include <vmm_devtree.h>

#include <generic_board.h>

#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>

#include <versatile/clcd.h>

/*
 * CLCD support.
 */

static void foundation_v8_clcd_enable(struct clcd_fb *fb)
{
	/* Nothing to do here. */
}

static int foundation_v8_clcd_setup(struct clcd_fb *fb)
{
	unsigned long framesize = 1024 * 768 * 2;

	fb->panel = versatile_clcd_get_panel("XVGA");
	if (!fb->panel)
		return VMM_EINVALID;

	return versatile_clcd_setup_dma(fb, framesize);
}

static struct clcd_board clcd_system_data = {
	.name		= "Foundation-v8",
	.caps		= CLCD_CAP_5551 | CLCD_CAP_565,
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.enable		= foundation_v8_clcd_enable,
	.setup		= foundation_v8_clcd_setup,
	.remove		= versatile_clcd_remove,
};

/*
 * Initialization functions
 */

static int __init foundation_v8_early_init(struct vmm_devtree_node *node)
{
	/* Setup CLCD (before probing) */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,pl111");
	if (node) {
		node->system_data = &clcd_system_data;
	}
	vmm_devtree_dref_node(node);

	return 0;
}

static int __init foundation_v8_final_init(struct vmm_devtree_node *node)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static struct generic_board foundation_v8_info = {
	.name		= "Foundation-v8",
	.early_init	= foundation_v8_early_init,
	.final_init	= foundation_v8_final_init,
};

GENERIC_BOARD_DECLARE(fv8, "arm,foundation-v8", &foundation_v8_info);
