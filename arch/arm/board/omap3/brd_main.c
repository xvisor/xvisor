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
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <arch_board.h>
#include <arch_timer.h>
#include <omap3_plat.h>
#include <omap3_prcm.h>
#include <omap/sdrc.h>
#include <omap/intc.h>
#include <omap/gpt.h>
#include <omap/s32k-timer.h>

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
static struct sdrc_params mt46h32m32lf6_sdrc_params[] = {
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
	if ((rc = cm_init())) {
		return rc;
	}

	/* Initialize Power & Reset Mamagment */
	if ((rc = prm_init())) {
		return rc;
	}

	/* Enable I-clock for S32K timer 
	 * Note: S32K is our reference clocksource and also used
	 * as clock reference for GPTs
	 */
	cm_setbits(OMAP3_WKUP_CM, 
		   OMAP3_CM_ICLKEN_WKUP, 
		   OMAP3_CM_ICLKEN_WKUP_EN_32KSYNC_M);

	/* Initialize SDRAM Controller (SDRC) */
	if ((rc = sdrc_init(OMAP3_SDRC_BASE,
			    OMAP3_SMS_BASE,
			    mt46h32m32lf6_sdrc_params, 
			    mt46h32m32lf6_sdrc_params))) {
		return rc;
	}

	return 0;
}

#define OMAP3_CLK_EVENT_GPT	0 

#ifndef CONFIG_OMAP3_CLKSRC_S32KT
#define OMAP3_CLK_SRC_GPT	1 
#endif

struct gpt_cfg {
	const char *name;
	physical_addr_t base_pa;
	u32 cm_domain;
	u32 clksel_offset;
	u32 clksel_mask;
	u32 iclken_offset;
	u32 iclken_mask;
	u32 fclken_offset;
	u32 fclken_mask;
	bool src_sys_clk;
	u32 clk_hz;
	u32 irq_no;
};

struct gpt_cfg omap3_gpt[] = {
	{
		.name		= "gpt1",
		.base_pa	= OMAP3_GPT1_BASE,
		.cm_domain	= OMAP3_WKUP_CM,
		.clksel_offset	= OMAP3_CM_CLKSEL,
		.clksel_mask	= OMAP3_CM_CLKSEL_WKUP_CLKSEL_GPT1_M,
		.iclken_offset	= OMAP3_CM_ICLKEN,
		.iclken_mask	= OMAP3_CM_ICLKEN_WKUP_EN_GPT1_M,
		.fclken_offset	= OMAP3_CM_FCLKEN,
		.fclken_mask	= OMAP3_CM_FCLKEN_WKUP_EN_GPT1_M,	
		.src_sys_clk	= TRUE,
		.irq_no	=	OMAP3_MPU_INTC_GPT1_IRQ
	},
	{
		.name		= "gpt2",
		.base_pa	= OMAP3_GPT2_BASE,
		.cm_domain	= OMAP3_PER_CM,
		.clksel_offset	= OMAP3_CM_CLKSEL,
		.clksel_mask	= OMAP3_CM_CLKSEL_PER_CLKSEL_GPT2_M,
		.iclken_offset	= OMAP3_CM_ICLKEN,
		.iclken_mask	= OMAP3_CM_ICLKEN_PER_EN_GPT2_M,
		.fclken_offset	= OMAP3_CM_FCLKEN,
		.fclken_mask	= OMAP3_CM_FCLKEN_PER_EN_GPT2_M,	
		.src_sys_clk	= TRUE,
		.irq_no		= OMAP3_MPU_INTC_GPT2_IRQ
	}
};

static u32 get_osc_clk_speed(u32 gpt_num, u32 sys_clk_div)
{
	u32 osc_clk_hz = 0, regval;
	u32 start, cstart, cend, cdiff;
	virtual_addr_t gpt_va, s32k_va;

	/* Map gpt registers */
	gpt_va = vmm_host_iomap(omap3_gpt[gpt_num].base_pa, 0x1000);

	/* Map s32k registers */
	s32k_va = vmm_host_iomap(OMAP3_S32K_BASE, 0x1000);

	/* Start counting at 0 */
	vmm_writel(0, (void *)(gpt_va + GPT_TLDR));

	/* Enable GPT */
	vmm_writel(GPT_TCLR_ST_M, (void *)(gpt_va + GPT_TCLR));

	/* Start time in 20 cycles */
	start = 20 + vmm_readl((void *)(s32k_va + S32K_CR));

	/* Dead loop till start time */
	while (vmm_readl((void *)(s32k_va + S32K_CR)) < start);

	/* Get start sys_clk count */
	cstart = vmm_readl((void *)(gpt_va + GPT_TCRR));

	/* Wait for 20 cycles */
	while (vmm_readl((void *)(s32k_va + S32K_CR)) < (start + 20)) ;
	cend = vmm_readl((void *)(gpt_va + GPT_TCRR));
	cdiff = cend - cstart;	/* get elapsed ticks */
	cdiff *= sys_clk_div;

	/* Stop Timer (TCLR[ST] = 0) */
	regval = vmm_readl((void *)(gpt_va + GPT_TCLR));
	regval &= ~GPT_TCLR_ST_M;
	vmm_writel(regval, (void *)(gpt_va + GPT_TCLR));

	/* Based on number of ticks assign speed */
	if (cdiff > 19000) {
		osc_clk_hz = OMAP3_SYSCLK_S38_4M;
	} else if (cdiff > 15200) {
		osc_clk_hz = OMAP3_SYSCLK_S26M;
	} else if (cdiff > 13000) {
		osc_clk_hz = OMAP3_SYSCLK_S24M;
	} else if (cdiff > 9000) {
		osc_clk_hz = OMAP3_SYSCLK_S19_2M;
	} else if (cdiff > 7600) {
		osc_clk_hz = OMAP3_SYSCLK_S13M;
	} else {
		osc_clk_hz = OMAP3_SYSCLK_S12M;
	}

	/* Unmap s32k registers */
	vmm_host_iounmap(s32k_va, 0x1000);

	/* Unmap gpt registers */
	vmm_host_iounmap(gpt_va, 0x1000);

	return osc_clk_hz >> (sys_clk_div - 1);
}

static void omap3_gpt_clock_enable(u32 gpt_num)
{
	u32 sys_div;

	/* select clock source (1=sys_clk; 0=32K) for GPT */
	if (omap3_gpt[gpt_num].src_sys_clk) {
		sys_div = prm_read(OMAP3_GLOBAL_REG_PRM, OMAP3_PRM_CLKSRC_CTRL);
		sys_div = (sys_div & OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_M) 
			>> OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_S;
		cm_setbits(omap3_gpt[gpt_num].cm_domain, 
			   omap3_gpt[gpt_num].clksel_offset,
			   omap3_gpt[gpt_num].clksel_mask);
		omap3_gpt[gpt_num].clk_hz = 
					get_osc_clk_speed(gpt_num, sys_div);
	} else {
		cm_clrbits(omap3_gpt[gpt_num].cm_domain, 
			   omap3_gpt[gpt_num].clksel_offset,
			   omap3_gpt[gpt_num].clksel_mask);
		omap3_gpt[gpt_num].clk_hz = S32K_FREQ_HZ;
	}

	/* Enable I Clock for GPT */
	cm_setbits(omap3_gpt[gpt_num].cm_domain, 
		   omap3_gpt[gpt_num].iclken_offset, 
		   omap3_gpt[gpt_num].iclken_mask);

	/* Enable F Clock for GPT */
	cm_setbits(omap3_gpt[gpt_num].cm_domain, 
		   omap3_gpt[gpt_num].fclken_offset, 
		   omap3_gpt[gpt_num].fclken_mask);
}

int __init arch_clocksource_init(void)
{
#ifdef CONFIG_OMAP3_CLKSRC_S32KT
	return s32k_clocksource_init(OMAP3_S32K_BASE);
#else
	u32 gpt_num = OMAP3_CLK_SRC_GPT;

	omap3_gpt_clock_enable(gpt_num);

	return gpt_clocksource_init(omap3_gpt[gpt_num].name, 
				    omap3_gpt[gpt_num].base_pa, 
				    omap3_gpt[gpt_num].clk_hz);
#endif
}

int __cpuinit arch_clockchip_init(void)
{
	u32 gpt_num = OMAP3_CLK_EVENT_GPT;

	omap3_gpt_clock_enable(gpt_num);

	return gpt_clockchip_init(omap3_gpt[gpt_num].name, 
				  omap3_gpt[gpt_num].base_pa, 
				  omap3_gpt[gpt_num].clk_hz,
				  omap3_gpt[gpt_num].irq_no);
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;

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

	return VMM_OK;
}
