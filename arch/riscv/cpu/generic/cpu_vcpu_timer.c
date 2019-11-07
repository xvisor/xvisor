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
#include <vmm_stdio.h>
#include <vmm_vcpu_irq.h>
#include <cpu_vcpu_timer.h>

#include <riscv_encoding.h>

#define TIMER_EVENT_THRESHOLD_NS		1000

static void riscv_timer_event_expired(struct vmm_timer_event *ev)
{
	struct vmm_vcpu *vcpu = ev->priv;
	struct riscv_timer_event *tevent = riscv_timer_priv(vcpu);

	BUG_ON(!tevent);
	vmm_vcpu_irq_assert(vcpu, IRQ_VS_TIMER, 0x0);
}

void riscv_timer_event_start(struct vmm_vcpu *vcpu, u64 next_cycle)
{
	u64 delta_ns;
	struct riscv_timer_event *tevent = riscv_timer_priv(vcpu);;

	next_cycle -= riscv_guest_priv(vcpu->guest)->time_delta;
	delta_ns = vmm_timer_delta_cycles_to_ns(next_cycle);

	/*
	 * In RISC-V, we should clear the timer pending bit before
	 * programming next one.
	 */
	vmm_vcpu_irq_clear(vcpu, IRQ_VS_TIMER);

	/* No point in programming a timer for 1us */
	if (delta_ns <= TIMER_EVENT_THRESHOLD_NS)
		vmm_vcpu_irq_assert(vcpu, IRQ_VS_TIMER, 0x0);
	else
		vmm_timer_event_start(&tevent->time_ev, delta_ns);
}

int riscv_timer_event_init(struct vmm_vcpu *vcpu, void **timer_event)
{
	struct riscv_timer_event *tevent;

	if (!vcpu || !timer_event)
		return VMM_EINVALID;

	if (!(*timer_event)) {
		*timer_event = vmm_zalloc(sizeof(struct riscv_timer_event));
		if (!(*timer_event))
			return VMM_ENOMEM;
		tevent = *timer_event;
		INIT_TIMER_EVENT(&tevent->time_ev, riscv_timer_event_expired,
				 vcpu);
	} else {
		tevent = *timer_event;
	}

	vmm_timer_event_stop(&tevent->time_ev);

	return VMM_OK;
}

int riscv_timer_event_deinit(struct vmm_vcpu *vcpu, void **timer_event)
{
	struct riscv_timer_event *tevent;

	if (!vcpu || !timer_event)
		return VMM_EINVALID;

	if (!(*timer_event))
		return VMM_EINVALID;
	tevent = *timer_event;

	vmm_timer_event_stop(&tevent->time_ev);
	vmm_free(tevent);

	return VMM_OK;
}
