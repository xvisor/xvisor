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
#include <vmm_scheduler.h>
#include <vmm_manager.h>
#include <vmm_smp.h>
#include <vmm_devemu.h>
#include <cpu_inline_asm.h>
#include <arch_barrier.h>
#include <generic_timer.h>
#include <cpu_generic_timer.h>
#include <libs/mathlib.h>
#include <gic.h>

static u32 generic_timer_hz = 0;

static u64 generic_counter_read(struct vmm_clocksource *cs)
{
	return generic_timer_pcounter_read();
}

int __init generic_timer_clocksource_init(struct vmm_devtree_node *node)
{
	struct vmm_clocksource *cs;
	u32 *freq, *shift, *rating;

	if (generic_timer_hz == 0) {
		freq = vmm_devtree_attrval(node, "freq");
		if (freq == NULL) {
			/* Use preconfigured counter frequency in absence of dts node */
			generic_timer_hz = generic_timer_reg_read(GENERIC_TIMER_REG_FREQ);
		} else {
			if (generic_timer_freq_writeable()) {
				/* Program the counter frequency as per the dts node */
				generic_timer_reg_write(GENERIC_TIMER_REG_FREQ, *freq);
			}
			generic_timer_hz = *freq;
		}
	}

	if (generic_timer_hz == 0) {
		return VMM_EFAIL;
	}

	shift = vmm_devtree_attrval(node, "shift");
	if (!shift) {
		return VMM_EFAIL;
	}

	rating = vmm_devtree_attrval(node, "rating");
	if (!rating) {
		return VMM_EFAIL;
	}

	cs = vmm_zalloc(sizeof(struct vmm_clocksource));
	if (!cs) {
		return VMM_EFAIL;
	}

	cs->name = "gen-timer";
	cs->rating = *rating;
	cs->read = &generic_counter_read;
	cs->mask = VMM_CLOCKSOURCE_MASK(56);
	cs->mult = vmm_clocksource_hz2mult(generic_timer_hz, *shift);
	cs->shift = *shift;
	cs->priv = NULL;
	return vmm_clocksource_register(cs);
}

static vmm_irq_return_t generic_timer_irq_handler(u32 irq,
					       arch_regs_t *regs,
					       void *dev)
{
	struct vmm_clockchip *cc = dev;
	unsigned long ctrl;

	ctrl = generic_timer_reg_read(GENERIC_TIMER_REG_HYP_CTRL);
	if (ctrl & GENERIC_TIMER_CTRL_IT_STAT) {
		ctrl |= GENERIC_TIMER_CTRL_IT_MASK;
		ctrl &= ~GENERIC_TIMER_CTRL_ENABLE;
		generic_timer_reg_write(GENERIC_TIMER_REG_HYP_CTRL, ctrl);
		cc->event_handler(cc, regs);
		return VMM_IRQ_HANDLED;
	}

	return VMM_IRQ_NONE;
}

static void generic_timer_stop(void)
{
	unsigned long ctrl;

	ctrl = generic_timer_reg_read(GENERIC_TIMER_REG_HYP_CTRL);
	ctrl &= ~GENERIC_TIMER_CTRL_ENABLE;
	generic_timer_reg_write(GENERIC_TIMER_REG_HYP_CTRL, ctrl);
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

	ctrl = generic_timer_reg_read(GENERIC_TIMER_REG_HYP_CTRL);
	ctrl |= GENERIC_TIMER_CTRL_ENABLE;
	ctrl &= ~GENERIC_TIMER_CTRL_IT_MASK;

	generic_timer_reg_write(GENERIC_TIMER_REG_HYP_TVAL, evt);
	generic_timer_reg_write(GENERIC_TIMER_REG_HYP_CTRL, ctrl);

	return 0;
}

static int generic_timer_expire(struct vmm_clockchip *cc)
{
	unsigned long ctrl;
	int i;

	ctrl = generic_timer_reg_read(GENERIC_TIMER_REG_HYP_CTRL);
	ctrl |= GENERIC_TIMER_CTRL_ENABLE;
	ctrl &= ~GENERIC_TIMER_CTRL_IT_MASK;

	generic_timer_reg_write(GENERIC_TIMER_REG_HYP_TVAL, 1);
	generic_timer_reg_write(GENERIC_TIMER_REG_HYP_CTRL, ctrl);

	while(!(generic_timer_reg_read(GENERIC_TIMER_REG_HYP_CTRL) &
				       GENERIC_TIMER_CTRL_IT_STAT)) {
		for(i = 0; i<1000; i++);
	}

	return 0;
}

static vmm_irq_return_t generic_phys_timer_virq_handler(u32 irq,
						       arch_regs_t *regs,
						       void *dev)
{
	struct vmm_vcpu * vcpu;
	u32 ctl;

	vcpu = vmm_scheduler_current_vcpu();

	if (!vcpu->is_normal) {
		vmm_panic("%s in orphan-vcpu context\n", __func__);
	}

	if(arm_gentimer_context(vcpu)->phys_timer_irq == 0)
		return VMM_IRQ_NONE;

	if(!vmm_devemu_emulate_percpu_irq(vcpu->guest,
			arm_gentimer_context(vcpu)->phys_timer_irq, vcpu->subid, 0)) {
		ctl = generic_timer_reg_read(GENERIC_TIMER_REG_PHYS_CTRL);
		ctl |= GENERIC_TIMER_CTRL_IT_MASK;
		generic_timer_reg_write(GENERIC_TIMER_REG_PHYS_CTRL, ctl);
		vmm_devemu_emulate_percpu_irq(vcpu->guest,
			arm_gentimer_context(vcpu)->phys_timer_irq, vcpu->subid, 1);	
	}

	return VMM_IRQ_HANDLED;
}

static vmm_irq_return_t generic_virt_timer_virq_handler(u32 irq,
						       arch_regs_t *regs,
						       void *dev)
{
	struct vmm_vcpu * vcpu;
	u32 ctl;

	vcpu = vmm_scheduler_current_vcpu();

	if (!vcpu->is_normal) {
		vmm_panic("%s in orphan-vcpu context\n", __func__);
	}

	if(arm_gentimer_context(vcpu)->virt_timer_irq == 0)
		return VMM_IRQ_NONE;

	if(!vmm_devemu_emulate_percpu_irq(vcpu->guest,
			arm_gentimer_context(vcpu)->virt_timer_irq, vcpu->subid, 0)) {
		ctl = generic_timer_reg_read(GENERIC_TIMER_REG_VIRT_CTRL);
		ctl |= GENERIC_TIMER_CTRL_IT_MASK;
		generic_timer_reg_write(GENERIC_TIMER_REG_VIRT_CTRL, ctl);
		vmm_devemu_emulate_percpu_irq(vcpu->guest,
			arm_gentimer_context(vcpu)->virt_timer_irq, vcpu->subid, 1);
	}

	return VMM_IRQ_HANDLED;
}

u64 generic_timer_wakeup_timeout(void)
{
	u32 vtval = 0, ptval = 0;
	u64 nsecs = 0;

	if (generic_timer_hz == 0)
		return 0;
	
	if (generic_timer_reg_read(GENERIC_TIMER_REG_PHYS_CTRL) &
			GENERIC_TIMER_CTRL_ENABLE) {
		ptval = generic_timer_reg_read(GENERIC_TIMER_REG_PHYS_TVAL);
	}
	
	if (generic_timer_reg_read(GENERIC_TIMER_REG_VIRT_CTRL) &
			GENERIC_TIMER_CTRL_ENABLE) {
		vtval = generic_timer_reg_read(GENERIC_TIMER_REG_VIRT_TVAL);
		
	}

	if ((ptval > 0) && (vtval > 0))
		nsecs = (ptval > vtval) ? vtval : ptval;
	else
		nsecs = (ptval > vtval) ? ptval : vtval;

	if (nsecs) {
		if (generic_timer_hz == 100000000) {
			nsecs = nsecs * 10;
		} else {
			nsecs = udiv64((nsecs * 1000000000), (u64)generic_timer_hz);
		}
	}

	return nsecs;
}

int generic_timer_clockchip_init(struct vmm_devtree_node *node)
{
	u32 *freq, *irq, *rating, num_irqs, val, rc;
	struct vmm_clockchip *cc;

	if (generic_timer_hz == 0) {
		freq = vmm_devtree_attrval(node, "freq");
		if (freq == NULL) {
			/* Use preconfigured counter frequency in absence of dts node */
			generic_timer_hz = generic_timer_reg_read(GENERIC_TIMER_REG_FREQ);
		} else {
			if (generic_timer_freq_writeable()) {
				/* Program the counter frequency as per the dts node */
				generic_timer_reg_write(GENERIC_TIMER_REG_FREQ, *freq);
			}
			generic_timer_hz = *freq;
		}
	}

	if (generic_timer_hz == 0) {
		return VMM_EFAIL;
	}

	irq = vmm_devtree_attrval(node, "irq");
	num_irqs = (vmm_devtree_attrlen(node, "irq") / sizeof(u32));
	if (irq == NULL) {
		return VMM_EFAIL;
	}

	generic_timer_stop();

	/* Initialize generic hypervisor timer as clockchip */
	cc = vmm_zalloc(sizeof(struct vmm_clockchip));
	if (!cc) {
		return VMM_EFAIL;
	}

	rating = vmm_devtree_attrval(node, "rating");
	if (!rating) {
		return VMM_EFAIL;
	}

	cc->name = "gen-hyp-timer";
	cc->hirq = irq[GENERIC_HYPERVISOR_TIMER];
	cc->rating = *rating;
	cc->cpumask = vmm_cpumask_of(vmm_smp_processor_id());
	cc->features = VMM_CLOCKCHIP_FEAT_ONESHOT;
	cc->mult = vmm_clockchip_hz2mult(generic_timer_hz, 32);
	cc->shift = 32;
	cc->min_delta_ns = vmm_clockchip_delta2ns(0xF, cc);
	cc->max_delta_ns = vmm_clockchip_delta2ns(0x7FFFFFFF, cc);
	cc->set_mode = &generic_timer_set_mode;
	cc->set_next_event = &generic_timer_set_next_event;
	cc->expire = &generic_timer_expire;
	cc->priv = NULL;

	rc = vmm_clockchip_register(cc);
	if (rc) {
		return rc;
	}

	if (!vmm_smp_processor_id()) {
		/* Register irq for handling hypervisor timer */
		if ((rc = vmm_host_irq_register(irq[GENERIC_HYPERVISOR_TIMER],
						"gen-hyp-timer",
						&generic_timer_irq_handler, cc))) {
			return rc;
		}

		if ((rc = vmm_host_irq_mark_per_cpu(cc->hirq))) {
			return rc;
		}

		/* Register irq for handling physical timer */
		if (num_irqs > 1) {
			val = generic_timer_reg_read(GENERIC_TIMER_REG_HCTL);
			val |= GENERIC_TIMER_HCTL_KERN_PCNT_EN;
			val |= GENERIC_TIMER_HCTL_KERN_PTMR_EN;
			generic_timer_reg_write(GENERIC_TIMER_REG_HCTL, val);
			rc = vmm_host_irq_register(irq[GENERIC_PHYSICAL_TIMER],
					"gen-phys-timer",
					&generic_phys_timer_virq_handler,
					NULL);
			if (rc) {
				return rc;
			}

			rc = vmm_host_irq_mark_per_cpu(irq[GENERIC_PHYSICAL_TIMER]);
			if (rc)	{
				return rc;
			}
		}

		/* Register irq for handling virtual timer */
		if (num_irqs > 2) {
			rc = vmm_host_irq_register(irq[GENERIC_VIRTUAL_TIMER],
					"gen-virt-timer",
					&generic_virt_timer_virq_handler,
					NULL);
			if (rc) {
				return rc;
			}

			rc = vmm_host_irq_mark_per_cpu(irq[GENERIC_VIRTUAL_TIMER]);
			if (rc) {
				return rc;
			}
		}
	}

	for(val = 0; val < num_irqs; val++) {
		gic_enable_ppi(irq[val]);
	}

	return VMM_OK;
}


void generic_timer_vcpu_context_init(struct generic_timer_context *context)
{
	context->cntpctl = GENERIC_TIMER_CTRL_IT_MASK;
	context->cntvctl = GENERIC_TIMER_CTRL_IT_MASK;
	context->cntpcval = 0;
	context->cntvcval = 0;
	context->cntkctl = 0;
	context->cntvoff = generic_timer_pcounter_read();
}

void generic_timer_vcpu_context_save(struct generic_timer_context *context)
{
	context->cntpctl = generic_timer_reg_read(GENERIC_TIMER_REG_PHYS_CTRL);
	context->cntvctl = generic_timer_reg_read(GENERIC_TIMER_REG_VIRT_CTRL);
	context->cntpcval = generic_timer_reg_read64(GENERIC_TIMER_REG_PHYS_CVAL);
	context->cntvcval = generic_timer_reg_read64(GENERIC_TIMER_REG_VIRT_CVAL);
	context->cntkctl = generic_timer_reg_read(GENERIC_TIMER_REG_KCTL);
	generic_timer_reg_write(GENERIC_TIMER_REG_PHYS_CTRL, 0);
	generic_timer_reg_write(GENERIC_TIMER_REG_VIRT_CTRL, 0);
}

void generic_timer_vcpu_context_restore(struct generic_timer_context *context)
{
	generic_timer_reg_write64(GENERIC_TIMER_REG_VIRT_OFF, context->cntvoff);
	generic_timer_reg_write(GENERIC_TIMER_REG_KCTL, context->cntkctl);
	generic_timer_reg_write64(GENERIC_TIMER_REG_PHYS_CVAL, context->cntpcval);
	generic_timer_reg_write64(GENERIC_TIMER_REG_VIRT_CVAL, context->cntvcval);
	generic_timer_reg_write(GENERIC_TIMER_REG_PHYS_CTRL, context->cntpctl);
	generic_timer_reg_write(GENERIC_TIMER_REG_VIRT_CTRL, context->cntvctl);
}

