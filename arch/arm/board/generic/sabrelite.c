/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file sabrelite.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX6 Sabrelite board specific code
 *
 * Adapted from linux/drivers/mfd/vexpres-sysreg.c
 *
 * Copyright (c) 2014 Anup Patel.
 *
 * The original source is licensed under GPL.
 *
 */
#include <vmm_error.h>
#include <vmm_devtree.h>
#include <vmm_chardev.h>

#include <generic_board.h>

#include <imx-common.h>
#include <imx-hardware.h>

/*
 * Initialization functions
 */
static void __init imx6q_init_irq(void)
{
	imx_gpc_init();
}

static void imx6_print_info(struct vmm_chardev *cdev)
{
	imx_print_silicon_rev(cpu_is_imx6dl() ? "i.MX6DL" : "i.MX6Q",
			      imx_get_soc_revision());
}

static int __init imx6_early_init(struct vmm_devtree_node *node)
{
	imx_print_silicon_rev(cpu_is_imx6dl() ? "i.MX6DL" : "i.MX6Q",
			      imx_get_soc_revision());
	imx_soc_device_init();
	imx6q_init_irq();

	return 0;
}

static int __init imx6_final_init(struct vmm_devtree_node *node)
{
	int rc;

	/* Setup arch specific command fo IMX6 */
	rc = imx6_command_setup();
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static struct generic_board imx6_info = {
	.name		= "iMX6",
	.early_init	= imx6_early_init,
	.final_init	= imx6_final_init,
	.print_info	= imx6_print_info,
};

GENERIC_BOARD_DECLARE(imx6, "fsl,imx6q", &imx6_info);
