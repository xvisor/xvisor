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
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_stdio.h>
#include <vmm_chardev.h>
#include <arch_timer.h>
#include <libfdt.h>
#include <omap3/prcm.h>
#include <omap3/sdrc.h>
#include <omap3/intc.h>
#include <omap3/gpt.h>
#include <omap3/s32k-timer.h>
#include <omap3/prcm.h>

/*
 * Device Tree support
 */

extern u32 dt_blob_start;

int arch_board_ram_start(physical_addr_t * addr)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_node;
	
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

	rc = libfdt_get_property(&fdt, fdt_node,
				 VMM_DEVTREE_MEMORY_PHYS_ADDR_ATTR_NAME, addr);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int arch_board_ram_size(physical_size_t *size)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_node;
	
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

	rc = libfdt_get_property(&fdt, fdt_node,
				 VMM_DEVTREE_MEMORY_PHYS_SIZE_ATTR_NAME, size);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int arch_board_devtree_populate(struct vmm_devtree_node ** root)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	
	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	return libfdt_parse_devtree(&fdt, root);
}

/*
 * Reset & Shutdown
 */

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

/*
 * Initialization functions
 */

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

#define BEAGLE_CLK_EVENT_GPT	0 

#ifndef CONFIG_OMAP3_CLKSRC_S32KT
#define BEAGLE_CLK_SRC_GPT	1 
#endif

struct omap3_gpt_cfg beagle_gpt_cfg[] = {
	{
		.name =		"gpt1",
		.base_pa =	OMAP3_GPT1_BASE,
		.cm_domain =	OMAP3_WKUP_CM,
		.clksel_mask = 	OMAP3_CM_CLKSEL_WKUP_CLKSEL_GPT1_M,
		.iclken_mask =	OMAP3_CM_ICLKEN_WKUP_EN_GPT1_M,
		.fclken_mask =  OMAP3_CM_FCLKEN_WKUP_EN_GPT1_M,	
		.src_sys_clk =	TRUE,
		.irq_no	=	OMAP3_MPU_INTC_GPT1_IRQ
	},
	{
		.name =		"gpt2",
		.base_pa =	OMAP3_GPT2_BASE,
		.cm_domain =	OMAP3_PER_CM,
		.clksel_mask = 	OMAP3_CM_CLKSEL_PER_CLKSEL_GPT2_M,
		.iclken_mask =	OMAP3_CM_ICLKEN_PER_EN_GPT2_M,
		.fclken_mask =  OMAP3_CM_FCLKEN_PER_EN_GPT2_M,	
		.src_sys_clk =	TRUE,
		.irq_no	=	OMAP3_MPU_INTC_GPT2_IRQ
	}
};

int __init arch_clocksource_init(void)
{
#ifdef CONFIG_OMAP3_CLKSRC_S32KT
	return omap3_s32k_clocksource_init();
#else
	omap3_gpt_global_init(sizeof(beagle_gpt_cfg)/sizeof(struct omap3_gpt_cfg), 
			beagle_gpt_cfg);
	return omap3_gpt_clocksource_init(BEAGLE_CLK_SRC_GPT, 
					  OMAP3_GLOBAL_REG_PRM);
#endif
}

int __init arch_clockchip_init(void)
{
	omap3_gpt_global_init(sizeof(beagle_gpt_cfg)/sizeof(struct omap3_gpt_cfg), 
			beagle_gpt_cfg);

	return omap3_gpt_clockchip_init(BEAGLE_CLK_EVENT_GPT, 
					OMAP3_GLOBAL_REG_PRM);
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;
	struct vmm_chardev * cdev;

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Do Probing using device driver framework */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "l3");

	if (!node) {
		return VMM_ENOTAVAIL;
	}

	rc = vmm_devdrv_probe(node, NULL, NULL);
	if (rc) {
		return rc;
	}

	/* Find uart0 character device and 
	 * set it as vmm_stdio character device */
	if ((cdev = vmm_chardev_find("uart0"))) {
		vmm_stdio_change_device(cdev);
	}

	return VMM_OK;

}
