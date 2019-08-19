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
 * @file cpu_vcpu_timer.h
 * @author Atish Patra (atish.patra@wdc.com)
 * @brief generic timer for vcpu
 */

#ifndef _CPU_VCPU_TIMER_H__
#define _CPU_VCPU_TIMER_H__

#include <vmm_types.h>
#include <vmm_manager.h>
#include <vmm_timer.h>

struct riscv_timer_event {
	struct vmm_timer_event time_ev;
};

void riscv_timer_event_start(struct vmm_vcpu *vcpu, u64 next_cycle);
int riscv_timer_event_init(struct vmm_vcpu *vcpu, void **timer_event);
int riscv_timer_event_deinit(struct vmm_vcpu *vcpu, void **timer_event);

#endif
