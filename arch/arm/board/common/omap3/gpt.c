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

static omap3_gpt_cfg_t *omap3_gpt_config = NULL;
static int omap3_sys_clk_div = 0;

static void omap3_gpt_write(u32 gpt_num, u32 reg, u32 val)
{
	vmm_writel(val, (void *)(omap3_gpt_config[gpt_num].base_va + reg));
}

static u32 omap3_gpt_read(u32 gpt_num, u32 reg)
{
	return vmm_readl((void *)(omap3_gpt_config[gpt_num].base_va + reg));
}

void omap3_gpt_disable(u32 gpt_num)
{
	omap3_gpt_write(gpt_num, OMAP3_GPT_TCLR, 0);
}

u32 omap3_gpt_get_counter(u32 gpt_num)
{
	return omap3_gpt_read(gpt_num, OMAP3_GPT_TCRR);
}

void omap3_gpt_stop(u32 gpt_num)
{
	u32 regval;
	/* Stop Timer (TCLR[ST] = 0) */
	regval = omap3_gpt_read(gpt_num, OMAP3_GPT_TCLR);
	regval &= ~OMAP3_GPT_TCLR_ST_M;
	omap3_gpt_write(gpt_num, OMAP3_GPT_TCLR, regval);
}

void omap3_gpt_start(u32 gpt_num)
{
	u32 regval;
	/* Start Timer (TCLR[ST] = 1) */
	regval = omap3_gpt_read(gpt_num, OMAP3_GPT_TCLR);
	regval |= OMAP3_GPT_TCLR_ST_M;
	omap3_gpt_write(gpt_num, OMAP3_GPT_TCLR, regval);
}

void omap3_gpt_ack_irq(u32 gpt_num)
{
	omap3_gpt_write(gpt_num, OMAP3_GPT_TISR, OMAP3_GPT_TISR_OVF_IT_FLAG_M);
}

void omap3_gpt_poll_overflow(u32 gpt_num)
{
	while(!(omap3_gpt_read(gpt_num, OMAP3_GPT_TISR) & 
				OMAP3_GPT_TISR_OVF_IT_FLAG_M));
}

void omap3_gpt_load_start(u32 gpt_num, u32 usecs)
{
	u32 freq = (omap3_gpt_config[gpt_num].clk_hz);
	/* Number of clocks */
	u32 clocks = (usecs * freq) / 1000000;
	/* Reset the counter */
	omap3_gpt_write(gpt_num, OMAP3_GPT_TCRR, 0xFFFFFFFF - clocks);
	omap3_gpt_start(gpt_num);
}

void omap3_gpt_oneshot(u32 gpt_num)
{
	u32 regval;
	/* Disable AR (auto-reload) */
	regval = omap3_gpt_read(gpt_num, OMAP3_GPT_TCLR);
	regval &= ~OMAP3_GPT_TCLR_AR_M;
	omap3_gpt_write(gpt_num, OMAP3_GPT_TCLR, regval);
	/* Enable Overflow Interrupt TIER[OVF_IT_ENA] */
	omap3_gpt_write(gpt_num, OMAP3_GPT_TIER, OMAP3_GPT_TIER_OVF_IT_ENA_M);
}

void omap3_gpt_continuous(u32 gpt_num)
{
	u32 regval;
	/* Enable AR (auto-reload) */
	regval = omap3_gpt_read(gpt_num, OMAP3_GPT_TCLR);
	regval |= OMAP3_GPT_TCLR_AR_M;
	omap3_gpt_write(gpt_num, OMAP3_GPT_TCLR, regval);
	/* Disable interrupts TIER[OVF_IT_ENA] */
	omap3_gpt_write(gpt_num, OMAP3_GPT_TIER, 0);
	/* Auto reload value set to 0 */
	omap3_gpt_write(gpt_num, OMAP3_GPT_TLDR, 0);
	omap3_gpt_write(gpt_num, OMAP3_GPT_TCRR, 0);
	omap3_gpt_start(gpt_num);
}

u32 omap3_gpt_get_clk_speed(u32 gpt_num)
{
	u32 start, cstart, cend, cdiff, val, omap3_osc_clk_hz = 0;

	/* Start counting at 0 */
	omap3_gpt_write(gpt_num, OMAP3_GPT_TLDR, 0);

	/* Enable GPT */
	omap3_gpt_write(gpt_num, OMAP3_GPT_TCLR, OMAP3_GPT_TCLR_ST_M);

	/* enable 32kHz source, determine sys_clk via gauging */
	omap3_s32k_init();

	/* start time in 20 cycles */
	start = 20 + omap3_s32k_get_counter();

	/* dead loop till start time */
	while (omap3_s32k_get_counter() < start);

	/* get start sys_clk count */
	cstart = omap3_gpt_read(gpt_num, OMAP3_GPT_TCRR);

	/* wait for 40 cycles */
	while (omap3_s32k_get_counter() < (start + 20)) ;
	cend = omap3_gpt_read(gpt_num, OMAP3_GPT_TCRR);	/* get end sys_clk count */
	cdiff = cend - cstart;			/* get elapsed ticks */
	cdiff *= omap3_sys_clk_div;

	omap3_gpt_stop(gpt_num);

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

	val = omap3_osc_clk_hz >> (omap3_sys_clk_div -1);
	return(val);
}

void omap3_gpt_clock_enable(u32 gpt_num)
{
	u32 val;
	/* select clock source (1=sys_clk; 0=32K) for GPT */
	if(omap3_gpt_config[gpt_num].src_sys_clk) {
		val = vmm_readl((void *)(omap3_gpt_config[gpt_num].cm_va +
				  	OMAP3_CM_CLKSEL)) 
			| omap3_gpt_config[gpt_num].clksel_mask;
		omap3_gpt_config[gpt_num].clk_hz = omap3_gpt_get_clk_speed(gpt_num);
	} else {
		val = vmm_readl((void *)(omap3_gpt_config[gpt_num].cm_va +
				  	OMAP3_CM_CLKSEL)) 
			& ~(omap3_gpt_config[gpt_num].clksel_mask);
		omap3_gpt_config[gpt_num].clk_hz = OMAP3_S32K_FREQ_HZ;
	}
	vmm_writel(val, (void *)(omap3_gpt_config[gpt_num].cm_va + 
				OMAP3_CM_CLKSEL));

	/* Enable I Clock for GPT */
	val = vmm_readl((void *)(omap3_gpt_config[gpt_num].cm_va + OMAP3_CM_ICLKEN)) 
		| omap3_gpt_config[gpt_num].iclken_mask; 
	vmm_writel(val, (void *)(omap3_gpt_config[gpt_num].cm_va + OMAP3_CM_ICLKEN));

	/* Enable F Clock for GPT */
	val = vmm_readl((void *)(omap3_gpt_config[gpt_num].cm_va + OMAP3_CM_FCLKEN)) 
		| omap3_gpt_config[gpt_num].fclken_mask;
	vmm_writel(val, (void *)(omap3_gpt_config[gpt_num].cm_va + OMAP3_CM_FCLKEN));
}

int omap3_gpt_instance_init(u32 gpt_num, physical_addr_t prm_pa, 
		vmm_host_irq_handler_t irq_handler)
{
	int ret;
	u32 val;
	virtual_addr_t glbl_prm_base = vmm_host_iomap(prm_pa, 0x100);

	/* Determine system clock divider */
	val = vmm_readl((void *)(glbl_prm_base + OMAP3_PRM_CLKSRC_CTRL));
	omap3_sys_clk_div = (val & OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_M) 
		>> OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_S;
	
	vmm_host_iounmap(glbl_prm_base, 0x100);

	/* Enable clock */
	omap3_gpt_clock_enable(gpt_num);

#if defined(CONFIG_VERBOSE_MODE)
	vmm_printf("GPT%d (base: 0x%08X) running @ %d Hz\n", gpt_num+1, 
			omap3_gpt_config[gpt_num].base_va, 
			omap3_gpt_config[gpt_num].clk_hz);
#endif

	/* Register interrupt handler */
	if(irq_handler) {
		ret = vmm_host_irq_register(omap3_gpt_config[gpt_num].irq_no, irq_handler, NULL);
		if (ret) {
			return ret;
		}
	}

	/* Disable System Timer irq for sanity */
	vmm_host_irq_disable(omap3_gpt_config[gpt_num].irq_no);

	return VMM_OK;
}

int omap3_gpt_global_init(u32 gpt_count, omap3_gpt_cfg_t *cfg)
{
	int i;
	if(!omap3_gpt_config) {
		omap3_gpt_config = cfg;
		for(i=0; i<gpt_count; i++) {
			omap3_gpt_config[i].base_va = vmm_host_iomap(omap3_gpt_config[i].base_pa, 0x1000);
			omap3_gpt_config[i].cm_va = vmm_host_iomap(omap3_gpt_config[i].cm_pa, 0x100);
			if(!omap3_gpt_config[i].base_va || !omap3_gpt_config[i].cm_va)
				return VMM_EFAIL;
		}
	}
	return VMM_OK;
}

