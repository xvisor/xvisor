/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief SP804 Dual-Mode Timer Implementation
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_devtree.h>
#include <vmm_host_irq.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <drv/clk.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)	vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define TIMER_LOAD		0x00
#define TIMER_VALUE		0x04
#define TIMER_CTRL		0x08
#define TIMER_CTRL_ONESHOT	(1 << 0)
#define TIMER_CTRL_32BIT	(1 << 1)
#define TIMER_CTRL_DIV1		(0 << 2)
#define TIMER_CTRL_DIV16	(1 << 2)
#define TIMER_CTRL_DIV256	(2 << 2)
#define TIMER_CTRL_IE		(1 << 5)
#define TIMER_CTRL_PERIODIC	(1 << 6)
#define TIMER_CTRL_ENABLE	(1 << 7)
#define TIMER_INTCLR		0x0c
#define TIMER_RIS		0x10
#define TIMER_MIS		0x14
#define TIMER_BGLOAD		0x18

static long __init sp804_get_clock_rate(struct clk *clk)
{
	long rate;
	int err;

	err = clk_prepare(clk);
	if (err) {
		vmm_printf("sp804: clock failed to prepare: %d\n", err);
		clk_put(clk);
		return err;
	}

	err = clk_enable(clk);
	if (err) {
		vmm_printf("sp804: clock failed to enable: %d\n", err);
		clk_unprepare(clk);
		clk_put(clk);
		return err;
	}

	rate = clk_get_rate(clk);
	if (rate < 0) {
		vmm_printf("sp804: clock failed to get rate: %ld\n", rate);
		clk_disable(clk);
		clk_unprepare(clk);
		clk_put(clk);
	}

	return rate;
}

struct sp804_clocksource {
	virtual_addr_t base;
	struct vmm_clocksource clksrc;
};

static u64 sp804_clocksource_read(struct vmm_clocksource *cs)
{
	u32 count;
	struct sp804_clocksource *tcs = cs->priv;

	count = vmm_readl((void *)(tcs->base + TIMER_VALUE));

	return ~count;
}

static int __init sp804_clocksource_init(struct vmm_devtree_node *node)
{
	int rc;
	long hz;
	u32 ctrl, freq_hz;
	virtual_addr_t base;
	struct clk *clk;
	struct sp804_clocksource *cs;

	rc = vmm_devtree_regmap(node, &base, 0);
	if (rc) {
		return rc;
	}

	clk = of_clk_get(node, 1);
	if (!clk) {
		clk = of_clk_get(node, 0);
	}
	if (!clk) {
		clk = clk_get_sys("sp804", "arm,sp804");
	}
	if (!clk) {
		vmm_devtree_regunmap(node, base, 0);
		return VMM_ENODEV;
	}
	hz = sp804_get_clock_rate(clk);
	if (hz < 0) {
		vmm_devtree_regunmap(node, base, 0);
		return (int)hz;
	}
	freq_hz = (u32)hz;

	DPRINTF("%s: name=%s base=0x%08x freq_hz=%d\n", 
		__func__, node->name, base, freq_hz);

	cs = vmm_zalloc(sizeof(struct sp804_clocksource));
	if (!cs) {
		return VMM_EFAIL;
	}

	cs->base = base;
	cs->clksrc.name = node->name;
	cs->clksrc.rating = 300;
	cs->clksrc.read = &sp804_clocksource_read;
	cs->clksrc.mask = VMM_CLOCKSOURCE_MASK(32);
	vmm_clocks_calc_mult_shift(&cs->clksrc.mult, &cs->clksrc.shift, 
				   freq_hz, VMM_NSEC_PER_SEC, 10);
	cs->clksrc.priv = cs;

	vmm_writel(0x0, (void *)(base + TIMER_CTRL));
	vmm_writel(0xFFFFFFFF, (void *)(base + TIMER_LOAD));
	ctrl = (TIMER_CTRL_ENABLE | TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC);
	vmm_writel(ctrl, (void *)(base + TIMER_CTRL));

	return vmm_clocksource_register(&cs->clksrc);
}
VMM_CLOCKSOURCE_INIT_DECLARE(sp804clksrc, "arm,sp804", sp804_clocksource_init);

struct sp804_clockchip {
	virtual_addr_t base;
	struct vmm_clockchip clkchip;
};

static vmm_irq_return_t sp804_clockchip_irq_handler(int irq_no, void *dev)
{
	struct sp804_clockchip *tcc = dev;

	/* clear the interrupt */
	vmm_writel(1, (void *)(tcc->base + TIMER_INTCLR));

	tcc->clkchip.event_handler(&tcc->clkchip);

	return VMM_IRQ_HANDLED;
}

static void sp804_clockchip_set_mode(enum vmm_clockchip_mode mode,
				     struct vmm_clockchip *cc)
{
	struct sp804_clockchip *tcc = cc->priv;
	unsigned long ctrl = TIMER_CTRL_32BIT | TIMER_CTRL_IE;

	vmm_writel(ctrl, (void *)(tcc->base + TIMER_CTRL));

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_PERIODIC:
		/* FIXME: */
		vmm_writel(10000, (void *)(tcc->base + TIMER_LOAD));
		ctrl |= TIMER_CTRL_PERIODIC | TIMER_CTRL_ENABLE;
		break;
	case VMM_CLOCKCHIP_MODE_ONESHOT:
		ctrl |= TIMER_CTRL_ONESHOT;
		break;
	case VMM_CLOCKCHIP_MODE_UNUSED:
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
	default:
		break;
	}

	vmm_writel(ctrl, (void *)(tcc->base + TIMER_CTRL));
}

static int sp804_clockchip_set_next_event(unsigned long next,
					  struct vmm_clockchip *cc)
{
	struct sp804_clockchip *tcc = cc->priv;
	unsigned long ctrl = vmm_readl((void *)(tcc->base + TIMER_CTRL));

	vmm_writel(next, (void *)(tcc->base + TIMER_LOAD));
	vmm_writel(ctrl | TIMER_CTRL_ENABLE, (void *)(tcc->base + TIMER_CTRL));

	return 0;
}

static int __cpuinit sp804_clockchip_init(struct vmm_devtree_node *node)
{
	int rc;
	long hz;
	u32 hirq, freq_hz;
	virtual_addr_t base;
	struct clk *clk;
	struct sp804_clockchip *cc;

	if (!vmm_smp_is_bootcpu()) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_irq_get(node, &hirq, 0);
	if (rc) {
		return rc;
	}

	rc = vmm_devtree_regmap(node, &base, 0);
	if (rc) {
		return rc;
	}
	base += 0x20;

	clk = of_clk_get(node, 1);
	if (VMM_IS_ERR(clk)) {
		clk = of_clk_get(node, 0);
	}
	if (VMM_IS_ERR(clk)) {
		clk = clk_get_sys("sp804", "arm,sp804");
	}
	if (VMM_IS_ERR(clk)) {
		vmm_devtree_regunmap(node, base, 0);
		return VMM_PTR_ERR(clk);
	}
	hz = sp804_get_clock_rate(clk);
	if (hz < 0) {
		vmm_devtree_regunmap(node, base, 0);
		return (int)hz;
	}
	freq_hz = (u32)hz;

	DPRINTF("%s: name=%s base=0x%08x freq_hz=%d\n",
		__func__, node->name, base, freq_hz);

	cc = vmm_zalloc(sizeof(struct sp804_clockchip));
	if (!cc) {
		vmm_devtree_regunmap(node, base, 0);
		return VMM_EFAIL;
	}

	cc->base = base;
	cc->clkchip.name = node->name;
	cc->clkchip.hirq = hirq;
	cc->clkchip.rating = 300;
	cc->clkchip.cpumask = cpu_all_mask;
	cc->clkchip.features = 
		VMM_CLOCKCHIP_FEAT_PERIODIC | VMM_CLOCKCHIP_FEAT_ONESHOT;
	vmm_clocks_calc_mult_shift(&cc->clkchip.mult, &cc->clkchip.shift, 
				   VMM_NSEC_PER_SEC, freq_hz, 10);
	cc->clkchip.min_delta_ns = vmm_clockchip_delta2ns(0xF, &cc->clkchip);
	cc->clkchip.max_delta_ns = 
			vmm_clockchip_delta2ns(0xFFFFFFFF, &cc->clkchip);
	cc->clkchip.set_mode = &sp804_clockchip_set_mode;
	cc->clkchip.set_next_event = &sp804_clockchip_set_next_event;
	cc->clkchip.priv = cc;

	/* Register interrupt handler */
	rc = vmm_host_irq_register(hirq, node->name,
				   &sp804_clockchip_irq_handler, cc);
	if (rc) {
		vmm_free(cc);
		return rc;
	}

	/* Register clockchip */
	rc = vmm_clockchip_register(&cc->clkchip);
	if (rc) {
		vmm_host_irq_unregister(hirq, cc);
		vmm_free(cc);
		return rc;
	}

	return VMM_OK;
}
VMM_CLOCKCHIP_INIT_DECLARE(sp804clkchip, "arm,sp804", sp804_clockchip_init);

