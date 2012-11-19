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
 * @brief source code for OMAP general purpose timers
 *
 * Parts of this source code has been taken from u-boot
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_host_io.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <vmm_host_aspace.h>
#include <omap/gpt.h>

static void gpt_write(virtual_addr_t base, u32 reg, u32 val)
{
	vmm_writel(val, (void *)(base + reg));
}

static u32 gpt_read(virtual_addr_t base, u32 reg)
{
	return vmm_readl((void *)(base + reg));
}

static void gpt_oneshot(virtual_addr_t base)
{
	u32 regval;
	/* Disable AR (auto-reload) */
	regval = gpt_read(base, GPT_TCLR);
	regval &= ~GPT_TCLR_AR_M;
	gpt_write(base, GPT_TCLR, regval);
	/* Enable Overflow Interrupt TIER[OVF_IT_ENA] */
	gpt_write(base, GPT_TIER, GPT_TIER_OVF_IT_ENA_M);
}

static void gpt_continuous(virtual_addr_t base)
{
	u32 regval;
	/* Enable AR (auto-reload) */
	regval = gpt_read(base, GPT_TCLR);
	regval |= GPT_TCLR_AR_M;
	gpt_write(base, GPT_TCLR, regval);
	/* Disable interrupts TIER[OVF_IT_ENA] */
	gpt_write(base, GPT_TIER, 0);
	/* Auto reload value set to 0 */
	gpt_write(base, GPT_TLDR, 0);
	gpt_write(base, GPT_TCRR, 0);
	/* Start Timer (TCLR[ST] = 1) */
	regval = gpt_read(base, GPT_TCLR);
	regval |= GPT_TCLR_ST_M;
	gpt_write(base, GPT_TCLR, regval);
}

struct gpt_clocksource {
	virtual_addr_t gpt_va;
	struct vmm_clocksource clksrc;
};

static u64 gpt_clocksource_read(struct vmm_clocksource *cs)
{
	struct gpt_clocksource *gpt_cs = cs->priv;
	return gpt_read(gpt_cs->gpt_va, GPT_TCRR);
}

int __init gpt_clocksource_init(const char *name, 
				physical_addr_t gpt_pa, u32 gpt_hz)
{
	struct gpt_clocksource *cs;

	cs = vmm_zalloc(sizeof(struct gpt_clocksource));
	if (!cs) {
		return VMM_EFAIL;
	}

	cs->gpt_va = vmm_host_iomap(gpt_pa, 0x1000);
	cs->clksrc.name = name;
	cs->clksrc.rating = 200;
	cs->clksrc.read = &gpt_clocksource_read;
	cs->clksrc.mask = 0xFFFFFFFF;
	cs->clksrc.mult = vmm_clocksource_khz2mult(gpt_hz/1000, 24);
	cs->clksrc.shift = 24;
	cs->clksrc.priv = cs;

	gpt_continuous(cs->gpt_va);

	return vmm_clocksource_register(&cs->clksrc);
}

struct gpt_clockchip {
	virtual_addr_t gpt_va;
	struct vmm_clockchip clkchip;
};

static vmm_irq_return_t gpt_clockevent_irq_handler(u32 irq_no, 
						arch_regs_t * regs, void *dev)
{
	u32 regval;
	struct gpt_clockchip *tcc = dev;

	gpt_write(tcc->gpt_va, GPT_TISR, GPT_TISR_OVF_IT_FLAG_M);

	/* Stop Timer (TCLR[ST] = 0) */
	regval = gpt_read(tcc->gpt_va, GPT_TCLR);
	regval &= ~GPT_TCLR_ST_M;
	gpt_write(tcc->gpt_va, GPT_TCLR, regval);

	tcc->clkchip.event_handler(&tcc->clkchip, regs);

	return VMM_IRQ_HANDLED;
}

static void gpt_clockchip_set_mode(enum vmm_clockchip_mode mode,
					 struct vmm_clockchip *cc)
{
	u32 regval;
	struct gpt_clockchip *tcc = cc->priv;

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_ONESHOT:
		gpt_oneshot(tcc->gpt_va);
		break;
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
		/* Stop Timer (TCLR[ST] = 0) */
		regval = gpt_read(tcc->gpt_va, GPT_TCLR);
		regval &= ~GPT_TCLR_ST_M;
		gpt_write(tcc->gpt_va, GPT_TCLR, regval);
		break;
	case VMM_CLOCKCHIP_MODE_PERIODIC:
	case VMM_CLOCKCHIP_MODE_UNUSED:
	default:
		break;
	}
}

static int gpt_clockchip_set_next_event(unsigned long next,
					      struct vmm_clockchip *cc)
{
	u32 regval;
	struct gpt_clockchip *tcc = cc->priv;

	gpt_write(tcc->gpt_va, GPT_TCRR, 0xFFFFFFFF - next);
	/* Start Timer (TCLR[ST] = 1) */
	regval = gpt_read(tcc->gpt_va, GPT_TCLR);
	regval |= GPT_TCLR_ST_M;
	gpt_write(tcc->gpt_va, GPT_TCLR, regval);

	return VMM_OK;
}

static int gpt_clockchip_expire(struct vmm_clockchip *cc)
{
	u32 regval;
	struct gpt_clockchip *tcc = cc->priv;

	gpt_write(tcc->gpt_va, GPT_TCRR, 0xFFFFFFFF - 1);
	/* Start Timer (TCLR[ST] = 1) */
	regval = gpt_read(tcc->gpt_va, GPT_TCLR);
	regval |= GPT_TCLR_ST_M;
	gpt_write(tcc->gpt_va, GPT_TCLR, regval);

	/* No need to worry about irq-handler as irqs are disabled 
	 * before polling for overflow */
	while(!(gpt_read(tcc->gpt_va, GPT_TISR) & 
				GPT_TISR_OVF_IT_FLAG_M));

	return VMM_OK;
}

int __init gpt_clockchip_init(const char *name,
			physical_addr_t gpt_pa, u32 gpt_hz, u32 gpt_irq)
{
	int rc;
	struct gpt_clockchip *cc;

	cc = vmm_zalloc(sizeof(struct gpt_clockchip));
	if (!cc) {
		return VMM_EFAIL;
	}

	cc->gpt_va = vmm_host_iomap(gpt_pa, 0x1000);
	cc->clkchip.name = name;
	cc->clkchip.hirq = gpt_irq;
	cc->clkchip.rating = 200;
	cc->clkchip.cpumask = cpu_all_mask;
	cc->clkchip.features = VMM_CLOCKCHIP_FEAT_ONESHOT;
	cc->clkchip.mult = vmm_clockchip_hz2mult(gpt_hz, 32);
	cc->clkchip.shift = 32;
	cc->clkchip.min_delta_ns = vmm_clockchip_delta2ns(0xF, &cc->clkchip);
	cc->clkchip.max_delta_ns = 
			vmm_clockchip_delta2ns(0xFFFFFFFF, &cc->clkchip);
	cc->clkchip.set_mode = &gpt_clockchip_set_mode;
	cc->clkchip.set_next_event = &gpt_clockchip_set_next_event;
	cc->clkchip.expire = &gpt_clockchip_expire;
	cc->clkchip.priv = cc;

	gpt_write(cc->gpt_va, GPT_TCLR, 0);

	/* Register interrupt handler */
	rc = vmm_host_irq_register(gpt_irq, name,
				   &gpt_clockevent_irq_handler, cc);
	if (rc) {
		return rc;
	}

	return vmm_clockchip_register(&cc->clkchip);
}

