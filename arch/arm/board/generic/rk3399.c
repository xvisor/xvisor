/**
 * Copyright (c) 2019 PT Switch.
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
 * @file rk3399.c
 * @author Andy Dean (acd@kynesim.co.uk)
 * @brief RK3399 SOC specific code
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_host_io.h>
#include <libs/mathlib.h>

#include <generic_timer.h>
#include <cpu_generic_timer.h>

#include <generic_board.h>

#include <linux/clk-provider.h>

/*
 * Initialization functions
 */

static int __init rk3399_early_init(struct vmm_devtree_node *node)
{
	int rc = VMM_OK;

	return rc;
}

static int __init rk3399_final_init(struct vmm_devtree_node *node)
{
	/* Turn off unused clocks */
	clk_disable_unused();

	return VMM_OK;
}

static struct generic_board rk3399_info = {
	.name		= "RK3399",
	.early_init	= rk3399_early_init,
	.final_init	= rk3399_final_init,
};

GENERIC_BOARD_DECLARE(rk3399, "rockchip,rk3399", &rk3399_info);
