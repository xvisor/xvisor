/**
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
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
 * @file cpu_vcpu_timer.c
 * @author Atish Patra (atish.patra@wdc.com)
 * @brief RISC-V timer event
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_limits.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_vcpu_irq.h>

#include <cpu_hwcap.h>
#include <cpu_vcpu_timer.h>
#include <riscv_encoding.h>

struct cpu_vcpu_timer {
	u64 next_cycle;
	struct vmm_timer_event time_ev;
};

static inline u64 cpu_vcpu_timer_delta(struct vmm_vcpu *vcpu,
					bool nested_virt)
{
	u64 ndelta = 0;

	if (nested_virt) {
		ndelta = riscv_nested_priv(vcpu)->htimedelta;
#ifdef CONFIG_32BIT
		ndelta |= ((u64)riscv_nested_priv(vcpu)->htimedeltah) << 32;
#endif
	}

	return riscv_guest_priv(vcpu->guest)->time_delta + ndelta;
}

static void cpu_vcpu_timer_expired(struct vmm_timer_event *ev)
{
	struct vmm_vcpu *vcpu = ev->priv;
	struct cpu_vcpu_timer *t = riscv_timer_priv(vcpu);

	BUG_ON(!t);

	if (riscv_isa_extension_available(riscv_priv(vcpu)->isa, SSTC)) {
		vmm_vcpu_irq_wait_resume(vcpu);
	} else {
		vmm_vcpu_irq_assert(vcpu, IRQ_VS_TIMER, 0x0);
	}
}

void cpu_vcpu_timer_start(struct vmm_vcpu *vcpu, u64 next_cycle)
{
	u64 delta_ns;
	struct cpu_vcpu_timer *t = riscv_timer_priv(vcpu);

	/* Save the next timer tick value */
	t->next_cycle = next_cycle;

	/* If Sstc available then simply update vstimecmp CSRs */
	if (riscv_isa_extension_available(riscv_priv(vcpu)->isa, SSTC)) {
#ifdef CONFIG_32BIT
		csr_write(CSR_VSTIMECMP, (u32)t->next_cycle);
		csr_write(CSR_VSTIMECMPH, (u32)(t->next_cycle >> 32));
#else
		csr_write(CSR_VSTIMECMP, t->next_cycle);
#endif
		return;
	}

	/* Stop the timer when next timer tick equals U64_MAX */
	if (next_cycle == U64_MAX) {
		vmm_timer_event_stop(&t->time_ev);
		vmm_vcpu_irq_clear(vcpu, IRQ_VS_TIMER);
		return;
	}

	/*
	 * In RISC-V, we should clear the timer pending bit before
	 * programming next one.
	 */
	vmm_vcpu_irq_clear(vcpu, IRQ_VS_TIMER);

	/* Start the timer event */
	next_cycle -= cpu_vcpu_timer_delta(vcpu, riscv_nested_virt(vcpu));
	delta_ns = vmm_timer_delta_cycles_to_ns(next_cycle);
	vmm_timer_event_start(&t->time_ev, delta_ns);
}

void cpu_vcpu_timer_delta_update(struct vmm_vcpu *vcpu, bool nested_virt)
{
	u64 current_delta, new_delta = 0;
	struct cpu_vcpu_timer *t = riscv_timer_priv(vcpu);

	current_delta = cpu_vcpu_timer_delta(vcpu, riscv_nested_virt(vcpu));
	new_delta = cpu_vcpu_timer_delta(vcpu, nested_virt);

#ifdef CONFIG_32BIT
	csr_write(CSR_HTIMEDELTA, (u32)new_delta);
	csr_write(CSR_HTIMEDELTAH, (u32)(new_delta >> 32));
#else
	csr_write(CSR_HTIMEDELTA, new_delta);
#endif

	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, SSTC)) {
		return;
	}

	t->next_cycle += new_delta - current_delta;
#ifdef CONFIG_32BIT
	csr_write(CSR_VSTIMECMP, (u32)t->next_cycle);
	csr_write(CSR_VSTIMECMPH, (u32)(t->next_cycle >> 32));
#else
	csr_write(CSR_VSTIMECMP, t->next_cycle);
#endif
}

void cpu_vcpu_timer_save(struct vmm_vcpu *vcpu)
{
	u64 delta_ns;
	struct cpu_vcpu_timer *t;

	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, SSTC)) {
		return;
	}

	t = riscv_timer_priv(vcpu);

#ifdef CONFIG_32BIT
	t->next_cycle = csr_swap(CSR_VSTIMECMP, -1UL);
	t->next_cycle |= (u64)csr_swap(CSR_VSTIMECMPH, -1UL) << 32;
#else
	t->next_cycle = csr_swap(CSR_VSTIMECMP, -1UL);
#endif
	if (t->next_cycle == U64_MAX) {
		return;
	}

	delta_ns = t->next_cycle -
		   cpu_vcpu_timer_delta(vcpu, riscv_nested_virt(vcpu));
	delta_ns = vmm_timer_delta_cycles_to_ns(delta_ns);
	vmm_timer_event_start(&t->time_ev, delta_ns);
}

void cpu_vcpu_timer_restore(struct vmm_vcpu *vcpu)
{
	u64 time_delta = cpu_vcpu_timer_delta(vcpu, riscv_nested_virt(vcpu));
	struct cpu_vcpu_timer *t = riscv_timer_priv(vcpu);

#ifdef CONFIG_32BIT
	csr_write(CSR_HTIMEDELTA, (u32)time_delta);
	csr_write(CSR_HTIMEDELTAH, (u32)(time_delta >> 32));
#else
	csr_write(CSR_HTIMEDELTA, time_delta);
#endif

	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, SSTC)) {
		return;
	}

	vmm_timer_event_stop(&t->time_ev);

#ifdef CONFIG_32BIT
	csr_write(CSR_VSTIMECMP, (u32)t->next_cycle);
	csr_write(CSR_VSTIMECMPH, (u32)(t->next_cycle >> 32));
#else
	csr_write(CSR_VSTIMECMP, t->next_cycle);
#endif
}

int cpu_vcpu_timer_init(struct vmm_vcpu *vcpu, void **timer)
{
	struct cpu_vcpu_timer *t;

	if (!vcpu || !timer)
		return VMM_EINVALID;

	if (!(*timer)) {
		*timer = vmm_zalloc(sizeof(struct cpu_vcpu_timer));
		if (!(*timer))
			return VMM_ENOMEM;
		t = *timer;
		INIT_TIMER_EVENT(&t->time_ev, cpu_vcpu_timer_expired, vcpu);
	} else {
		t = *timer;
	}

	t->next_cycle = U64_MAX;
	vmm_timer_event_stop(&t->time_ev);

	if (riscv_isa_extension_available(riscv_priv(vcpu)->isa, SSTC)) {
		riscv_priv(vcpu)->henvcfg |= ENVCFG_STCE;
	}

	return VMM_OK;
}

int cpu_vcpu_timer_deinit(struct vmm_vcpu *vcpu, void **timer)
{
	struct cpu_vcpu_timer *t;

	if (!vcpu || !timer || !(*timer))
		return VMM_EINVALID;
	t = *timer;

	vmm_timer_event_stop(&t->time_ev);
	vmm_free(t);

	return VMM_OK;
}
