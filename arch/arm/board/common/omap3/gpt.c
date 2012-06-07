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
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source code for OMAP3 general purpose timers
 *
 * Parts of this source code has been taken from u-boot
 */

#include <omap3/intc.h>
#include <omap3/gpt.h>
#include <omap3/prcm.h>
#include <omap3/s32k-timer.h>
#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_host_io.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <vmm_host_aspace.h>

static struct omap3_gpt_cfg *omap3_gpt_config = NULL;
static int omap3_sys_clk_div = 0;

static void omap3_gpt_write(u32 gpt_num, u32 reg, u32 val)
{
	vmm_writel(val, (void *)(omap3_gpt_config[gpt_num].base_va + reg));
}

static u32 omap3_gpt_read(u32 gpt_num, u32 reg)
{
	return vmm_readl((void *)(omap3_gpt_config[gpt_num].base_va + reg));
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
	/* Start Timer (TCLR[ST] = 1) */
	regval = omap3_gpt_read(gpt_num, OMAP3_GPT_TCLR);
	regval |= OMAP3_GPT_TCLR_ST_M;
	omap3_gpt_write(gpt_num, OMAP3_GPT_TCLR, regval);
}

u32 omap3_gpt_get_clk_speed(u32 gpt_num)
{
	u32 omap3_osc_clk_hz = 0, val, regval;
	u32 start, cstart, cend, cdiff;

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

	/* Stop Timer (TCLR[ST] = 0) */
	regval = omap3_gpt_read(gpt_num, OMAP3_GPT_TCLR);
	regval &= ~OMAP3_GPT_TCLR_ST_M;
	omap3_gpt_write(gpt_num, OMAP3_GPT_TCLR, regval);

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
	/* select clock source (1=sys_clk; 0=32K) for GPT */
	if(omap3_gpt_config[gpt_num].src_sys_clk) {
		omap3_cm_setbits(omap3_gpt_config[gpt_num].cm_domain, 
			OMAP3_CM_CLKSEL, omap3_gpt_config[gpt_num].clksel_mask);
		omap3_gpt_config[gpt_num].clk_hz = omap3_gpt_get_clk_speed(gpt_num);
	} else {
		omap3_cm_clrbits(omap3_gpt_config[gpt_num].cm_domain, 
			OMAP3_CM_CLKSEL, omap3_gpt_config[gpt_num].clksel_mask);
		omap3_gpt_config[gpt_num].clk_hz = OMAP3_S32K_FREQ_HZ;
	}

	/* Enable I Clock for GPT */
	omap3_cm_setbits(omap3_gpt_config[gpt_num].cm_domain, 
			OMAP3_CM_ICLKEN, omap3_gpt_config[gpt_num].iclken_mask);

	/* Enable F Clock for GPT */
	omap3_cm_setbits(omap3_gpt_config[gpt_num].cm_domain, 
			OMAP3_CM_FCLKEN, omap3_gpt_config[gpt_num].fclken_mask);
}

int omap3_gpt_instance_init(u32 gpt_num, u32 prm_domain, 
		vmm_host_irq_handler_t irq_handler)
{
	u32 val;

	/* Determine system clock divider */
	val = omap3_prm_read(prm_domain, OMAP3_PRM_CLKSRC_CTRL);
	omap3_sys_clk_div = (val & OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_M) 
		>> OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_S;
	
	/* Enable clock */
	omap3_gpt_clock_enable(gpt_num);

#if defined(CONFIG_VERBOSE_MODE)
	vmm_printf("GPT%d (base: 0x%08X) running @ %d Hz\n", gpt_num+1, 
			omap3_gpt_config[gpt_num].base_va, 
			omap3_gpt_config[gpt_num].clk_hz);
#endif

	return VMM_OK;
}

struct omap3_gpt_clocksource 
{
	u32 gpt_num;
	struct vmm_clocksource clksrc;
};

static u64 omap3_gpt_clocksource_read(struct vmm_clocksource *cs)
{
	struct omap3_gpt_clocksource * omap3_cs = cs->priv;
	return omap3_gpt_read(omap3_cs->gpt_num, OMAP3_GPT_TCRR);
}

int __init omap3_gpt_clocksource_init(u32 gpt_num, physical_addr_t prm_pa)
{
	int rc;
	struct omap3_gpt_clocksource *cs;

	if ((rc = omap3_gpt_instance_init(gpt_num, prm_pa, NULL))) {
		return rc;
	}

	omap3_gpt_continuous(gpt_num);

	cs = vmm_malloc(sizeof(struct omap3_gpt_clocksource));
	if (!cs) {
		return VMM_EFAIL;
	}

	cs->gpt_num = gpt_num;
	cs->clksrc.name = omap3_gpt_config[gpt_num].name;
	cs->clksrc.rating = 200;
	cs->clksrc.read = &omap3_gpt_clocksource_read;
	cs->clksrc.mask = 0xFFFFFFFF;
	cs->clksrc.mult = 
	vmm_clocksource_khz2mult((omap3_gpt_config[gpt_num].clk_hz)/1000, 24);
	cs->clksrc.shift = 24;
	cs->clksrc.priv = cs;

	return vmm_clocksource_register(&cs->clksrc);
}

struct omap3_gpt_clockchip 
{
	u32 gpt_num;
	struct vmm_clockchip clkchip;
};

static vmm_irq_return_t omap3_gpt_clockevent_irq_handler(u32 irq_no, 
							 arch_regs_t * regs, 
							 void *dev)
{
	u32 regval;
	struct omap3_gpt_clockchip *tcc = dev;

	omap3_gpt_write(tcc->gpt_num, 
			OMAP3_GPT_TISR, 
			OMAP3_GPT_TISR_OVF_IT_FLAG_M);

	/* Stop Timer (TCLR[ST] = 0) */
	regval = omap3_gpt_read(tcc->gpt_num, OMAP3_GPT_TCLR);
	regval &= ~OMAP3_GPT_TCLR_ST_M;
	omap3_gpt_write(tcc->gpt_num, OMAP3_GPT_TCLR, regval);

	tcc->clkchip.event_handler(&tcc->clkchip, regs);

	return VMM_IRQ_HANDLED;
}

static void omap3_gpt_clockchip_set_mode(enum vmm_clockchip_mode mode,
					 struct vmm_clockchip *cc)
{
	u32 regval;
	struct omap3_gpt_clockchip *tcc = cc->priv;

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_ONESHOT:
		omap3_gpt_oneshot(tcc->gpt_num);
		break;
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
		/* Stop Timer (TCLR[ST] = 0) */
		regval = omap3_gpt_read(tcc->gpt_num, OMAP3_GPT_TCLR);
		regval &= ~OMAP3_GPT_TCLR_ST_M;
		omap3_gpt_write(tcc->gpt_num, OMAP3_GPT_TCLR, regval);
		break;
	case VMM_CLOCKCHIP_MODE_PERIODIC:
	case VMM_CLOCKCHIP_MODE_UNUSED:
	default:
		break;
	}
}

static int omap3_gpt_clockchip_set_next_event(unsigned long next,
					      struct vmm_clockchip *cc)
{
	u32 regval;
	struct omap3_gpt_clockchip *tcc = cc->priv;

	omap3_gpt_write(tcc->gpt_num, OMAP3_GPT_TCRR, 0xFFFFFFFF - next);
	/* Start Timer (TCLR[ST] = 1) */
	regval = omap3_gpt_read(tcc->gpt_num, OMAP3_GPT_TCLR);
	regval |= OMAP3_GPT_TCLR_ST_M;
	omap3_gpt_write(tcc->gpt_num, OMAP3_GPT_TCLR, regval);

	return VMM_OK;
}

static int omap3_gpt_clockchip_expire(struct vmm_clockchip *cc)
{
	u32 regval;
	struct omap3_gpt_clockchip *tcc = cc->priv;

	omap3_gpt_write(tcc->gpt_num, OMAP3_GPT_TCRR, 0xFFFFFFFF - 1);
	/* Start Timer (TCLR[ST] = 1) */
	regval = omap3_gpt_read(tcc->gpt_num, OMAP3_GPT_TCLR);
	regval |= OMAP3_GPT_TCLR_ST_M;
	omap3_gpt_write(tcc->gpt_num, OMAP3_GPT_TCLR, regval);

	/* No need to worry about irq-handler as irqs are disabled 
	 * before polling for overflow */
	while(!(omap3_gpt_read(tcc->gpt_num, OMAP3_GPT_TISR) & 
				OMAP3_GPT_TISR_OVF_IT_FLAG_M));

	return VMM_OK;
}

int __init omap3_gpt_clockchip_init(u32 gpt_num, physical_addr_t prm_pa)
{
	int rc;
	struct omap3_gpt_clockchip *cc;

	if ((rc = omap3_gpt_instance_init(gpt_num, prm_pa, NULL))) {
		return rc;
	}

	omap3_gpt_write(gpt_num, OMAP3_GPT_TCLR, 0);

	cc = vmm_malloc(sizeof(struct omap3_gpt_clockchip));
	if (!cc) {
		return VMM_EFAIL;
	}

	cc->gpt_num = gpt_num;
	cc->clkchip.name = omap3_gpt_config[gpt_num].name;
	cc->clkchip.hirq = omap3_gpt_config[gpt_num].irq_no;
	cc->clkchip.rating = 200;
	cc->clkchip.cpumask = cpu_all_mask;
	cc->clkchip.features = VMM_CLOCKCHIP_FEAT_ONESHOT;
	cc->clkchip.mult = 
		vmm_clockchip_hz2mult(omap3_gpt_config[gpt_num].clk_hz, 32);
	cc->clkchip.shift = 32;
	cc->clkchip.min_delta_ns = vmm_clockchip_delta2ns(0xF, &cc->clkchip);
	cc->clkchip.max_delta_ns = 
			vmm_clockchip_delta2ns(0xFFFFFFFF, &cc->clkchip);
	cc->clkchip.set_mode = &omap3_gpt_clockchip_set_mode;
	cc->clkchip.set_next_event = &omap3_gpt_clockchip_set_next_event;
	cc->clkchip.expire = &omap3_gpt_clockchip_expire;
	cc->clkchip.priv = cc;

	/* Disable GPT irq for sanity */
	vmm_host_irq_disable(omap3_gpt_config[gpt_num].irq_no);

	/* Register interrupt handler */
	rc = vmm_host_irq_register(omap3_gpt_config[gpt_num].irq_no,
				   omap3_gpt_config[gpt_num].name,
				   &omap3_gpt_clockevent_irq_handler, cc);
	if (rc) {
		return rc;
	}

	/* Enabled GPT irq */
	rc = vmm_host_irq_enable(omap3_gpt_config[gpt_num].irq_no);
	if (rc) {
		return rc;
	}

	return vmm_clockchip_register(&cc->clkchip);
}

int __init omap3_gpt_global_init(u32 gpt_count, struct omap3_gpt_cfg *cfg)
{
	int i;
	if(!omap3_gpt_config) {
		omap3_gpt_config = cfg;
		for(i=0; i<gpt_count; i++) {
			omap3_gpt_config[i].base_va = 
			vmm_host_iomap(omap3_gpt_config[i].base_pa, 0x1000);
			if(!omap3_gpt_config[i].base_va)
				return VMM_EFAIL;
		}
	}
	return VMM_OK;
}

