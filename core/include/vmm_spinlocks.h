/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_spinlocks.h
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for spinlock synchronization mechanisms.
 */

#ifndef __VMM_SPINLOCKS_H__
#define __VMM_SPINLOCKS_H__

#include <arch_cpu_irq.h>
#include <arch_locks.h>
#include <vmm_types.h>

#if defined(CONFIG_SMP)

/* 
 * FIXME: With SMP should rather be holding
 * more information like which core is holding the
 * lock.
 */
struct vmm_spinlock {
	spinlock_t __tlock;
};

#define __SPIN_LOCK_INITIALIZER(_lptr) 	ARCH_SPIN_LOCK_INIT(&((_lptr)->__tlock))
#define DEFINE_SPIN_LOCK(_lock) 	vmm_spinlock_t _lock = __SPIN_LOCK_INITIALIZER(&_lock);

#define DECLARE_SPIN_LOCK(_lock)	vmm_spinlock_t _lock;
#define INIT_SPIN_LOCK(_lptr)		__SPIN_LOCK_INITIALIZER(_lptr)

#else 

struct vmm_spinlock {
	u32 __tlock;
};

#define INIT_SPIN_LOCK(_lptr)		((_lptr)->__tlock = 0)

#endif

typedef struct vmm_spinlock vmm_spinlock_t;

extern void vmm_scheduler_preempt_disable(void);
extern void vmm_scheduler_preempt_enable(void);

/** Check status of spinlock (TRUE: Locked, FALSE: Unlocked) 
 *  PROTOTYPE: bool vmm_spin_lock_check(vmm_spinlock_t * lock)
 */
#if defined(CONFIG_SMP)
#define vmm_spin_lock_check(lock)	arch_spin_lock_check((lock)->__tlock)
#else
#define vmm_spin_lock_check(lock)	FALSE
#endif

/** Lock the spinlock 
 *  PROTOTYPE: void vmm_spin_lock(vmm_spinlock_t * lock) 
 */
#if defined(CONFIG_SMP)
#define vmm_spin_lock(lock)		do { \
					vmm_scheduler_preempt_disable(); \
					arch_spin_lock(&(lock)->__tlock); \
					} while (0)
#else
#define vmm_spin_lock(lock)		do { \
					vmm_scheduler_preempt_disable(); \
					} while (0)
#endif

/** Unlock the spinlock 
 *  PROTOTYPE: void vmm_spin_unlock(vmm_spinlock_t * lock)
 */
#if defined(CONFIG_SMP)
#define vmm_spin_unlock(lock)		do { \
					arch_spin_unlock(&(lock)->__tlock); \
					vmm_scheduler_preempt_enable(); \
					} while (0)
#else
#define vmm_spin_unlock(lock)		do { \
					vmm_scheduler_preempt_enable(); \
					} while (0)
#endif

/** Disable irq and lock the spinlock
 *  PROTOTYPE: void vmm_spin_lock_irq(vmm_spinlock_t * lock) 
 */
#if defined(CONFIG_SMP)
#define vmm_spin_lock_irq(lock) 	do { \
					arch_cpu_irq_disable(); \
					arch_spin_lock(&(lock)->__tlock); \
					} while (0)
#else
#define vmm_spin_lock_irq(lock) 	do { \
					arch_cpu_irq_disable(); \
					} while (0)
#endif

/** Unlock the spinlock and enable irq 
 *  PROTOTYPE: void vmm_spin_unlock_irq(vmm_spinlock_t * lock)
 */
#if defined(CONFIG_SMP)
#define vmm_spin_unlock_irq(lock)	do { \
					arch_spin_unlock(&(lock)->__tlock); \
					arch_cpu_irq_enable(); \
					} while (0)
#else
#define vmm_spin_unlock_irq(lock) 	do { \
					arch_cpu_irq_enable(); \
					} while (0)
#endif

/** Save irq flags and lock the spinlock
 *  PROTOTYPE: irq_flags_t vmm_spin_lock_irqsave(vmm_spinlock_t * lock) 
 */
#if defined(CONFIG_SMP)
#define vmm_spin_lock_irqsave(lock, flags) \
					do { \
					flags = arch_cpu_irq_save(); \
					arch_spin_lock(&(lock)->__tlock); \
					} while (0)
#else
#define vmm_spin_lock_irqsave(lock, flags) \
					do { \
					flags = arch_cpu_irq_save(); \
					} while (0)
#endif

/** Unlock the spinlock and restore irq flags 
 *  PROTOTYPE: void vmm_spin_unlock_irqrestore(vmm_spinlock_t * lock,
						irq_flags_t flags)
 */
#if defined(CONFIG_SMP)
#define vmm_spin_unlock_irqrestore(lock, flags)	\
					do { \
					arch_spin_unlock(&(lock)->__tlock); \
					arch_cpu_irq_restore(flags); \
					} while (0)
#else
#define vmm_spin_unlock_irqrestore(lock, flags) \
					do { \
					arch_cpu_irq_restore(flags); \
					} while (0)
#endif

#endif /* __VMM_SPINLOCKS_H__ */
