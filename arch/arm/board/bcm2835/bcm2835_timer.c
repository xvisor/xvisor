/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file bcm2835_timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2835 timer implementation
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>

#include <bcm2835_timer.h>

#define REG_CONTROL	0x00
#define REG_COUNTER_LO	0x04
#define REG_COUNTER_HI	0x08
#define REG_COMPARE(n)	(0x0c + (n) * 4)
#define MAX_TIMER	3
#define DEFAULT_TIMER	3

#define MIN_REG_COMPARE	0xFF
#define MAX_REG_COMPARE	0xFFFFFFFF

struct bcm2835_clocksource {
	virtual_addr_t base;
	void *system_clock;
	struct vmm_clocksource clksrc;
};

static u64 bcm2835_clksrc_read(struct vmm_clocksource *cs)
{
	struct bcm2835_clocksource *bcs = cs->priv;

	return vmm_readl(bcs->system_clock);
}

int __init bcm2835_clocksource_init(void)
{
	int rc;
	u32 clock;
	struct vmm_devtree_node *node;
	struct bcm2835_clocksource *bcs;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME);
	if (!node) {
		return VMM_ENODEV;
	}

	node = vmm_devtree_find_compatible(node, NULL, 
					   "brcm,bcm2835-system-timer");
	if (!node) {
		return VMM_ENODEV;
	}

	/* Read clock frequency */
	rc = vmm_devtree_clock_frequency(node, &clock);
	if (rc) {
		return rc;
	}

	bcs = vmm_zalloc(sizeof(struct bcm2835_clocksource));
	if (!bcs) {
		return VMM_ENOMEM;
	}

	/* Map timer registers */
	rc = vmm_devtree_regmap(node, &bcs->base, 0);
	if (rc) {
		vmm_free(bcs);
		return rc;
	}
	bcs->system_clock = (void *)(bcs->base + REG_COUNTER_LO);

	/* Setup clocksource */
	bcs->clksrc.name = "bcm2835_timer";
	bcs->clksrc.rating = 300;
	bcs->clksrc.read = bcm2835_clksrc_read;
	bcs->clksrc.mask = VMM_CLOCKSOURCE_MASK(32);
	vmm_clocks_calc_mult_shift(&bcs->clksrc.mult, 
				   &bcs->clksrc.shift,
				   clock, VMM_NSEC_PER_SEC, 10);
	bcs->clksrc.priv = bcs;

	/* Register clocksource */
	rc = vmm_clocksource_register(&bcs->clksrc);
	if (rc) {
		vmm_devtree_regunmap(node, bcs->base, 0);
		vmm_free(bcs);
		return rc;
	}

	return VMM_OK;
}

struct bcm2835_clockchip {
	void *system_clock;
	void *control;
	void *compare;
	u32 match_mask;
	virtual_addr_t base;
	struct vmm_clockchip clkchip;
};

static vmm_irq_return_t bcm2835_clockchip_irq_handler(u32 irq_no, void *dev)
{
	struct bcm2835_clockchip *bcc = dev;

	if (vmm_readl(bcc->control) & bcc->match_mask) {
		vmm_writel(bcc->match_mask, bcc->control);

		bcc->clkchip.event_handler(&bcc->clkchip);

		return VMM_IRQ_HANDLED;
	}

	return VMM_IRQ_NONE;
}

static void bcm2835_clockchip_set_mode(enum vmm_clockchip_mode mode,
					struct vmm_clockchip *cc)
{
	/* Timer is always running in one-shot mode */
	/* Nothing to do here !!!!! */

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_PERIODIC:
	case VMM_CLOCKCHIP_MODE_ONESHOT:
	case VMM_CLOCKCHIP_MODE_UNUSED:
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
		break;
	default:
		break;
	}
}

static int bcm2835_clockchip_set_next_event(unsigned long next, 
					    struct vmm_clockchip *cc)
{
	struct bcm2835_clockchip *bcc = cc->priv;

	/* Configure compare register */
	vmm_writel(vmm_readl(bcc->system_clock) + next, bcc->compare);

	return VMM_OK;
}

static int bcm2835_clockchip_expire(struct vmm_clockchip *cc)
{
	u32 i;
	struct bcm2835_clockchip *bcc = cc->priv;

	/* Configure compare register for shortest duration */
	i = vmm_readl(bcc->system_clock) + MIN_REG_COMPARE;
	vmm_writel(i, bcc->compare);

	/* Wait for timer to expire */
	while (!(vmm_readl(bcc->control) & bcc->match_mask)) {
		/* Relax CPU with ramdom delay */
		for (i = 0; i < 100; i++) ;
	}
	
	return VMM_OK;
}

int __cpuinit bcm2835_clockchip_init(void)
{
	int rc;
	u32 clock, hirq;
	struct vmm_devtree_node *node;
	struct bcm2835_clockchip *bcc;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME);
	if (!node) {
		return VMM_ENODEV;
	}

	node = vmm_devtree_find_compatible(node, NULL, 
					   "brcm,bcm2835-system-timer");
	if (!node) {
		return VMM_ENODEV;
	}

	/* Read clock frequency */
	rc = vmm_devtree_clock_frequency(node, &clock);
	if (rc) {
		return rc;
	}

	/* Read irq attribute */
	rc = vmm_devtree_irq_get(node, &hirq, DEFAULT_TIMER);
	if (rc) {
		return rc;
	}

	bcc = vmm_zalloc(sizeof(struct bcm2835_clockchip));
	if (!bcc) {
		return VMM_ENOMEM;
	}

	/* Map timer registers */
	rc = vmm_devtree_regmap(node, &bcc->base, 0);
	if (rc) {
		vmm_free(bcc);
		return rc;
	}
	bcc->system_clock = (void *)(bcc->base + REG_COUNTER_LO);
	bcc->control = (void *)(bcc->base + REG_CONTROL);
	bcc->compare = (void *)(bcc->base + REG_COMPARE(DEFAULT_TIMER));
	bcc->match_mask = 1 << DEFAULT_TIMER;

	/* Setup clockchip */
	bcc->clkchip.name = "bcm2835-clkchip";
	bcc->clkchip.hirq = hirq;
	bcc->clkchip.rating = 300;
	bcc->clkchip.cpumask = vmm_cpumask_of(0);
	bcc->clkchip.features = VMM_CLOCKCHIP_FEAT_ONESHOT;
	vmm_clocks_calc_mult_shift(&bcc->clkchip.mult, 
				   &bcc->clkchip.shift,
				   VMM_NSEC_PER_SEC, clock, 10);
	bcc->clkchip.min_delta_ns = vmm_clockchip_delta2ns(MIN_REG_COMPARE, 
							   &bcc->clkchip);
	bcc->clkchip.max_delta_ns = vmm_clockchip_delta2ns(MAX_REG_COMPARE, 
							   &bcc->clkchip);
	bcc->clkchip.set_mode = &bcm2835_clockchip_set_mode;
	bcc->clkchip.set_next_event = &bcm2835_clockchip_set_next_event;
	bcc->clkchip.expire = &bcm2835_clockchip_expire;
	bcc->clkchip.priv = bcc;

	/* Make sure compare register is set to zero */
	vmm_writel(0x0, bcc->compare);

	/* Make sure pending timer interrupts acknowledged */
	if (vmm_readl(bcc->control) & bcc->match_mask) {
		vmm_writel(bcc->match_mask, bcc->control);
	}

	/* Register interrupt handler */
	rc = vmm_host_irq_register(hirq, "bcm2835_timer",
				   &bcm2835_clockchip_irq_handler, bcc);
	if (rc) {
		vmm_devtree_regunmap(node, bcc->base, 0);
		vmm_free(bcc);
		return rc;
	}

	/* Register clockchip */
	rc = vmm_clockchip_register(&bcc->clkchip);
	if (rc) {
		vmm_host_irq_unregister(hirq, bcc);
		vmm_devtree_regunmap(node, bcc->base, 0);
		vmm_free(bcc);
		return rc;
	}

	return VMM_OK;
}

