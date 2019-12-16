/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file riscv_timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V timer clocksource and clockchips
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_smp.h>
#include <vmm_cpuhp.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_host_irq.h>
#include <vmm_clockchip.h>
#include <vmm_clocksource.h>
#include <libs/mathlib.h>

#include <cpu_hwcap.h>
#include <cpu_sbi.h>
#include <riscv_encoding.h>
#include <riscv_timex.h>
#include <riscv_csr.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

static int riscv_hart_of_timer(struct vmm_devtree_node *node, u32 *hart_id)
{
	int rc;

	if (!node)
		return VMM_EINVALID;
	if (!vmm_devtree_is_compatible(node, "riscv"))
		return VMM_ENODEV;

	if (hart_id) {
		rc = vmm_devtree_read_u32(node, "reg", hart_id);
		if (rc)
			return rc;
	}

	return VMM_OK;
}

static u64 riscv_timer_read(struct vmm_clocksource *cs)
{
	return get_cycles64();
}

static int __init riscv_timer_clocksource_init(struct vmm_devtree_node *node)
{
	int rc;
	u32 hart_id = 0;
	unsigned long hwid;
	struct vmm_clocksource *cs;

	rc = vmm_smp_map_hwid(vmm_smp_processor_id(), &hwid);
	if (rc) {
		return rc;
	}

	rc = riscv_hart_of_timer(node, &hart_id);
	if (rc) {
		return rc;
	}

	if (hwid != hart_id) {
		return VMM_OK;
	}

	/* Create riscv timer clocksource */
	cs = vmm_zalloc(sizeof(struct vmm_clocksource));
	if (!cs) {
		return VMM_EFAIL;
	}

	cs->name = "riscv-timer";
	cs->rating = 400;
	cs->read = &riscv_timer_read;
	cs->mask = VMM_CLOCKSOURCE_MASK(64);
	cs->freq = riscv_timer_hz;
	vmm_clocks_calc_mult_shift(&cs->mult, &cs->shift,
				   riscv_timer_hz, VMM_NSEC_PER_SEC, 10);
	cs->priv = NULL;

	/* Register riscv timer clocksource */
	return vmm_clocksource_register(cs);
}
VMM_CLOCKSOURCE_INIT_DECLARE(riscvclksrc, "riscv",
			     riscv_timer_clocksource_init);

static void riscv_timer_set_mode(enum vmm_clockchip_mode mode,
				 struct vmm_clockchip *cc)
{
	/* For now, nothing to do here. */
}

static int riscv_timer_set_next_event(unsigned long evt,
				      struct vmm_clockchip *unused)
{
	csr_set(sie, SIE_STIE);
	sbi_set_timer(get_cycles64() + evt);

	return VMM_OK;
}

static vmm_irq_return_t riscv_timer_handler(int irq, void *dev)
{
	struct vmm_clockchip *cc = dev;

	/*
	 * There are no direct SBI calls to clear pending timer interrupt bit.
	 * Disable timer interrupt to ignore pending interrupt until next
	 * interrupt.
	 */
	csr_clear(sie, SIE_STIE);
	cc->event_handler(cc);

	return VMM_IRQ_HANDLED;
}

static int riscv_timer_startup(struct vmm_cpuhp_notify *cpuhp, u32 cpu)
{
	int rc;
	struct vmm_clockchip *cc;

	/* Create riscv timer clockchip */
	cc = vmm_zalloc(sizeof(struct vmm_clockchip));
	if (!cc) {
		return VMM_EFAIL;
	}
	cc->name = "riscv-timer";
	cc->hirq = IRQ_S_TIMER;
	cc->rating = 400;
	cc->cpumask = vmm_cpumask_of(cpu);
	cc->features = VMM_CLOCKCHIP_FEAT_ONESHOT;
	cc->freq = riscv_timer_hz;
	vmm_clocks_calc_mult_shift(&cc->mult, &cc->shift,
				   VMM_NSEC_PER_SEC, riscv_timer_hz, 10);
	cc->min_delta_ns = vmm_clockchip_delta2ns(0xF, cc);
	cc->max_delta_ns = vmm_clockchip_delta2ns(0x7FFFFFFF, cc);
	cc->set_mode = &riscv_timer_set_mode;
	cc->set_next_event = &riscv_timer_set_next_event;
	cc->priv = NULL;

	/* Register riscv timer clockchip */
	rc = vmm_clockchip_register(cc);
	if (rc) {
		goto fail_free_cc;
	}

	/* Register irq handler for riscv timer */
	rc = vmm_host_irq_register(IRQ_S_TIMER, "riscv-timer",
				   &riscv_timer_handler, cc);
	if (rc) {
		goto fail_unreg_cc;
	}

	return VMM_OK;

fail_unreg_cc:
	vmm_clockchip_unregister(cc);
fail_free_cc:
	vmm_free(cc);
	return rc;
}

static struct vmm_cpuhp_notify riscv_timer_cpuhp = {
	.name = "RISCV_TIMER",
	.state = VMM_CPUHP_STATE_CLOCKCHIP,
	.startup = riscv_timer_startup,
};

static int __init riscv_timer_clockchip_init(struct vmm_devtree_node *node)
{
	int rc;
	u32 hart_id;
	unsigned long hwid;

	rc = vmm_smp_map_hwid(vmm_smp_processor_id(), &hwid);
	if (rc) {
		return rc;
	}

	rc = riscv_hart_of_timer(node, &hart_id);
	if (rc) {
		return rc;
	}

	if (hwid != hart_id) {
		return VMM_OK;
	}

	return vmm_cpuhp_register(&riscv_timer_cpuhp, TRUE);
}
VMM_CLOCKCHIP_INIT_DECLARE(riscvclkchip, "riscv",
			   riscv_timer_clockchip_init);
