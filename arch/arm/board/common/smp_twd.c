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
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <gic.h>
#include <smp_twd.h>

struct twd_clockchip {
	char name[32];
	struct vmm_clockchip clkchip;
};

static DEFINE_PER_CPU(struct twd_clockchip, twd_cc);
static u32 twd_freq_hz;

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

static void twd_caliberate_freq(virtual_addr_t base, 
				virtual_addr_t ref_counter_addr,
				u32 ref_counter_freq)
{
	u32 i, count, ref_count;
	u64 tmp;

	/* Do caliberation only once */
	if (!twd_freq_hz) {
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
}

const static struct vmm_devtree_nodeid twd_match[] = {
	{ .compatible = "arm,cortex-a9-twd-timer",	},
	{ .compatible = "arm,cortex-a5-twd-timer",	},
	{ .compatible = "arm,arm11mp-twd-timer",	},
	{ },
};

int __cpuinit twd_clockchip_init(virtual_addr_t ref_counter_addr,
				 u32 ref_counter_freq)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();
	struct vmm_devtree_node *node;
	struct twd_clockchip *cc = &this_cpu(twd_cc);

	node = vmm_devtree_find_matching(NULL, twd_match);
	if (!node) {
		return VMM_ENODEV;
	}

	if (!twd_base) {
		rc = vmm_devtree_regmap(node, &twd_base, 0);
		if (rc) {
			return rc;
		}
	}

	if (!twd_ppi_irq) {
		rc = vmm_devtree_irq_get(node, &twd_ppi_irq, 0);
		if (rc) {
			return rc;
		}
	}

	twd_caliberate_freq(twd_base, ref_counter_addr, ref_counter_freq);

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

	if (!cpu) {
		/* Register interrupt handler */
		if ((rc = vmm_host_irq_register(twd_ppi_irq, "twd",
						&twd_clockchip_irq_handler, 
						cc))) {
			return rc;
		}

		/* Mark interrupt as per-cpu */
		if ((rc = vmm_host_irq_mark_per_cpu(twd_ppi_irq))) {
			return rc;
		}
	}

	/* Explicitly enable local timer PPI in GIC 
	 * Note: Local timer requires PPI support hence requires GIC
	 */
	gic_enable_ppi(twd_ppi_irq);

	return vmm_clockchip_register(&cc->clkchip);
}

