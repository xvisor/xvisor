/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
 * Copyright (c) 2011 Sukanto Ghosh.
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
 * @file gpt.c
 * @version 1.0
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source code for OMAP3 general purpose timers
 *
 * Parts of this source code has been taken from u-boot
 */

#include <vmm_error.h>
#include <vmm_math.h>
#include <vmm_main.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <omap3/intc.h>
#include <omap3/gpt.h>
#include <omap3/prcm.h>
#include <omap3/s32k-timer.h>

static virtual_addr_t omap35x_gpt1_base = 0;	/* used for clockevent */
static u32 omap3_osc_clk_hz = 0;
static u32 omap3_gpt1_clk_hz = 0;

static void omap3_gpt_write(u32 reg, u32 val)
{
	vmm_writel(val, (void *)(omap35x_gpt1_base + reg));
}

static u32 omap3_gpt_read(u32 reg)
{
	return vmm_readl((void *)(omap35x_gpt1_base + reg));
}

void omap3_gpt_disable(void)
{
	omap3_gpt_write(OMAP3_GPT_TCLR, 0);
}

void omap3_gpt_stop(void)
{
	u32 regval;
	/* Stop Timer (TCLR[ST] = 0) */
	regval = omap3_gpt_read(OMAP3_GPT_TCLR);
	regval &= ~OMAP3_GPT_TCLR_ST_M;
	omap3_gpt_write(OMAP3_GPT_TCLR, regval);
}

void omap3_gpt_start(void)
{
	u32 regval;
	/* Start Timer (TCLR[ST] = 1) */
	regval = omap3_gpt_read(OMAP3_GPT_TCLR);
	regval |= OMAP3_GPT_TCLR_ST_M;
	omap3_gpt_write(OMAP3_GPT_TCLR, regval);
}

void omap3_gpt_ack_irq(void)
{
	omap3_gpt_write(OMAP3_GPT_TISR, OMAP3_GPT_TISR_OVF_IT_FLAG_M);
}

void omap3_gpt_load_start(u32 usecs)
{
	/* Number of clocks */
	u32 clocks = (usecs * omap3_gpt1_clk_hz) / 1000000;
	/* Reset the counter */
	omap3_gpt_write(OMAP3_GPT_TCRR, 0xFFFFFFFF - clocks);
	omap3_gpt_start();
}

void omap3_gpt_oneshot(void)
{
	u32 regval;
	/* Disable AR (auto-reload) */
	regval = omap3_gpt_read(OMAP3_GPT_TCLR);
	regval &= ~OMAP3_GPT_TCLR_AR_M;
	omap3_gpt_write(OMAP3_GPT_TCLR, regval);
	/* Enable Overflow Interrupt TIER[OVF_IT_ENA] */
	omap3_gpt_write(OMAP3_GPT_TIER, OMAP3_GPT_TIER_OVF_IT_ENA_M);
}

/******************************************************************************
 * omap3_osc_clk_speed() - determine reference oscillator speed
 *                       based on known 32kHz clock and gptimer.
 *****************************************************************************/
void omap3_osc_clk_speed(virtual_addr_t wkup_cm_base, virtual_addr_t glbl_prm_base)
{
	u32 start, cstart, cend, cdiff, cdiv, val;

	/* Determine system clock divider */
	val = vmm_readl((void *)(glbl_prm_base + OMAP3_PRM_CLKSRC_CTRL));
	cdiv = (val & OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_M) 
		>> OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_S;

	/* select sys_clk for GPT1 */
	val = vmm_readl((void *)(wkup_cm_base +  OMAP3_CM_CLKSEL_WKUP)) 
		| OMAP3_CM_CLKSEL_WKUP_CLKSEL_GPT1_M;
	vmm_writel(val, (void *)(wkup_cm_base + OMAP3_CM_CLKSEL_WKUP));

	/* Enable I and F Clocks for GPT1 */
	val = vmm_readl((void *)(wkup_cm_base + OMAP3_CM_ICLKEN_WKUP)) 
		| OMAP3_CM_ICLKEN_WKUP_EN_GPT1_M; 
	vmm_writel(val, (void *)(wkup_cm_base + OMAP3_CM_ICLKEN_WKUP));

	val = vmm_readl((void *)(wkup_cm_base + OMAP3_CM_FCLKEN_WKUP)) 
		| OMAP3_CM_FCLKEN_WKUP_EN_GPT1_M;
	vmm_writel(val, (void *)(wkup_cm_base + OMAP3_CM_FCLKEN_WKUP));

	/* Start counting at 0 */
	omap3_gpt_write(OMAP3_GPT_TLDR, 0);

	/* Enable GPT */
	omap3_gpt_write(OMAP3_GPT_TCLR, OMAP3_GPT_TCLR_ST_M);

	/* enable 32kHz source, determine sys_clk via gauging */
	omap3_s32k_init();

	/* start time in 20 cycles */
	start = 20 + omap3_s32k_get_counter();

	/* dead loop till start time */
	while (omap3_s32k_get_counter() < start);

	/* get start sys_clk count */
	cstart = omap3_gpt_read(OMAP3_GPT_TCRR);

	/* wait for 40 cycles */
	while (omap3_s32k_get_counter() < (start + 20)) ;
	cend = omap3_gpt_read(OMAP3_GPT_TCRR);	/* get end sys_clk count */
	cdiff = cend - cstart;			/* get elapsed ticks */
	cdiff *= cdiv;

	omap3_gpt_stop();

	/* based on number of ticks assign speed */
	if (cdiff > 19000)
		omap3_osc_clk_hz = OMAP3_SYSCLK_S38_4M;
	else if (cdiff > 15200)
		omap3_osc_clk_hz = OMAP3_SYSCLK_S26M;
	else if (cdiff > 13000)
		omap3_osc_clk_hz = OMAP3_SYSCLK_S24M;
	else if (cdiff > 9000)
		omap3_osc_clk_hz = OMAP3_SYSCLK_S19_2M;
	else if (cdiff > 7600)
		omap3_osc_clk_hz = OMAP3_SYSCLK_S13M;
	else
		omap3_osc_clk_hz = OMAP3_SYSCLK_S12M;

	omap3_gpt1_clk_hz = omap3_osc_clk_hz >> (cdiv-1);
#if defined(CONFIG_VERBOSE_MODE)
	vmm_printf("Clockevent-generator [GPT1] running @ %d Hz\n",
			omap3_gpt1_clk_hz);
#endif
}

int omap3_gpt_init(physical_addr_t gpt_pa, physical_addr_t cm_pa, 
		physical_addr_t prm_pa, u32 gpt_irq_no, 
		vmm_host_irq_handler_t irq_handler)
{
	int ret;
	virtual_addr_t wkup_cm_base = vmm_host_iomap(cm_pa, 0x100);
	virtual_addr_t glbl_prm_base = vmm_host_iomap(prm_pa, 0x100);

	/* Map timer registers */
	if(!omap35x_gpt1_base)
		omap35x_gpt1_base = vmm_host_iomap(gpt_pa, 0x1000);
	
	/* Determing OSC-CLK speed based on S32K timer */
	omap3_osc_clk_speed(wkup_cm_base, glbl_prm_base);

	/* Register interrupt handler */
	ret = vmm_host_irq_register(gpt_irq_no, irq_handler, NULL);
	if (ret) {
		return ret;
	}

	/* Disable System Timer irq for sanity */
	vmm_host_irq_disable(gpt_irq_no);

	return VMM_OK;
}
