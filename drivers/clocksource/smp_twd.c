/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file smp_twd.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief SMP Local Timer Implementation
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_percpu.h>
#include <vmm_devtree.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <drv/clk.h>
#include <drv/gic.h>

#define TWD_TIMER_LOAD			0x00
#define TWD_TIMER_COUNTER		0x04
#define TWD_TIMER_CONTROL		0x08
#define TWD_TIMER_INTSTAT		0x0C

#define TWD_WDOG_LOAD			0x20
#define TWD_WDOG_COUNTER		0x24
#define TWD_WDOG_CONTROL		0x28
#define TWD_WDOG_INTSTAT		0x2C
#define TWD_WDOG_RESETSTAT		0x30
#define TWD_WDOG_DISABLE		0x34

#define TWD_TIMER_CONTROL_ENABLE	(1 << 0)
#define TWD_TIMER_CONTROL_ONESHOT	(0 << 1)
#define TWD_TIMER_CONTROL_PERIODIC	(1 << 1)
#define TWD_TIMER_CONTROL_IT_ENABLE	(1 << 2)

struct twd_clockchip {
	char name[32];
	struct vmm_clockchip clkchip;
};

static DEFINE_PER_CPU(struct twd_clockchip, twd_cc);
static u32 twd_freq_hz;

static struct clk *twd_clk;
static virtual_addr_t twd_base;
static u32 twd_ppi_irq;

static vmm_irq_return_t twd_clockchip_irq_handler(int irq_no, void *dev)
{
	struct twd_clockchip *tcc = &this_cpu(twd_cc);

	if (vmm_readl((void *)(twd_base + TWD_TIMER_INTSTAT))) {
		vmm_writel(1, (void *)(twd_base + TWD_TIMER_INTSTAT));
	}

	tcc->clkchip.event_handler(&tcc->clkchip);

	return VMM_IRQ_HANDLED;
}

static void twd_clockchip_set_mode(enum vmm_clockchip_mode mode,
				   struct vmm_clockchip *cc)
{
	u32 ctrl;

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_PERIODIC:
		/* timer load already set up */
		ctrl = TWD_TIMER_CONTROL_ENABLE | TWD_TIMER_CONTROL_IT_ENABLE
			| TWD_TIMER_CONTROL_PERIODIC;
		vmm_writel(twd_freq_hz / 100, /* Assuming HZ = 100 */
			   (void *)(twd_base + TWD_TIMER_LOAD));
		break;
	case VMM_CLOCKCHIP_MODE_ONESHOT:
		/* period set, and timer enabled in 'next_event' hook */
		ctrl = TWD_TIMER_CONTROL_IT_ENABLE | TWD_TIMER_CONTROL_ONESHOT;
		break;
	case VMM_CLOCKCHIP_MODE_UNUSED:
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
	default:
		ctrl = 0;
		break;
	}

	vmm_writel(ctrl, (void *)(twd_base + TWD_TIMER_CONTROL));
}

static int twd_clockchip_set_next_event(unsigned long next,
					  struct vmm_clockchip *cc)
{
	u32 ctrl = vmm_readl((void *)(twd_base + TWD_TIMER_CONTROL));

	ctrl |= TWD_TIMER_CONTROL_ENABLE;

	vmm_writel(next, (void *)(twd_base + TWD_TIMER_COUNTER));
	vmm_writel(ctrl, (void *)(twd_base + TWD_TIMER_CONTROL));

	return 0;
}

static void __cpuinit twd_caliberate_freq(virtual_addr_t base, 
					  virtual_addr_t ref_counter_addr,
					  u32 ref_counter_freq)
{
	u32 i, count, ref_count;
	u64 tmp;

	/* enable, no interrupt or reload */
	vmm_writel(0x1, (void *)(base + TWD_TIMER_CONTROL));

	/* read reference counter */
	ref_count = vmm_readl((void *)ref_counter_addr);

	/* maximum value */
	vmm_writel(0xFFFFFFFFU, (void *)(base + TWD_TIMER_COUNTER));

	/* wait some arbitary amount of time */
	for (i = 0; i < 1000000; i++);

	/* read counter */
	count = vmm_readl((void *)(base + TWD_TIMER_COUNTER));
	count = 0xFFFFFFFFU - count;

	/* take reference counter difference */
	ref_count = vmm_readl((void *)ref_counter_addr) - ref_count;

	/* disable */
	vmm_writel(0x0, (void *)(base + TWD_TIMER_CONTROL));

	/* determine frequency */
	tmp = (u64)count * (u64)ref_counter_freq;
	twd_freq_hz = udiv64(tmp, ref_count);
}

static int __cpuinit twd_clockchip_init(struct vmm_devtree_node *node)
{
	int rc;
	u32 ref_cnt_freq;
	virtual_addr_t ref_cnt_addr;
	u32 cpu = vmm_smp_processor_id();
	struct twd_clockchip *cc = &this_cpu(twd_cc);

	if (!twd_base) {
		rc = vmm_devtree_regmap(node, &twd_base, 0);
		if (rc) {
			goto fail;
		}
	}

	if (!twd_ppi_irq) {
		rc = vmm_devtree_irq_get(node, &twd_ppi_irq, 0);
		if (rc) {
			goto fail_regunmap;
		}
	}

	if (!twd_freq_hz) {
		/* First try to find TWD clock */
		if (!twd_clk) {
			twd_clk = of_clk_get(node, 0);
		}
		if (!twd_clk) {
			twd_clk = clk_get_sys("smp_twd", NULL);
		}

		if (twd_clk) {
			/* Use TWD clock to find frequency */
			rc = clk_prepare_enable(twd_clk);
			if (rc) {
				clk_put(twd_clk);
				goto fail_regunmap;
			}
			twd_freq_hz = clk_get_rate(twd_clk);
		} else {
			/* No TWD clock found hence caliberate */
			rc = vmm_devtree_regmap(node, &ref_cnt_addr, 1);
			if (rc) {
				vmm_devtree_regunmap(node, ref_cnt_addr, 1);
				goto fail_regunmap;
			}
			if (vmm_devtree_read_u32(node, "ref-counter-freq",
						 &ref_cnt_freq)) {
				vmm_devtree_regunmap(node, ref_cnt_addr, 1);
				goto fail_regunmap;
			}
			twd_caliberate_freq(twd_base, 
					ref_cnt_addr, ref_cnt_freq);
			vmm_devtree_regunmap(node, ref_cnt_addr, 1);
		}
	}

	memset(cc, 0, sizeof(struct twd_clockchip));

	vmm_sprintf(cc->name, "twd/%d", cpu);

	cc->clkchip.name = cc->name;
	cc->clkchip.hirq = twd_ppi_irq;
	cc->clkchip.rating = 350;
	cc->clkchip.cpumask = vmm_cpumask_of(cpu);
	cc->clkchip.features = 
		VMM_CLOCKCHIP_FEAT_PERIODIC | VMM_CLOCKCHIP_FEAT_ONESHOT;
	vmm_clocks_calc_mult_shift(&cc->clkchip.mult, &cc->clkchip.shift, 
				   VMM_NSEC_PER_SEC, twd_freq_hz, 10);
	cc->clkchip.min_delta_ns = vmm_clockchip_delta2ns(0xF, &cc->clkchip);
	cc->clkchip.max_delta_ns = 
			vmm_clockchip_delta2ns(0xFFFFFFFF, &cc->clkchip);
	cc->clkchip.set_mode = &twd_clockchip_set_mode;
	cc->clkchip.set_next_event = &twd_clockchip_set_next_event;
	cc->clkchip.priv = cc;

	if (vmm_smp_is_bootcpu()) {
		/* Register interrupt handler */
		if ((rc = vmm_host_irq_register(twd_ppi_irq, "twd",
						&twd_clockchip_irq_handler, 
						cc))) {
			
			goto fail_regunmap;
		}

		/* Mark interrupt as per-cpu */
		if ((rc = vmm_host_irq_mark_per_cpu(twd_ppi_irq))) {
			goto fail_unreg_irq;
		}
	}

	/* Explicitly enable local timer PPI in GIC 
	 * Note: Local timer requires PPI support hence requires GIC
	 */
	gic_enable_ppi(twd_ppi_irq);

	rc = vmm_clockchip_register(&cc->clkchip);
	if (rc) {
		goto fail_unreg_irq;
	}

	return VMM_OK;

fail_unreg_irq:
	if (vmm_smp_is_bootcpu()) {
		vmm_host_irq_unregister(twd_ppi_irq, cc);
	}
fail_regunmap:
	vmm_devtree_regunmap(node, twd_base, 0);
fail:
	return rc;
}
VMM_CLOCKCHIP_INIT_DECLARE(ca9twd, "arm,cortex-a9-twd-timer", twd_clockchip_init);
VMM_CLOCKCHIP_INIT_DECLARE(ca5twd, "arm,cortex-a5-twd-timer", twd_clockchip_init);
VMM_CLOCKCHIP_INIT_DECLARE(arm11mptwd, "arm,arm11mp-twd-timer", twd_clockchip_init);

