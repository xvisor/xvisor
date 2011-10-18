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
 * @file vmm_cpu.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for CPU functions required by VMM
 */
#ifndef _VMM_CPU_H__
#define _VMM_CPU_H__

#include <vmm_types.h>
#include <vmm_manager.h>
#include <cpu_interrupts.h>
#include <cpu_timer.h>
#include <cpu_locks.h>

/** CPU related functions required by VMM core */
int vmm_cpu_early_init(void);
int vmm_cpu_final_init(void);

/** Register related functions required by VMM core */
int vmm_vcpu_regs_init(vmm_vcpu_t *vcpu);
void vmm_vcpu_regs_switch(vmm_vcpu_t *tvcpu, vmm_vcpu_t *vcpu,
			  vmm_user_regs_t *regs);
void vmm_vcpu_regs_dump(vmm_vcpu_t *vcpu);

/** Host address space management related functions */
/** Host address space related functions required by VMM core */
int vmm_cpu_aspace_init(physical_addr_t * resv_pa, 
			virtual_addr_t * resv_va,
			virtual_size_t * resv_sz);
int vmm_cpu_aspace_map(virtual_addr_t va, 
			virtual_size_t sz, 
			physical_addr_t pa,
			u32 mem_flags);
int vmm_cpu_aspace_unmap(virtual_addr_t va, 
			 virtual_size_t sz);

/** Interrupt related functions required by VMM core */
int vmm_cpu_irq_setup(void);
void vmm_cpu_irq_enable(void);
void vmm_cpu_irq_disable(void);
irq_flags_t vmm_cpu_irq_save(void);
void vmm_cpu_irq_restore(irq_flags_t flags);
s32 vmm_vcpu_irq_execute(vmm_vcpu_t *vcpu,vmm_user_regs_t *regs,
			u32 interrupt_no,u32 reason);
irq_flags_t vmm_cpu_irq_save(void);
void vmm_interrupts_restore(irq_flags_t flags);
s32 vmm_vcpu_irq_execute(vmm_vcpu_t *vcpu,vmm_user_regs_t *regs,u32 interrupt_no,u32 reason);
u32 vmm_vcpu_irq_priority(vmm_vcpu_t * vcpu, u32 irq_no);
u32 vmm_vcpu_irq_count(vmm_vcpu_t * vcpu);

/** Hrtimer related functions. */
u64 vmm_cpu_clocksource_cycles(void);
int vmm_cpu_clockevent_start(u64 ticks_nsecs);
int vmm_cpu_clockevent_setup(void);
int vmm_cpu_clockevent_shutdown(void);
u64 vmm_cpu_clocksource_mask(void);
u32 vmm_cpu_clocksource_mult(void);
u32 vmm_cpu_clocksource_shift(void);
int vmm_cpu_clockevent_init(void);
int vmm_cpu_clocksource_init(void);

/** Timer related functions required by VMM core */
void vmm_cpu_timer_enable(void);
void vmm_cpu_timer_disable(void);
int vmm_cpu_timer_setup(u64 tick_nsecs);
int vmm_cpu_timer_init(void);

/** Atomic Operations and spinlock */
void vmm_cpu_atomic_inc(atomic_t *atom);
void vmm_cpu_atomic_dec(atomic_t *atom);
void vmm_cpu_spin_lock(vmm_cpu_spinlock_t *lock);
void vmm_cpu_spin_unlock(vmm_cpu_spinlock_t *lock);
irq_flags_t vmm_cpu_spin_lock_irqsave(vmm_cpu_spinlock_t *lock);
void vmm_cpu_spin_unlock_irqrestore(vmm_cpu_spinlock_t *lock, 
				irq_flags_t flags);

u32 get_vcpu_word(u32 *);

#endif
