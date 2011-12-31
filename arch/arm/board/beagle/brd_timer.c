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
 * @version 1.0
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source code for Beagle board specific general purpose timers
 *
 */

#include <vmm_cpu.h>
#include <vmm_board.h>
#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_host_aspace.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_math.h>
#include <omap3/intc.h>
#include <omap3/gpt.h>
#include <omap3/s32k-timer.h>
#include <omap3/prcm.h>

#define BEAGLE_CLK_EVENT_GPT	0 

#ifndef CONFIG_BEAGLE_CLKSRC_S32KT
#define BEAGLE_CLK_SRC_GPT	1 
#endif

omap3_gpt_cfg_t beagle_gpt_cfg[] = {
	{
		.base_pa =	OMAP3_GPT1_BASE,
		.cm_domain =	OMAP3_WKUP_CM,
		.clksel_mask = 	OMAP3_CM_CLKSEL_WKUP_CLKSEL_GPT1_M,
		.iclken_mask =	OMAP3_CM_ICLKEN_WKUP_EN_GPT1_M,
		.fclken_mask =  OMAP3_CM_FCLKEN_WKUP_EN_GPT1_M,	
		.src_sys_clk =	TRUE,
		.irq_no	=	OMAP3_MPU_INTC_GPT1_IRQ
	},
	{
		.base_pa =	OMAP3_GPT2_BASE,
		.cm_domain =	OMAP3_PER_CM,
		.clksel_mask = 	OMAP3_CM_CLKSEL_PER_CLKSEL_GPT2_M,
		.iclken_mask =	OMAP3_CM_ICLKEN_PER_EN_GPT2_M,
		.fclken_mask =  OMAP3_CM_FCLKEN_PER_EN_GPT2_M,	
		.src_sys_clk =	TRUE,
		.irq_no	=	OMAP3_MPU_INTC_GPT2_IRQ
	}
};

#ifdef CONFIG_BEAGLE_CLKSRC_S32KT
u64 vmm_cpu_clocksource_cycles(void)
{
	return ((u64)omap3_s32k_get_counter());
}

u64 vmm_cpu_clocksource_mask(void)
{
	return 0xFFFFFFFF;
}

u32 vmm_cpu_clocksource_mult(void)
{
	return vmm_timer_clocksource_hz2mult(OMAP3_S32K_FREQ_HZ, 15);
}

u32 vmm_cpu_clocksource_shift(void)
{
	return 15;
}

int vmm_cpu_clocksource_init(void)
{
	return omap3_s32k_init();
}
#else
u64 vmm_cpu_clocksource_cycles(void)
{
	return omap3_gpt_get_counter(BEAGLE_CLK_SRC_GPT);
}

u64 vmm_cpu_clocksource_mask(void)
{
	return 0xFFFFFFFF;
}

u32 vmm_cpu_clocksource_mult(void)
{
	return vmm_timer_clocksource_khz2mult((beagle_gpt_cfg[BEAGLE_CLK_SRC_GPT].clk_hz)/1000, 24);
}

u32 vmm_cpu_clocksource_shift(void)
{
	return 24;
}

int vmm_cpu_clocksource_init(void)
{
	omap3_gpt_global_init(sizeof(beagle_gpt_cfg)/sizeof(omap3_gpt_cfg_t), 
			beagle_gpt_cfg);
	omap3_gpt_instance_init(BEAGLE_CLK_SRC_GPT, OMAP3_GLOBAL_REG_PRM, NULL);
	omap3_gpt_continuous(BEAGLE_CLK_SRC_GPT);
	return 0;
}
#endif

int vmm_cpu_timer_irq_handler(u32 irq_no, vmm_user_regs_t * regs, void *dev)
{
	omap3_gpt_ack_irq(BEAGLE_CLK_EVENT_GPT);
	omap3_gpt_stop(BEAGLE_CLK_EVENT_GPT);
	vmm_timer_clockevent_process(regs);

	return VMM_OK;
}

int vmm_cpu_clockevent_start(u64 nsecs)
{
	u32 usecs;

	/* Expected microseconds is usecs = (nsecs / 1000).
	 * In integer arithmetic this can be approximated 
	 * as follows:
	 * usecs = (nsecs / 1000)
	 *       = (nsecs / 1024) * (1024 / 1000)
	 *       = (nsecs / 1024) + (nsecs / 1024) * (24 / 1000)
	 *       = (nsecs >> 10) + (nsecs >> 10) * (3 / 125)
	 *       = (nsecs >> 10) + (nsecs >> 10) * (3 / 128) * (128 / 125)
	 *       = (nsecs >> 10) + (nsecs >> 10) * (3 / 128) + 
	 *                                (nsecs >> 10) * (3 / 128) * (3 / 125)
	 *       ~ (nsecs >> 10) + (nsecs >> 10) * (3 / 128) + 
	 *                                (nsecs >> 10) * (3 / 128) * (3 / 128)
	 *       ~ (nsecs >> 10) + (((nsecs >> 10) * 3) >> 7) + 
	 *                                          (((nsecs >> 10) * 9) >> 14)
	 */
	nsecs = nsecs >> 10;
	usecs = nsecs + ((nsecs * 3) >> 7) + ((nsecs * 9) >> 14);
	if (!usecs) {
		usecs = 1;
	}

	omap3_gpt_load_start(BEAGLE_CLK_EVENT_GPT, usecs);
	return VMM_OK;
}

int vmm_cpu_clockevent_stop(void)
{
	omap3_gpt_stop(BEAGLE_CLK_EVENT_GPT);
	return VMM_OK;
}

int vmm_cpu_clockevent_expire(void)
{
	omap3_gpt_load_start(BEAGLE_CLK_EVENT_GPT, 1);

	/* No need to worry about irq-handler as irqs are disabled 
	 * before calling this */
	omap3_gpt_poll_overflow(BEAGLE_CLK_EVENT_GPT);

	return VMM_OK;
}

int vmm_cpu_clockevent_init(void)
{
	int rc = VMM_OK;

	omap3_gpt_global_init(sizeof(beagle_gpt_cfg)/sizeof(omap3_gpt_cfg_t), 
			beagle_gpt_cfg);

	rc = omap3_gpt_instance_init(BEAGLE_CLK_EVENT_GPT, OMAP3_GLOBAL_REG_PRM,
			&vmm_cpu_timer_irq_handler);
	if (rc) {
		return rc;
	}

	omap3_gpt_disable(BEAGLE_CLK_EVENT_GPT);
	omap3_gpt_oneshot(BEAGLE_CLK_EVENT_GPT);

	vmm_host_irq_enable(beagle_gpt_cfg[BEAGLE_CLK_EVENT_GPT].irq_no);

	return VMM_OK;
}

