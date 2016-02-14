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
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for hypervisor scheduler
 */
#ifndef _VMM_SCHEDULER_H__
#define _VMM_SCHEDULER_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Disable pre-emption of current VCPU */
void vmm_scheduler_preempt_disable(void);

/** Enable pre-emption of current VCPU */
void vmm_scheduler_preempt_enable(void);

/** Preempt Orphan VCPU (Must be called from somewhere) */
void vmm_scheduler_preempt_orphan(arch_regs_t *regs);

/** Force re-scheduling on given host CPU */
int vmm_scheduler_force_resched(u32 hcpu);

/** Change the vcpu state
 *  (Do not call this function directly.)
 *  (Always prefer vmm_manager_vcpu_xxx() APIs for vcpu state change.)
 */
int vmm_scheduler_state_change(struct vmm_vcpu *vcpu, u32 new_state);

/** Retrive host CPU assigned to given VCPU */
int vmm_scheduler_get_hcpu(struct vmm_vcpu *vcpu, u32 *hcpu);

/** Check host CPU assigned to given VCPU is current host CPU */
bool vmm_scheduler_check_current_hcpu(struct vmm_vcpu *vcpu);

/** Update host CPU assigned to given VCPU */
int vmm_scheduler_set_hcpu(struct vmm_vcpu *vcpu, u32 hcpu);

/** Enter IRQ Context (Must be called from somewhere) */
void vmm_scheduler_irq_enter(arch_regs_t *regs, bool vcpu_context);

/** Exit IRQ Context (Must be called from somewhere) */
void vmm_scheduler_irq_exit(arch_regs_t *regs);

/** Check whether we are in IRQ context */
bool vmm_scheduler_irq_context(void);

/** Check whether we are in Orphan VCPU context */
bool vmm_scheduler_orphan_context(void);

/** Check whether we are in Normal VCPU context */
bool vmm_scheduler_normal_context(void);

/** Count number ready VCPUs with given priority on a host CPU */
u32 vmm_scheduler_ready_count(u32 hcpu, u8 priority);

/** Get scheduler sampling period in nanosecs */
u64 vmm_scheduler_get_sample_period(u32 hcpu);

/** Set scheduler sampling period in nanosecs */
void vmm_scheduler_set_sample_period(u32 hcpu, u64 period);

/** Last sampled irq time in nanosecs for given host CPU */
u64 vmm_scheduler_irq_time(u32 hcpu);

/** Last sampled idle time in nanosecs for given host CPU */
u64 vmm_scheduler_idle_time(u32 hcpu);

/** Retrive idle vcpu for given host CPU */
struct vmm_vcpu *vmm_scheduler_idle_vcpu(u32 hcpu);

/** Retrive current vcpu number */
struct vmm_vcpu *vmm_scheduler_current_vcpu(void);

/** Retrive current guest number */
struct vmm_guest *vmm_scheduler_current_guest(void);

/** Yield current vcpu (Should not be called in IRQ context) */
void vmm_scheduler_yield(void);

/** Initialize scheduler */
int vmm_scheduler_init(void);

#endif
