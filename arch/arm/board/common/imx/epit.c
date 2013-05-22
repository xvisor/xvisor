/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
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
 * @file epit.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source file for EPIT timer support.
 *
 *  Based on linux/arch/arm/plat-mxc/epit.c
 *
 *  Copyright (C) 2010 Sascha Hauer <s.hauer@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <vmm_types.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_smp.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <vmm_wallclock.h>
#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_delay.h>

#include <linux/clk.h>

#define EPITCR				(0x00)
#define EPITSR				(0x04)
#define EPITLR				(0x08)
#define EPITCMPR			(0x0c)
#define EPITCNR				(0x10)

#define EPITCR_EN			(1 << 0)
#define EPITCR_ENMOD			(1 << 1)
#define EPITCR_OCIEN			(1 << 2)
#define EPITCR_RLD			(1 << 3)
#define EPITCR_PRESC(x)			(((x) & 0xfff) << 4)
#define EPITCR_SWR			(1 << 16)
#define EPITCR_IOVW			(1 << 17)
#define EPITCR_DBGEN			(1 << 18)
#define EPITCR_WAITEN			(1 << 19)
#define EPITCR_RES			(1 << 20)
#define EPITCR_STOPEN			(1 << 21)
#define EPITCR_OM_DISCON		(0 << 22)
#define EPITCR_OM_TOGGLE		(1 << 22)
#define EPITCR_OM_CLEAR			(2 << 22)
#define EPITCR_OM_SET			(3 << 22)
#define EPITCR_CLKSRC_OFF		(0 << 24)
#define EPITCR_CLKSRC_PERIPHERAL	(1 << 24)
#define EPITCR_CLKSRC_REF_HIGH		(1 << 24)
#define EPITCR_CLKSRC_REF_LOW		(3 << 24)

#define EPITSR_OCIF			(1 << 0)

#define MIN_REG_COMPARE			(0x800)
#define MAX_REG_COMPARE			(0xfffffffe)

struct epit_clocksource {
	u32 cnt_high;
	u32 cnt_low;
	virtual_addr_t base;
	struct vmm_clocksource clksrc;
};

static u64 epit_clksrc_read(struct vmm_clocksource *cs)
{
	u32 temp;
	struct epit_clocksource *ecs = cs->priv;

	/*
	 * Get the current count. As the timer is decrementing we 
	 * invert the result.
	 */
	temp = ~vmm_readl((void *)(ecs->base + EPITCNR));

	/*
	 * if the timer wrapped around we increase the high 32 bits part
	 * Note: the clock source is read fairly often and therefore it
	 *       should not be possible for the 32 bits counter to wrap
	 *       arround several time between 2 reads.
	 */
	if (temp < ecs->cnt_low) {
		ecs->cnt_high++;
	}

	ecs->cnt_low = temp;

	/*
	 * We can combine the two 32 bits couters to make a 64 bits
	 * counter.
	 */
	return (((u64) ecs->cnt_high) << 32) | ecs->cnt_low;
}

int __init epit_clocksource_init(void)
{
	int rc = VMM_ENODEV;
	u32 clock;
	struct vmm_devtree_node *node;
	struct epit_clocksource *ecs;

	/* find a epit compatible node */
	node = vmm_devtree_find_compatible(NULL, NULL, "freescale,epit-timer");
	if (!node) {
		goto fail;
	}

	/* Read clock frequency from node */
	rc = vmm_devtree_clock_frequency(node, &clock);
	if (rc) {
		goto fail;
	}

	/* allocate our struct */
	ecs = vmm_zalloc(sizeof(struct epit_clocksource));
	if (!ecs) {
		rc = VMM_ENOMEM;
		goto fail;
	}

	/* Map timer registers */
	rc = vmm_devtree_regmap(node, &ecs->base, 0);
	if (rc) {
		goto regmap_fail;
	}

	/* Setup clocksource */
	ecs->clksrc.name = node->name;
	ecs->clksrc.rating = 300;
	ecs->clksrc.read = epit_clksrc_read;
	ecs->clksrc.mask = VMM_CLOCKSOURCE_MASK(32);
	vmm_clocks_calc_mult_shift(&ecs->clksrc.mult,
				   &ecs->clksrc.shift,
				   clock, VMM_NSEC_PER_SEC, 10);
	ecs->clksrc.priv = ecs;

	/* Register clocksource */
	rc = vmm_clocksource_register(&ecs->clksrc);
	if (rc) {
		goto register_fail;
	}

	return VMM_OK;

 register_fail:
	vmm_devtree_regunmap(node, ecs->base, 0);
 regmap_fail:
	vmm_free(ecs);
 fail:
	return rc;
}

struct epit_clockchip {
	u32 match_mask;
	u32 timer_num;
	enum vmm_clockchip_mode clockevent_mode;
	virtual_addr_t base;
	struct vmm_clockchip clkchip;
};

static inline void epit_irq_disable(struct epit_clockchip *ecc)
{
	u32 val;

	val = vmm_readl((void *)(ecc->base + EPITCR));
	val &= ~EPITCR_OCIEN;
	vmm_writel(val, (void *)(ecc->base + EPITCR));
}

static inline void epit_irq_enable(struct epit_clockchip *ecc)
{
	u32 val;

	val = vmm_readl((void *)(ecc->base + EPITCR));
	val |= EPITCR_OCIEN;
	vmm_writel(val, (void *)(ecc->base + EPITCR));
}

static void epit_irq_acknowledge(struct epit_clockchip *ecc)
{
	vmm_writel(EPITSR_OCIF, (void *)(ecc->base + EPITSR));
}

static int epit_set_next_event(unsigned long cycles, struct vmm_clockchip *evt)
{
	struct epit_clockchip *ecc = evt->priv;
	unsigned long tcmp;

	tcmp = vmm_readl((void *)(ecc->base + EPITCNR));

	vmm_writel(tcmp - cycles, (void *)(ecc->base + EPITCMPR));

	return VMM_OK;
}

static void epit_set_mode(enum vmm_clockchip_mode mode,
			  struct vmm_clockchip *evt)
{
	struct epit_clockchip *ecc = evt->priv;
	unsigned long flags;

	/*
	 * The timer interrupt generation is disabled at least
	 * for enough time to call epit_set_next_event()
	 */
	arch_cpu_irq_save(flags);

	/* Disable interrupt */
	epit_irq_disable(ecc);

	if (mode != ecc->clockevent_mode) {
		/*
		 * Set event time into far-far future.
		 * The further we can go is to let the timer wrap arround
		 * once.
		 */

		/* read the actual counter */
		unsigned long tcmp = vmm_readl((void *)(ecc->base + EPITCNR));

		/*
		 * add 1 (as the counter is decrementing) and write the
		 * value.
		 */
		vmm_writel(tcmp + 1, (void *)(ecc->base + EPITCMPR));

		/* Clear pending interrupt */
		epit_irq_acknowledge(ecc);
	}

	/* Remember timer mode */
	ecc->clockevent_mode = mode;
	arch_cpu_irq_restore(flags);

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_PERIODIC:
		vmm_printf("epit_set_mode: Periodic mode is not "
			   "supported for i.MX EPIT\n");
		break;
	case VMM_CLOCKCHIP_MODE_ONESHOT:
		/*
		 * Do not put overhead of interrupt enable/disable into
		 * epit_set_next_event(), the core has about 4 minutes
		 * to call epit_set_next_event() or shutdown clock after
		 * mode switching
		 */
		arch_cpu_irq_save(flags);
		epit_irq_enable(ecc);
		arch_cpu_irq_restore(flags);
		break;
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
	case VMM_CLOCKCHIP_MODE_UNUSED:
	case VMM_CLOCKCHIP_MODE_RESUME:
		/* Left event sources disabled, no more interrupts appear */
		break;
	}
}

/*
 * IRQ handler for the timer
 */
static vmm_irq_return_t epit_timer_interrupt(int irq, void *dev)
{
	struct epit_clockchip *ecc = dev;

	epit_irq_acknowledge(ecc);

	ecc->clkchip.event_handler(&ecc->clkchip);

	return VMM_IRQ_HANDLED;
}

int __cpuinit epit_clockchip_init(void)
{
	int rc = VMM_ENODEV;
	u32 clock, hirq, timer_num, *val;
	struct vmm_devtree_node *node;
	struct epit_clockchip *ecc;

	/* find the first epit compatible node */
	node = vmm_devtree_find_compatible(NULL, NULL, "freescale,epit-timer");
	if (!node) {
		goto fail;
	}

	/* Read clock frequency */
	rc = vmm_devtree_clock_frequency(node, &clock);
	if (rc) {
		goto fail;
	}

	/* Read timer_num attribute */
	val = vmm_devtree_attrval(node, "timer_num");
	if (!val) {
		rc = VMM_ENOTAVAIL;
		goto fail;
	}
	timer_num = *val;

	/* Read irq attribute */
	rc = vmm_devtree_irq_get(node, &hirq, 0);
	if (rc) {
		goto fail;
	}

	/* allocate our struct */
	ecc = vmm_zalloc(sizeof(struct epit_clockchip));
	if (!ecc) {
		rc = VMM_ENOMEM;
		goto fail;
	}

	/* Map timer registers */
	rc = vmm_devtree_regmap(node, &ecc->base, 0);
	if (rc) {
		goto regmap_fail;
	}

	ecc->match_mask = 1 << timer_num;
	ecc->timer_num = timer_num;

	/* Setup clockchip */
	ecc->clkchip.name = node->name;
	ecc->clkchip.hirq = hirq;
	ecc->clkchip.rating = 300;
	ecc->clkchip.cpumask = vmm_cpumask_of(0);
	ecc->clkchip.features = VMM_CLOCKCHIP_FEAT_ONESHOT;
	vmm_clocks_calc_mult_shift(&ecc->clkchip.mult,
				   &ecc->clkchip.shift,
				   VMM_NSEC_PER_SEC, clock, 10);
	ecc->clkchip.min_delta_ns = vmm_clockchip_delta2ns(MIN_REG_COMPARE,
							   &ecc->clkchip);
	ecc->clkchip.max_delta_ns = vmm_clockchip_delta2ns(MAX_REG_COMPARE,
							   &ecc->clkchip);
	ecc->clkchip.set_mode = epit_set_mode;
	ecc->clkchip.set_next_event = epit_set_next_event;
	ecc->clkchip.priv = ecc;

	/*
	 * Initialise to a known state (all timers off, and timing reset)
	 */
	vmm_writel(0x0, (void *)(ecc->base + EPITCR));
	/*
	 * Initialize the load register to the max value to decrement.
	 */
	vmm_writel(0xffffffff, (void *)(ecc->base + EPITLR));
	/*
	 * enable the timer, set it to the high reference clock,
	 * allow the timer to work in WAIT mode.
	 */
	vmm_writel(EPITCR_EN | EPITCR_CLKSRC_REF_HIGH | EPITCR_WAITEN,
		   (void *)(ecc->base + EPITCR));

	/* Register interrupt handler */
	rc = vmm_host_irq_register(hirq, ecc->clkchip.name,
				   &epit_timer_interrupt, ecc);
	if (rc) {
		goto irq_fail;
	}

	/* Register clockchip */
	rc = vmm_clockchip_register(&ecc->clkchip);
	if (rc) {
		goto register_fail;
	}

	return VMM_OK;

 register_fail:
	vmm_host_irq_unregister(hirq, ecc);
 irq_fail:
	vmm_devtree_regunmap(node, ecc->base, 0);
 regmap_fail:
	vmm_free(ecc);
 fail:
	return rc;
}
