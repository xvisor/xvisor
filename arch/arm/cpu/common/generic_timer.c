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
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_host_irq.h>
#include <vmm_clockchip.h>
#include <vmm_clocksource.h>
#include <vmm_host_aspace.h>
#include <vmm_scheduler.h>
#include <vmm_smp.h>
#include <vmm_devemu.h>
#include <generic_timer.h>
#include <cpu_generic_timer.h>
#include <libs/mathlib.h>

#define GENERIC_TIMER_HCTL_KERN_PCNT_EN		(1 << 0)
#define GENERIC_TIMER_HCTL_KERN_PTMR_EN		(1 << 1)

#define GENERIC_TIMER_CTRL_ENABLE		(1 << 0)
#define GENERIC_TIMER_CTRL_IT_MASK		(1 << 1)
#define GENERIC_TIMER_CTRL_IT_STAT		(1 << 2)

enum gen_timer_type {
	GENERIC_HYPERVISOR_TIMER,
	GENERIC_PHYSICAL_TIMER,
	GENERIC_VIRTUAL_TIMER,
};

struct generic_timer_context {
	struct vmm_vcpu *vcpu;
	u32 phys_timer_irq;
	u32 virt_timer_irq;
	u64 cntvoff;
	u64 cntpcval;
	u64 cntvcval;
	u32 cntkctl;
	u32 cntpctl;
	u32 cntvctl;
};

static u32 generic_timer_hz = 0;

static u64 generic_counter_read(struct vmm_clocksource *cs)
{
	return generic_timer_pcounter_read();
}

static int __init generic_timer_clocksource_init(struct vmm_devtree_node *node)
{
	int rc;
	struct vmm_clocksource *cs;

	if (generic_timer_hz == 0) {
		rc =  vmm_devtree_clock_frequency(node, &generic_timer_hz);
		if (rc) {
			/* Use preconfigured counter frequency 
			 * in absence of dts node 
			 */
			generic_timer_hz = 
				generic_timer_reg_read(GENERIC_TIMER_REG_FREQ);
		} else {
			if (generic_timer_freq_writeable()) {
				/* Program the counter frequency 
				 * as per the dts node
				 */
				generic_timer_reg_write(GENERIC_TIMER_REG_FREQ,
							generic_timer_hz);
			}
		}
	}

	if (generic_timer_hz == 0) {
		return VMM_EFAIL;
	}

	cs = vmm_zalloc(sizeof(struct vmm_clocksource));
	if (!cs) {
		return VMM_EFAIL;
	}

	cs->name = "gen-timer";
	cs->rating = 400;
	cs->read = &generic_counter_read;
	cs->mask = VMM_CLOCKSOURCE_MASK(56);
	vmm_clocks_calc_mult_shift(&cs->mult, &cs->shift, 
				   generic_timer_hz, VMM_NSEC_PER_SEC, 10);
	cs->priv = NULL;

	return vmm_clocksource_register(cs);
}
VMM_CLOCKSOURCE_INIT_DECLARE(gtv7clksrc, "arm,armv7-timer", generic_timer_clocksource_init);
VMM_CLOCKSOURCE_INIT_DECLARE(gtv8clksrc, "arm,armv8-timer", generic_timer_clocksource_init);

static vmm_irq_return_t generic_hyp_timer_handler(int irq, void *dev)
{
	struct vmm_clockchip *cc = dev;
	unsigned long ctrl;

	ctrl = generic_timer_reg_read(GENERIC_TIMER_REG_HYP_CTRL);
	if (ctrl & GENERIC_TIMER_CTRL_IT_STAT) {
		ctrl |= GENERIC_TIMER_CTRL_IT_MASK;
		ctrl &= ~GENERIC_TIMER_CTRL_ENABLE;
		generic_timer_reg_write(GENERIC_TIMER_REG_HYP_CTRL, ctrl);
		cc->event_handler(cc);
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

static vmm_irq_return_t generic_phys_timer_handler(int irq, void *dev)
{
	int rc;
	u32 ctl, pirq;
	struct vmm_vcpu *vcpu;
	struct generic_timer_context *cntx;

	ctl = generic_timer_reg_read(GENERIC_TIMER_REG_PHYS_CTRL);
	if (!(ctl & GENERIC_TIMER_CTRL_IT_STAT)) {
		/* We got interrupt without status bit set.
		 * Looks like we are running on buggy hardware.
		 */
		vmm_printf("%s: suprious interrupt\n", __func__);
		return VMM_IRQ_NONE;
	}

	ctl |= GENERIC_TIMER_CTRL_IT_MASK;
	generic_timer_reg_write(GENERIC_TIMER_REG_PHYS_CTRL, ctl);

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu->is_normal) {
		/* We accidently got an interrupt meant for normal VCPU 
		 * that was previously running on this host CPU. 
		 */
		vmm_printf("%s: In orphan context (current VCPU=%s)\n",
			   __func__, vcpu->name);
		return VMM_IRQ_NONE;
	}

	cntx = arm_gentimer_context(vcpu);
	if (!cntx) {
		/* We accidently got an interrupt meant another normal VCPU */
		vmm_printf("%s: Invalid normal context (current VCPU=%s)\n",
			   __func__, vcpu->name);
		return VMM_IRQ_NONE;
	}

	pirq = cntx->phys_timer_irq;
	if (pirq == 0) {
		return VMM_IRQ_NONE;
	}

	rc = vmm_devemu_emulate_percpu_irq(vcpu->guest, pirq, vcpu->subid, 0);
	if (rc) {
		vmm_printf("%s: Emulate VCPU=%s irq=%d level=0 failed\n",
			   __func__, vcpu->name, pirq);
	}

	rc = vmm_devemu_emulate_percpu_irq(vcpu->guest, pirq, vcpu->subid, 1);
	if (rc) {
		vmm_printf("%s: Emulate VCPU=%s irq=%d level=1 failed\n",
			   __func__, vcpu->name, pirq);
	}

	return VMM_IRQ_HANDLED;
}

static vmm_irq_return_t generic_virt_timer_handler(int irq, void *dev)
{
	int rc;
	u32 ctl, virq;
	struct vmm_vcpu *vcpu;
	struct generic_timer_context *cntx;

	ctl = generic_timer_reg_read(GENERIC_TIMER_REG_VIRT_CTRL);
	if (!(ctl & GENERIC_TIMER_CTRL_IT_STAT)) {
		/* We got interrupt without status bit set.
		 * Looks like we are running on buggy hardware.
		 */
		vmm_printf("%s: suprious interrupt\n", __func__);
		return VMM_IRQ_NONE;
	}

	ctl |= GENERIC_TIMER_CTRL_IT_MASK;
	generic_timer_reg_write(GENERIC_TIMER_REG_VIRT_CTRL, ctl);

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu->is_normal) {
		/* We accidently got an interrupt meant for normal VCPU 
		 * that was previously running on this host CPU. 
		 */
		vmm_printf("%s: In orphan context (current VCPU=%s)\n",
			   __func__, vcpu->name);
		return VMM_IRQ_NONE;
	}

	cntx = arm_gentimer_context(vcpu);
	if (!cntx) {
		/* We accidently got an interrupt meant another normal VCPU */
		vmm_printf("%s: Invalid normal context (current VCPU=%s)\n",
			   __func__, vcpu->name);
		return VMM_IRQ_NONE;
	}

	virq = cntx->virt_timer_irq;
	if (virq == 0) {
		return VMM_IRQ_NONE;
	}

	rc = vmm_devemu_emulate_percpu_irq(vcpu->guest, virq, vcpu->subid, 0);
	if (rc) {
		vmm_printf("%s: Emulate VCPU=%s irq=%d level=0 failed\n",
			   __func__, vcpu->name, virq);
	}

	rc = vmm_devemu_emulate_percpu_irq(vcpu->guest, virq, vcpu->subid, 1);
	if (rc) {
		vmm_printf("%s: Emulate VCPU=%s irq=%d level=1 failed\n",
			   __func__, vcpu->name, virq);
	}

	return VMM_IRQ_HANDLED;
}

u64 generic_timer_wakeup_timeout(void)
{
	u32 vtval = 0, ptval = 0;
	u64 nsecs = 0;

	if (generic_timer_hz == 0) {
		return 0;
	}
	
	if (generic_timer_reg_read(GENERIC_TIMER_REG_PHYS_CTRL) &
						GENERIC_TIMER_CTRL_ENABLE) {
		ptval = generic_timer_reg_read(GENERIC_TIMER_REG_PHYS_TVAL);
	}
	
	if (generic_timer_reg_read(GENERIC_TIMER_REG_VIRT_CTRL) &
						GENERIC_TIMER_CTRL_ENABLE) {
		vtval = generic_timer_reg_read(GENERIC_TIMER_REG_VIRT_TVAL);
		
	}

	if ((ptval > 0) && (vtval > 0)) {
		nsecs = (ptval > vtval) ? vtval : ptval;
	} else {
		nsecs = (ptval > vtval) ? ptval : vtval;
	}

	if (nsecs) {
		if (generic_timer_hz == 100000000) {
			nsecs = nsecs * 10;
		} else {
			nsecs = 
			udiv64((nsecs * 1000000000), (u64)generic_timer_hz);
		}
	}

	return nsecs;
}

static int __cpuinit generic_timer_clockchip_init(struct vmm_devtree_node *node)
{
	int rc;
	u32 irq[3], num_irqs, val;
	struct vmm_clockchip *cc;

	/* Determine generic timer frequency */
	if (generic_timer_hz == 0) {
		rc =  vmm_devtree_clock_frequency(node, &generic_timer_hz);
		if (rc) {
			/* Use preconfigured counter frequency 
			 * in absence of dts node
			 */
			generic_timer_hz = 
				generic_timer_reg_read(GENERIC_TIMER_REG_FREQ);
		} else if (generic_timer_freq_writeable()) {
			/* Program the counter frequency as per the dts node */
			generic_timer_reg_write(GENERIC_TIMER_REG_FREQ, 
							generic_timer_hz);
		}
	}
	if (generic_timer_hz == 0) {
		return VMM_EFAIL;
	}

	/* Get hypervisor timer irq number */
	rc = vmm_devtree_irq_get(node, 
				 &irq[GENERIC_HYPERVISOR_TIMER], 
				 GENERIC_HYPERVISOR_TIMER);
	if (rc) {
		return rc;
	}

	/* Get physical timer irq number */
	rc = vmm_devtree_irq_get(node, 
				 &irq[GENERIC_PHYSICAL_TIMER], 
				 GENERIC_PHYSICAL_TIMER);
	if (rc) {
		return rc;
	}

	/* Get virtual timer irq number */
	rc = vmm_devtree_irq_get(node, 
				 &irq[GENERIC_VIRTUAL_TIMER], 
				 GENERIC_VIRTUAL_TIMER);
	if (rc) {
		return rc;
	}

	/* Number of generic timer irqs */
	num_irqs = vmm_devtree_irq_count(node);
	if (!num_irqs) {
		return VMM_EFAIL;
	}

	/* Ensure hypervisor timer is stopped */
	generic_timer_stop();

	/* Create generic hypervisor timer clockchip */
	cc = vmm_zalloc(sizeof(struct vmm_clockchip));
	if (!cc) {
		return VMM_EFAIL;
	}
	cc->name = "gen-hyp-timer";
	cc->hirq = irq[GENERIC_HYPERVISOR_TIMER];
	cc->rating = 400;
	cc->cpumask = vmm_cpumask_of(vmm_smp_processor_id());
	cc->features = VMM_CLOCKCHIP_FEAT_ONESHOT;
	vmm_clocks_calc_mult_shift(&cc->mult, &cc->shift, 
				   VMM_NSEC_PER_SEC, generic_timer_hz, 10);
	cc->min_delta_ns = vmm_clockchip_delta2ns(0xF, cc);
	cc->max_delta_ns = vmm_clockchip_delta2ns(0x7FFFFFFF, cc);
	cc->set_mode = &generic_timer_set_mode;
	cc->set_next_event = &generic_timer_set_next_event;
	cc->priv = NULL;

	/* Register hypervisor timer clockchip */
	rc = vmm_clockchip_register(cc);
	if (rc) {
		goto fail_free_cc;
	}

	/* Register irq handler for hypervisor timer */
	rc = vmm_host_irq_register(irq[GENERIC_HYPERVISOR_TIMER],
				   "gen-hyp-timer", 
				   &generic_hyp_timer_handler, cc);
	if (rc) {
		goto fail_unreg_cc;
	}

	if (num_irqs > 1) {
		/* Register irq handler for physical timer */
		rc = vmm_host_irq_register(irq[GENERIC_PHYSICAL_TIMER],
					   "gen-phys-timer",
					   &generic_phys_timer_handler,
					   NULL);
		if (rc) {
			goto fail_unreg_htimer;
		}
	}

	if (num_irqs > 2) {
		/* Register irq handler for virtual timer */
		rc = vmm_host_irq_register(irq[GENERIC_VIRTUAL_TIMER],
					   "gen-virt-timer",
					   &generic_virt_timer_handler,
					   NULL);
		if (rc) {
			goto fail_unreg_ptimer;
		}
	}

	if (num_irqs > 1) {
		val = generic_timer_reg_read(GENERIC_TIMER_REG_HCTL);
		val |= GENERIC_TIMER_HCTL_KERN_PCNT_EN;
		val |= GENERIC_TIMER_HCTL_KERN_PTMR_EN;
		generic_timer_reg_write(GENERIC_TIMER_REG_HCTL, val);
	}

	return VMM_OK;

fail_unreg_ptimer:
	if (num_irqs > 1) {
		vmm_host_irq_unregister(irq[GENERIC_PHYSICAL_TIMER],
					&generic_phys_timer_handler);
	}
fail_unreg_htimer:
	vmm_host_irq_unregister(irq[GENERIC_HYPERVISOR_TIMER],
					&generic_hyp_timer_handler);
fail_unreg_cc:
	vmm_clockchip_register(cc);
fail_free_cc:
	vmm_free(cc);
	return rc;
}
VMM_CLOCKCHIP_INIT_DECLARE(gtv7clkchip, "arm,armv7-timer", generic_timer_clockchip_init);
VMM_CLOCKCHIP_INIT_DECLARE(gtv8clkchip, "arm,armv8-timer", generic_timer_clockchip_init);

int generic_timer_vcpu_context_init(void *vcpu_ptr,
				    void **context,
				    u32 phys_irq, u32 virt_irq)

{
	struct generic_timer_context *cntx;

	if (!context || !vcpu_ptr) {
		return VMM_EINVALID;
	}

	if (!(*context)) {
		*context = vmm_zalloc(sizeof(*cntx));
		if (!(*context)) {
			return VMM_ENOMEM;
		}
	}

	cntx = *context;
	cntx->vcpu = vcpu_ptr;
	cntx->cntpctl = GENERIC_TIMER_CTRL_IT_MASK;
	cntx->cntvctl = GENERIC_TIMER_CTRL_IT_MASK;
	cntx->cntpcval = 0;
	cntx->cntvcval = 0;
	cntx->cntkctl = 0;
	cntx->cntvoff = 0;
	cntx->phys_timer_irq = phys_irq;
	cntx->virt_timer_irq = virt_irq;

	return VMM_OK;
}

int generic_timer_vcpu_context_deinit(void *vcpu_ptr, void **context)
{
	if (!context || !vcpu_ptr) {
		return VMM_EINVALID;
	}
	if (!(*context)) {
		return VMM_EINVALID;
	}

	vmm_free(*context);

	return VMM_OK;
}

void generic_timer_vcpu_context_save(void *context)
{
	struct generic_timer_context *cntx = context;

	if (!cntx) {
		return;
	}

	cntx->cntpctl = generic_timer_reg_read(GENERIC_TIMER_REG_PHYS_CTRL);
	cntx->cntvctl = generic_timer_reg_read(GENERIC_TIMER_REG_VIRT_CTRL);
	cntx->cntpcval = generic_timer_reg_read64(GENERIC_TIMER_REG_PHYS_CVAL);
	cntx->cntvcval = generic_timer_reg_read64(GENERIC_TIMER_REG_VIRT_CVAL);
	cntx->cntkctl = generic_timer_reg_read(GENERIC_TIMER_REG_KCTL);
	generic_timer_reg_write(GENERIC_TIMER_REG_PHYS_CTRL, 
				GENERIC_TIMER_CTRL_IT_MASK);
	generic_timer_reg_write(GENERIC_TIMER_REG_VIRT_CTRL, 
				GENERIC_TIMER_CTRL_IT_MASK);
}

void generic_timer_vcpu_context_restore(void *context)
{
	struct generic_timer_context *cntx = context;

	if (!cntx) {
		return;
	}

	if (!cntx->cntvoff) {
		cntx->cntvoff =
			vmm_manager_guest_reset_timestamp(cntx->vcpu->guest);
		cntx->cntvoff = cntx->cntvoff * generic_timer_hz;
		cntx->cntvoff = udiv64(cntx->cntvoff, 1000000000ULL);
	}

	generic_timer_reg_write64(GENERIC_TIMER_REG_VIRT_OFF, cntx->cntvoff);
	generic_timer_reg_write(GENERIC_TIMER_REG_KCTL, cntx->cntkctl);
	generic_timer_reg_write64(GENERIC_TIMER_REG_PHYS_CVAL, cntx->cntpcval);
	generic_timer_reg_write64(GENERIC_TIMER_REG_VIRT_CVAL, cntx->cntvcval);
	generic_timer_reg_write(GENERIC_TIMER_REG_PHYS_CTRL, cntx->cntpctl);
	generic_timer_reg_write(GENERIC_TIMER_REG_VIRT_CTRL, cntx->cntvctl);
}

