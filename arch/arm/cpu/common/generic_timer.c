/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file generic_timer.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief API implementation for ARM architecture generic timers
 *
 * The source has been partially adapted from drivers/clocksource/arm_generic.c 
 * of git://git.kernel.org/pub/scm/linux/kernel/git/cmarinas/linux-aarch64.git
 * Copyright (C) 2012 ARM Ltd.
 * Author: Marc Zyngier <marc.zyngier@arm.com>
 *
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_timer.h>
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <vmm_clockchip.h>
#include <vmm_clocksource.h>
#include <vmm_host_aspace.h>
#include <vmm_smp.h>
#include <cpu_inline_asm.h>
#include <arch_barrier.h>
#include <generic_timer.h>
#include <cpu_generic_timer.h>
#include <gic.h>

static u64 generic_counter_read(struct vmm_clocksource *cs)
{
	return generic_timer_counter_read();
}

int __init generic_timer_clocksource_init(const char *name, 
				          int rating, 
				          u32 freq_hz,
				          u32 shift)
{
	struct vmm_clocksource *cs;

	cs = vmm_zalloc(sizeof(struct vmm_clocksource));
	if (!cs) {
		return VMM_EFAIL;
	}

	cs->name = name;
	cs->rating = rating;
	cs->read = &generic_counter_read;
	cs->mask = 0x00FFFFFFFFFFFFFF;
	cs->mult = vmm_clocksource_hz2mult(freq_hz, shift);
	cs->shift = shift;
	cs->priv = NULL;
	return vmm_clocksource_register(cs);
}

static vmm_irq_return_t generic_timer_irq_handler(u32 irq, 
					       arch_regs_t *regs,
					       void *dev)
{
	struct vmm_clockchip *cc = dev;
	unsigned long ctrl;

	ctrl = generic_timer_reg_read(GENERIC_TIMER_REG_CTRL);
	if (ctrl & GENERIC_TIMER_CTRL_IT_STAT) {
		ctrl |= GENERIC_TIMER_CTRL_IT_MASK;
		ctrl &= ~GENERIC_TIMER_CTRL_ENABLE;
		generic_timer_reg_write(GENERIC_TIMER_REG_CTRL, ctrl);
		cc->event_handler(cc, regs);
		return VMM_IRQ_HANDLED;
	}

	return VMM_IRQ_NONE;
}

static void generic_timer_stop(void)
{
	unsigned long ctrl;

	ctrl = generic_timer_reg_read(GENERIC_TIMER_REG_CTRL);
	ctrl &= ~GENERIC_TIMER_CTRL_ENABLE;
	generic_timer_reg_write(GENERIC_TIMER_REG_CTRL, ctrl);
}

static void generic_timer_set_mode(enum vmm_clockchip_mode mode,
				struct vmm_clockchip *cc)
{
	switch (mode) {
	case VMM_CLOCKCHIP_MODE_UNUSED:
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
		generic_timer_stop();
		break;
	default:
		break;
	}
}

static int generic_timer_set_next_event(unsigned long evt,
				     struct vmm_clockchip *unused)
{
	unsigned long ctrl;

	ctrl = generic_timer_reg_read(GENERIC_TIMER_REG_CTRL);
	ctrl |= GENERIC_TIMER_CTRL_ENABLE;
	ctrl &= ~GENERIC_TIMER_CTRL_IT_MASK;

	generic_timer_reg_write(GENERIC_TIMER_REG_TVAL, evt);
	generic_timer_reg_write(GENERIC_TIMER_REG_CTRL, ctrl);

	return 0;
}

static int generic_timer_expire(struct vmm_clockchip *cc)
{
	unsigned long ctrl;
	int i;

	ctrl = generic_timer_reg_read(GENERIC_TIMER_REG_CTRL);
	ctrl |= GENERIC_TIMER_CTRL_ENABLE;
	ctrl &= ~GENERIC_TIMER_CTRL_IT_MASK;

	generic_timer_reg_write(GENERIC_TIMER_REG_TVAL, 1);
	generic_timer_reg_write(GENERIC_TIMER_REG_CTRL, ctrl);

	while(!(generic_timer_reg_read(GENERIC_TIMER_REG_CTRL) &
				       GENERIC_TIMER_CTRL_IT_STAT)) {
		for(i = 0; i<1000; i++);
	}

	return 0;
}

int __init generic_timer_clockchip_init(const char *name,
				        u32 hirq,
				        int rating, 
				        u32 freq_hz)
{
	int rc;
	struct vmm_clockchip *cc;

	cc = vmm_zalloc(sizeof(struct vmm_clockchip));
	if (!cc) {
		return VMM_EFAIL;
	}

	cc->name = name;
	cc->hirq = hirq;
	cc->rating = rating;
	cc->cpumask = vmm_cpumask_of(vmm_smp_processor_id());
	cc->features = VMM_CLOCKCHIP_FEAT_ONESHOT;
	cc->mult = vmm_clockchip_hz2mult(freq_hz, 32);
	cc->shift = 32;
	cc->min_delta_ns = vmm_clockchip_delta2ns(0xF, cc);
	cc->max_delta_ns = vmm_clockchip_delta2ns(0x7FFFFFFF, cc);
	cc->set_mode = &generic_timer_set_mode;
	cc->set_next_event = &generic_timer_set_next_event;
	cc->expire = &generic_timer_expire;
	cc->priv = NULL;

	generic_timer_stop();
	/* Register interrupt handler */
	if ((rc = vmm_host_irq_register(hirq, name,
					&generic_timer_irq_handler, cc))) {
		return rc;
	}

	/* Mark interrupt as per-cpu */
	if ((rc = vmm_host_irq_mark_per_cpu(cc->hirq))) {
		return rc;
	}

	/* Explicitly enable local timer PPI in GIC 
	 * Note: Arch timers are connected to PPI
	 */
	gic_enable_ppi(hirq);

	return vmm_clockchip_register(cc);
}
