/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <libfdt.h>
#include <omap3/prcm.h>
#include <omap3/sdrc.h>

extern u32 dt_blob_start;

int arch_board_ram_start(physical_addr_t * addr)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header * fdt_node;
	struct fdt_property * prop;
	
	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	fdt_node = libfdt_find_node(&fdt, 
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_HOSTINFO_NODE_NAME
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	prop = libfdt_get_property(&fdt, fdt_node,
				   VMM_DEVTREE_MEMORY_PHYS_ADDR_ATTR_NAME);
	if (!prop) {
		return VMM_EFAIL;
	}
	*addr = *((physical_addr_t *)prop->data);

	return VMM_OK;
}

int arch_board_ram_size(physical_size_t * size)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header * fdt_node;
	struct fdt_property * prop;
	
	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	fdt_node = libfdt_find_node(&fdt, 
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_HOSTINFO_NODE_NAME
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	prop = libfdt_get_property(&fdt, fdt_node,
				   VMM_DEVTREE_MEMORY_PHYS_SIZE_ATTR_NAME);
	if (!prop) {
		return VMM_EFAIL;
	}
	*size = *((physical_size_t *)prop->data);

	return VMM_OK;
}

int arch_devtree_populate(struct vmm_devtree_node ** root)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	
	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	return libfdt_parse_devtree(&fdt, root);
}

int arch_board_reset(void)
{
	/* FIXME: TBD */
	return VMM_OK;
}

int arch_board_shutdown(void)
{
	/* FIXME: TBD */
	return VMM_OK;
}

/* Micron MT46H32M32LF-6 */
/* XXX Using ARE = 0x1 (no autorefresh burst) -- can this be changed? */
static struct omap3_sdrc_params mt46h32m32lf6_sdrc_params[] = {
	[0] = {
		.rate	     = 166000000,
		.actim_ctrla = 0x9a9db4c6,
		.actim_ctrlb = 0x00011217,
		.rfr_ctrl    = 0x0004dc01,
		.mr	     = 0x00000032,
	},
	[1] = {
		.rate	     = 165941176,
		.actim_ctrla = 0x9a9db4c6,
		.actim_ctrlb = 0x00011217,
		.rfr_ctrl    = 0x0004dc01,
		.mr	     = 0x00000032,
	},
	[2] = {
		.rate	     = 83000000,
		.actim_ctrla = 0x51512283,
		.actim_ctrlb = 0x0001120c,
		.rfr_ctrl    = 0x00025501,
		.mr	     = 0x00000032,
	},
	[3] = {
		.rate	     = 82970588,
		.actim_ctrla = 0x51512283,
		.actim_ctrlb = 0x0001120c,
		.rfr_ctrl    = 0x00025501,
		.mr	     = 0x00000032,
	},
	[4] = {
		.rate	     = 0
	},
};

int __init arch_board_early_init(void)
{
	int rc;

	/* Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */

	/* The function omap3_beagle_init_early() of 
	 * <linux>/arch/arm/mach-omap2/board-omap3beagle.c
	 * does the following:
	 *   1. Initialize Clock & Power Domains using function 
	 *      omap2_init_common_infrastructure() of
	 *      <linux>/arch/arm/mach-omap2/io.c
	 *   2. Initialize & Reprogram Clock of SDRC using function
	 *      omap2_sdrc_init() of <linux>/arch/arm/mach-omap2/sdrc.c
	 */

	/* Initialize Clock Mamagment */
	if ((rc = omap3_cm_init())) {
		return rc;
	}

	/* Initialize Power & Reset Mamagment */
	if ((rc = omap3_prm_init())) {
		return rc;
	}

	/* Initialize SDRAM Controller (SDRC) */
	if ((rc = omap3_sdrc_init(mt46h32m32lf6_sdrc_params, 
				  mt46h32m32lf6_sdrc_params))) {
		return rc;
	}

	return 0;
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Do Probing using device driver framework */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "l3");

	if (!node) {
		return VMM_ENOTAVAIL;
	}

	rc = vmm_devdrv_probe(node, NULL);
	if (rc) {
		return rc;
	}

	return VMM_OK;

}
