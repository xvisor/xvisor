/**
 * Copyright (c) 2011 Sukanto Ghosh
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
 * @file brd_timer.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source code for Beagle board specific general purpose timers
 *
 */

#include <arch_timer.h>
#include <omap3/intc.h>
#include <omap3/gpt.h>
#include <omap3/s32k-timer.h>
#include <omap3/prcm.h>
#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_host_aspace.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>

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

