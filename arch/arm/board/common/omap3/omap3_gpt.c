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
 * @file omap3_gpt.c
 * @version 1.0
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for OMAP3 general purpose timers
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <omap3/omap3_intc.h>
#include <omap3/omap3_gpt.h>

#define OMAP3_V_OSCK		26000000	/* Clock output from T2 */
#define OMAP3_V_SCLK		(OMAP3_V_OSCK >> 1)

#define OMAP3_SYS_TIMER_INCLK	(26000000 >> 1)
#define OMAP3_SYS_TIMER_BASE	(OMAP3_GPT1_BASE)
#define OMAP3_SYS_TIMER_IRQ	(OMAP3_MPU_INTC_GPT1_IRQ)
#define OMAP3_SYS_TIMER_PTV	2	/* Divisor: 2^(PTV+1) => 8 */
#define OMAP3_SYS_TIMER_CLK	(OMAP3_SYS_TIMER_INCLK/(2<<OMAP3_SYS_TIMER_PTV))

void omap3_gpt_write(u32 base, u32 reg, u32 val)
{
	vmm_writel(val, (void *)(base + reg));
}

u32 omap3_gpt_read(u32 base, u32 reg)
{
	return vmm_readl((void *)(base + reg));
}

int vmm_cpu_timer_irq_handler(u32 irq_no, vmm_user_regs_t * regs)
{
	vmm_timer_tick_process(regs, 1);

	omap3_gpt_write(OMAP3_SYS_TIMER_BASE,
			OMAP3_GPT_TISR, OMAP3_GPT_TISR_OVF_IT_FLAG_M);

	return VMM_OK;
}

int vmm_cpu_timer_setup(u32 tick_usecs)
{
	int ret;
	u32 regval;

	/* Register interrupt handler */
	ret = vmm_host_irq_register(OMAP3_SYS_TIMER_IRQ,
				    &vmm_cpu_timer_irq_handler);
	if (ret) {
		return ret;
	}

	/* Disable System Timer irq for sanity */
	vmm_host_irq_disable(OMAP3_SYS_TIMER_IRQ);

	/* Progamme System Timer */
	omap3_gpt_write(OMAP3_SYS_TIMER_BASE,
			OMAP3_GPT_TLDR,
			0xFFFFFFFF -
			tick_usecs * (OMAP3_SYS_TIMER_CLK / 1000000));
	regval = omap3_gpt_read(OMAP3_SYS_TIMER_BASE, OMAP3_GPT_TCLR);
	regval &= ~OMAP3_GPT_TCLR_ST_M;
	regval &= ~OMAP3_GPT_TCLR_PTV_M;
	regval |=
	    (OMAP3_SYS_TIMER_PTV << OMAP3_GPT_TCLR_PTV_S) &
	    OMAP3_GPT_TCLR_PTV_M;
	regval |= OMAP3_GPT_TCLR_AR_M;
	regval |= OMAP3_GPT_TCLR_PRE_M;
	omap3_gpt_write(OMAP3_SYS_TIMER_BASE, OMAP3_GPT_TCLR, regval);
	omap3_gpt_write(OMAP3_SYS_TIMER_BASE,
			OMAP3_GPT_TIER, OMAP3_GPT_TIER_OVF_IT_ENA_M);

	return VMM_OK;
}

void vmm_cpu_timer_enable(void)
{
	u32 regval;

	/* Enable System Timer irq */
	vmm_host_irq_enable(OMAP3_SYS_TIMER_IRQ);

	/* Enable System Timer */
	regval = omap3_gpt_read(OMAP3_SYS_TIMER_BASE, OMAP3_GPT_TCLR);
	omap3_gpt_write(OMAP3_SYS_TIMER_BASE,
			OMAP3_GPT_TCLR, regval | OMAP3_GPT_TCLR_ST_M);
}

void vmm_cpu_timer_disable(void)
{
	u32 regval;

	/* Disable System Timer irq */
	vmm_host_irq_disable(OMAP3_SYS_TIMER_IRQ);

	/* Disable System Timer */
	regval = omap3_gpt_read(OMAP3_SYS_TIMER_BASE, OMAP3_GPT_TCLR);
	omap3_gpt_write(OMAP3_SYS_TIMER_BASE,
			OMAP3_GPT_TCLR, regval & ~OMAP3_GPT_TCLR_ST_M);
}
