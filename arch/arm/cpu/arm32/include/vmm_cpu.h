/**
 * Copyright (c) 2011 Anup Patel.
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

/** CPU functions required by VMM core */
int vmm_cpu_early_init(void);
int vmm_cpu_final_init(void);

/** Register functions required by VMM core */
int vmm_vcpu_regs_init(vmm_vcpu_t * vcpu);
void vmm_vcpu_regs_switch(vmm_vcpu_t * tvcpu,
			  vmm_vcpu_t * vcpu, 
			  vmm_user_regs_t * regs);
void vmm_vcpu_regs_dump(vmm_vcpu_t * vcpu);
void vmm_vcpu_stat_dump(vmm_vcpu_t * vcpu);

/** Address space functions required by VMM core */
int vmm_cpu_aspace_init(physical_addr_t * resv_pa, 
			virtual_addr_t * resv_va,
			virtual_size_t * resv_sz);
int vmm_cpu_aspace_map(virtual_addr_t va, 
			virtual_size_t sz, 
			physical_addr_t pa,
			u32 mem_flags);
int vmm_cpu_aspace_unmap(virtual_addr_t va, 
			 virtual_size_t sz);
int vmm_cpu_aspace_va2pa(virtual_addr_t va, 
			 physical_addr_t * pa);
virtual_addr_t vmm_cpu_code_vaddr_start(void);
physical_addr_t vmm_cpu_code_paddr_start(void);
virtual_size_t vmm_cpu_code_size(void);

/** CPU Interrupt functions required by VMM core */
int vmm_cpu_irq_setup(void);
void vmm_cpu_irq_enable(void);
void vmm_cpu_irq_disable(void);
irq_flags_t vmm_cpu_irq_save(void);
void vmm_cpu_irq_restore(irq_flags_t flags);

/** VCPU Interrupt functions required by VMM core */
u32 vmm_vcpu_irq_count(vmm_vcpu_t * vcpu);
u32 vmm_vcpu_irq_priority(vmm_vcpu_t * vcpu, u32 irq_no);
int vmm_vcpu_irq_execute(vmm_vcpu_t * vcpu, 
			 vmm_user_regs_t * regs,
			 u32 irq_no, u32 reason);

/** Timer functions required by VMM core */
int vmm_cpu_clockevent_start(u64 tick_nsecs);
int vmm_cpu_clockevent_expire(void);
int vmm_cpu_clockevent_stop(void);
int vmm_cpu_clockevent_init(void);
u64 vmm_cpu_clocksource_cycles(void);
u64 vmm_cpu_clocksource_mask(void);
u32 vmm_cpu_clocksource_mult(void);
u32 vmm_cpu_clocksource_shift(void);
int vmm_cpu_clocksource_init(void);

#if defined(CONFIG_SMP)
/** Spinlock functions required by VMM core */
bool vmm_cpu_spin_lock_check(spinlock_t * lock);
void vmm_cpu_spin_lock(spinlock_t * lock);
void vmm_cpu_spin_unlock(spinlock_t * lock);
#endif

/** Atomic operations required by VMM core */
void vmm_cpu_atomic_inc(atomic_t * atom);
void vmm_cpu_atomic_dec(atomic_t * atom);
int vmm_cpu_atomic_testnset(atomic_t * atom, u32 test, u32 val);

/** Module functions required by VMM core */
extern u8 _modtbl_start;
extern u8 _modtbl_end;
static inline virtual_addr_t vmm_modtbl_vaddr(void)
{
	return (virtual_addr_t) &_modtbl_start;
}
static inline virtual_size_t vmm_modtbl_size(void)
{
	return (virtual_size_t) (&_modtbl_end - &_modtbl_start);
}

/** Init section functions required by VMM core */
extern u8 _init_text_start;
extern u8 _init_text_end;
static inline virtual_addr_t vmm_init_text_vaddr(void)
{
	return (virtual_addr_t) &_init_text_start;
}
static inline virtual_size_t vmm_init_text_size(void)
{
	return (virtual_size_t) (&_init_text_end - &_init_text_start);
}

#endif
