/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file arch_cpu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for CPU functions required by VMM
 */
#ifndef _ARCH_CPU_H__
#define _ARCH_CPU_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** CPU functions required by VMM core */
int arch_cpu_early_init(void);
int arch_cpu_final_init(void);

/** Register functions required by VMM core */
int arch_vcpu_regs_init(struct vmm_vcpu * vcpu);
int arch_vcpu_regs_deinit(struct vmm_vcpu * vcpu);
void arch_vcpu_regs_switch(struct vmm_vcpu * tvcpu,
			  struct vmm_vcpu * vcpu, 
			  arch_regs_t * regs);
void arch_vcpu_regs_dump(struct vmm_vcpu * vcpu);
void arch_vcpu_stat_dump(struct vmm_vcpu * vcpu);

/** Address space functions required by VMM core */
int arch_cpu_aspace_init(physical_addr_t * core_resv_pa, 
			 virtual_addr_t * core_resv_va,
			 virtual_size_t * core_resv_sz,
			 physical_addr_t * arch_resv_pa,
			 virtual_addr_t * arch_resv_va,
			 virtual_size_t * arch_resv_sz);
int arch_cpu_aspace_map(virtual_addr_t va, 
			virtual_size_t sz, 
			physical_addr_t pa,
			u32 mem_flags);
int arch_cpu_aspace_unmap(virtual_addr_t va, 
			 virtual_size_t sz);
int arch_cpu_aspace_va2pa(virtual_addr_t va, 
			 physical_addr_t * pa);
virtual_addr_t arch_code_vaddr_start(void);
physical_addr_t arch_code_paddr_start(void);
virtual_size_t arch_code_size(void);

/** CPU Interrupt functions required by VMM core */
int arch_cpu_irq_setup(void);
void arch_cpu_irq_enable(void);
void arch_cpu_irq_disable(void);
irq_flags_t arch_cpu_irq_save(void);
void arch_cpu_irq_restore(irq_flags_t flags);
void arch_cpu_wait_for_irq(void);

/** VCPU Interrupt functions required by VMM core */
u32 arch_vcpu_irq_count(struct vmm_vcpu * vcpu);
u32 arch_vcpu_irq_priority(struct vmm_vcpu * vcpu, u32 irq_no);
int arch_vcpu_irq_execute(struct vmm_vcpu * vcpu, 
			 arch_regs_t * regs,
			 u32 irq_no, u32 reason);

/** Timer functions required by VMM core */
int arch_cpu_clockevent_start(u64 tick_nsecs);
int arch_cpu_clockevent_expire(void);
int arch_cpu_clockevent_stop(void);
int arch_cpu_clockevent_init(void);
u64 arch_cpu_clocksource_cycles(void);
u64 arch_cpu_clocksource_mask(void);
u32 arch_cpu_clocksource_mult(void);
u32 arch_cpu_clocksource_shift(void);
int arch_cpu_clocksource_init(void);

#if defined(CONFIG_SMP)
/** Spinlock functions required by VMM core */
bool arch_cpu_spin_lock_check(spinlock_t * lock);
void arch_cpu_spin_lock(spinlock_t * lock);
void arch_cpu_spin_unlock(spinlock_t * lock);
#endif

/** Atomic operations required by VMM core */
long arch_cpu_atomic_read(atomic_t * atom);
void arch_cpu_atomic_write(atomic_t * atom, long value);
void arch_cpu_atomic_add(atomic_t * atom, long value);
long arch_cpu_atomic_add_return(atomic_t * atom, long value);
void arch_cpu_atomic_sub(atomic_t * atom, long value);
long arch_cpu_atomic_sub_return(atomic_t * atom, long value);
bool arch_cpu_atomic_testnset(atomic_t * atom, long test, long val);

/** Module functions required by VMM core */
extern u8 _modtbl_start;
extern u8 _modtbl_end;
static inline virtual_addr_t arch_modtbl_vaddr(void)
{
	return (virtual_addr_t) &_modtbl_start;
}
static inline virtual_size_t arch_modtbl_size(void)
{
	return (virtual_size_t) (&_modtbl_end - &_modtbl_start);
}

/** Init section functions required by VMM core */
extern u8 _init_text_start;
extern u8 _init_text_end;
static inline virtual_addr_t arch_init_text_vaddr(void)
{
	return (virtual_addr_t) &_init_text_start;
}
static inline virtual_size_t arch_init_text_size(void)
{
	return (virtual_size_t) (&_init_text_end - &_init_text_start);
}

#endif
