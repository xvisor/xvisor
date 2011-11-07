/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_scheduler.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for hypervisor scheduler
 */
#ifndef _VMM_SCHEDULER_H__
#define _VMM_SCHEDULER_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_manager.h>
#include <vmm_timer.h>

typedef struct vmm_scheduler_ctrl vmm_scheduler_ctrl_t;

/** Control structure for Scheduler */
struct vmm_scheduler_ctrl {
	vmm_spinlock_t lock;
	vmm_vcpu_t *idle_vcpu;
	u8 idle_vcpu_stack[1024];
	s32 vcpu_current;
	bool irq_context;
	vmm_timer_event_t * ev;
};

/** Enter IRQ Context (Must be called from somewhere) */
void vmm_scheduler_irq_enter(vmm_user_regs_t * regs, bool vcpu_context);

/** Exit IRQ Context (Must be called from somewhere) */
void vmm_scheduler_irq_exit(vmm_user_regs_t * regs);

/** Check whether we are in IRQ context */
bool vmm_scheduler_irq_context(void);

/** Notify Change in vcpu state 
 *  (Must be called before actually changing the state) 
 */
int vmm_scheduler_notify_state_change(vmm_vcpu_t * vcpu, u32 new_state);

/** Retrive current vcpu number */
vmm_vcpu_t * vmm_scheduler_current_vcpu(void);

/** Retrive current guest number */
vmm_guest_t * vmm_scheduler_current_guest(void);

/** Disable pre-emption */
void vmm_scheduler_preempt_disable(void);

/** Enable pre-emption */
void vmm_scheduler_preempt_enable(void);

/** Yield current vcpu (Should not be called in IRQ context) */
void vmm_scheduler_yield(void);

/** Initialize scheduler */
int vmm_scheduler_init(void);

#endif
