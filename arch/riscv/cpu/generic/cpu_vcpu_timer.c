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
#include <cpu_vcpu_timer.h>

#include <riscv_encoding.h>

struct cpu_vcpu_timer {
	u64 next_cycle;
	struct vmm_timer_event time_ev;
};

static void cpu_vcpu_timer_expired(struct vmm_timer_event *ev)
{
	struct vmm_vcpu *vcpu = ev->priv;
	struct cpu_vcpu_timer *t = riscv_timer_priv(vcpu);

	BUG_ON(!t);
	vmm_vcpu_irq_assert(vcpu, IRQ_VS_TIMER, 0x0);
}

void cpu_vcpu_timer_start(struct vmm_vcpu *vcpu, u64 next_cycle)
{
	u64 delta_ns;
	struct cpu_vcpu_timer *t = riscv_timer_priv(vcpu);

	/* Save the next timer tick value */
	t->next_cycle = next_cycle;

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
	next_cycle -= riscv_guest_priv(vcpu->guest)->time_delta;
	delta_ns = vmm_timer_delta_cycles_to_ns(next_cycle);
	vmm_timer_event_start(&t->time_ev, delta_ns);
}

void cpu_vcpu_timer_delta_update(struct vmm_vcpu *vcpu, bool nested_virt)
{
	u64 vtdelta, tdelta = riscv_guest_priv(vcpu->guest)->time_delta;

	if (nested_virt) {
		vtdelta = riscv_nested_priv(vcpu)->htimedelta;
#ifndef CONFIG_64BIT
		vtdelta |= ((u64)riscv_nested_priv(vcpu)->htimedeltah) << 32;
#endif
		tdelta += vtdelta;
	}

#ifdef CONFIG_64BIT
	csr_write(CSR_HTIMEDELTA, tdelta);
#else
	csr_write(CSR_HTIMEDELTA, (u32)tdelta);
	csr_write(CSR_HTIMEDELTAH, (u32)(tdelta >> 32));
#endif
}

void cpu_vcpu_timer_save(struct vmm_vcpu *vcpu)
{
}

void cpu_vcpu_timer_restore(struct vmm_vcpu *vcpu)
{
	cpu_vcpu_timer_delta_update(vcpu, riscv_nested_virt(vcpu));
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

	vmm_timer_event_stop(&t->time_ev);

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
