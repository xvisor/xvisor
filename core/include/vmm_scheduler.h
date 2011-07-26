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
 * @brief header file for vmm scheduler
 */
#ifndef _VMM_SCHEDULER_H__
#define _VMM_SCHEDULER_H__

#include <vmm_types.h>
#include <vmm_guest.h>
#include <vmm_spinlocks.h>

typedef struct vmm_scheduler_ctrl vmm_scheduler_ctrl_t;

/** Control structure for Scheduler */
struct vmm_scheduler_ctrl {
	s32 vcpu_current;
	u32 scheduler_count;
	u32 tick_usecs;

	vmm_spinlock_t lock;
	u32 max_vcpu_count;
	u32 max_guest_count;
	u32 vcpu_count;
	u32 guest_count;
	vmm_vcpu_t *vcpu_array;
	vmm_guest_t *guest_array;
	struct dlist orphan_vcpu_list;
	struct dlist guest_list;
};

/** Scheduler tick handler (Must be called from somewhere) */
void vmm_scheduler_tick(vmm_user_regs_t * regs);

/** Schedule next vcpu */
void vmm_scheduler_next(vmm_user_regs_t * regs);

/** Retrive current vcpu number */
vmm_vcpu_t * vmm_scheduler_current_vcpu(void);

/** Retrive current guest number */
vmm_guest_t * vmm_scheduler_current_guest(void);

/** Disable pre-emption */
void vmm_scheduler_preempt_disable(void);

/** Enable pre-emption */
void vmm_scheduler_preempt_enable(void);

/** Start scheduler */
void vmm_scheduler_start(void);

/** Stop scheduler */
void vmm_scheduler_stop(void);

/** Get scheduler tick delay in micorseconds */
u32 vmm_scheduler_tick_usecs(void);

/** Number of vcpus (thread + normal) */
u32 vmm_scheduler_vcpu_count(void);

/** Retrive vcpu */
vmm_vcpu_t * vmm_scheduler_vcpu(s32 vcpu_no);

/** Number of vcpus belonging to a given guest */
u32 vmm_scheduler_guest_vcpu_count(vmm_guest_t *guest);

/** Kick a vcpu out of reset state */
int vmm_scheduler_vcpu_kick(vmm_vcpu_t * vcpu);

/** Pause a vcpu */
int vmm_scheduler_vcpu_pause(vmm_vcpu_t * vcpu);

/** Resume a vcpu */
int vmm_scheduler_vcpu_resume(vmm_vcpu_t * vcpu);

/** Halt a vcpu */
int vmm_scheduler_vcpu_halt(vmm_vcpu_t * vcpu);

/** Dump registers of a vcpu */
int vmm_scheduler_vcpu_dumpreg(vmm_vcpu_t * vcpu);

/** Create an orphan vcpu */
vmm_vcpu_t * vmm_scheduler_vcpu_orphan_create(const char *name,
					      virtual_addr_t start_pc,
					      u32 tick_count,
					      vmm_vcpu_tick_t tick_func);

/** Destroy an orphan vcpu */
int vmm_scheduler_vcpu_orphan_destroy(vmm_vcpu_t * vcpu);

/** Number of guests */
u32 vmm_scheduler_guest_count(void);

/** Retrive guest */
vmm_guest_t * vmm_scheduler_guest(s32 guest_no);

/** Retrive vcpu belonging to a given guest */
vmm_vcpu_t * vmm_scheduler_guest_vcpu(vmm_guest_t *guest, s32 index);

/** Find the relative index of a vcpu under a given guest */
int vmm_scheduler_guest_vcpu_index(vmm_guest_t *guest, vmm_vcpu_t *vcpu);

/** Kick a guest out of reset state */
int vmm_scheduler_guest_kick(vmm_guest_t * guest);

/** Pause a guest */
int vmm_scheduler_guest_pause(vmm_guest_t * guest);

/** Resume a guest */
int vmm_scheduler_guest_resume(vmm_guest_t * guest);

/** Halt a guest */
int vmm_scheduler_guest_halt(vmm_guest_t * guest);

/** Dump registers of a guest */
int vmm_scheduler_guest_dumpreg(vmm_guest_t * guest);

/** Create a guest based on device tree configuration */
vmm_guest_t * vmm_scheduler_guest_create(vmm_devtree_node_t * node);

/** Destroy a guest */
int vmm_scheduler_guest_destroy(vmm_guest_t * guest);

/** Initialize scheduler */
int vmm_scheduler_init(void);

#endif
