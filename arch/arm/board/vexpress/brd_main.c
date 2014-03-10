/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file brd_main.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <arch_board.h>
#include <arch_timer.h>
#include <libs/vtemu.h>
#include <drv/clk-provider.h>
#include <drv/vexpress.h>

#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>

#include <versatile/clcd.h>

/*
 * Global board context
 */

static struct vexpress_config_func *muxfpga_func;
static struct vexpress_config_func *dvimode_func;

#if defined(CONFIG_VTEMU)
struct vtemu *v2m_vt;
#endif

/*
 * CLCD support.
 */

static void vexpress_clcd_enable(struct clcd_fb *fb)
{
	vexpress_config_write(muxfpga_func, 0, 0);
	vexpress_config_write(dvimode_func, 0, 2);
}

static int vexpress_clcd_setup(struct clcd_fb *fb)
{
	unsigned long framesize = 1024 * 768 * 2;

	fb->panel = versatile_clcd_get_panel("XVGA");
	if (!fb->panel)
		return VMM_EINVALID;

	return versatile_clcd_setup(fb, framesize);
}

static struct clcd_board clcd_system_data = {
	.name		= "VExpress",
	.caps		= CLCD_CAP_5551 | CLCD_CAP_565,
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.enable		= vexpress_clcd_enable,
	.setup		= vexpress_clcd_setup,
	.remove		= versatile_clcd_remove,
};

/*
 * Print board information
 */

void arch_board_print_info(struct vmm_chardev *cdev)
{
	/* FIXME: To be implemented. */
}

/*
 * Initialization functions
 */

int __init arch_board_early_init(void)
{
	struct vmm_devtree_node *node;

	/* Host aspace, Heap, Device tree, and Host IRQ available.
	 *
	 * Do necessary early stuff like:
	 * iomapping devices, 
	 * SOC clocking init, 
	 * Setting-up system data in device tree nodes,
	 * ....
	 */

	/* Sysreg early init */
	vexpress_sysreg_of_early_init();

	/* Initialize clocking framework */
	of_clk_init(NULL);

	/* Determine muxfpga function */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,vexpress-muxfpga");
	if (!node) {
		return VMM_ENODEV;
	}
	muxfpga_func = vexpress_config_func_get_by_node(node);
	if (!muxfpga_func) {
		return VMM_ENODEV;
	}

	/* Determine dvimode function */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,vexpress-dvimode");
	if (!node) {
		return VMM_ENODEV;
	}
	dvimode_func = vexpress_config_func_get_by_node(node);
	if (!dvimode_func) {
		return VMM_ENODEV;
	}

	/* Setup CLCD (before probing) */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,pl111");
	if (node) {
		node->system_data = &clcd_system_data;
	}

	return 0;
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;
#if defined(CONFIG_VTEMU)
	struct fb_info *info;
#endif

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Find simple-bus node */
	node = vmm_devtree_find_compatible(NULL, NULL, "simple-bus");
	if (!node) {
		return VMM_ENODEV;
	}

	/* Do probing using device driver framework */
	rc = vmm_devdrv_probe(node);
	if (rc) {
		return rc;
	}

	/* Create VTEMU instace if available*/
#if defined(CONFIG_VTEMU)
	info = fb_get(0);
	if (info) {
		v2m_vt = vtemu_create(info->name, info, NULL);
	}
#endif

	return VMM_OK;
}
