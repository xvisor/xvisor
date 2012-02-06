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
 * @brief header file for spinlock synchronization mechanisms.
 */

#ifndef __VMM_SPINLOCKS_H__
#define __VMM_SPINLOCKS_H__

#include <vmm_types.h>

#if defined(CONFIG_SMP)

/* 
 * FIXME: With SMP should rather be holding
 * more information like which core is holding the
 * lock.
 */
struct vmm_spinlock {
	spinlock_t __the_lock;
};

#define __SPIN_LOCK_INITIALIZER(_lptr) 	ARCH_SPIN_LOCK_INIT(&((_lptr)->__the_lock))
#define DEFINE_SPIN_LOCK(_lock) 	vmm_spinlock_t _lock = __SPIN_LOCK_INITIALIZER(&_lock);

#define DECLARE_SPIN_LOCK(_lock)	vmm_spinlock_t _lock;
#define INIT_SPIN_LOCK(_lptr)		__SPIN_LOCK_INITIALIZER(_lptr)

#else 

struct vmm_spinlock {
	u32 __the_lock;
};

#define INIT_SPIN_LOCK(_lptr)		((_lptr)->__the_lock = 0)

#endif

typedef struct vmm_spinlock vmm_spinlock_t;

/** Check status of spinlock (TRUE: Locked, FALSE: Unlocked) */
bool vmm_spin_lock_check(vmm_spinlock_t * lock);

/** Lock the spinlock */
void vmm_spin_lock(vmm_spinlock_t * lock);

/** Unlock the spinlock */
void vmm_spin_unlock(vmm_spinlock_t * lock);

/** Save irq flags and lock the spinlock */
irq_flags_t vmm_spin_lock_irqsave(vmm_spinlock_t * lock);

/** Unlock the spinlock and restore irq flags */
void vmm_spin_unlock_irqrestore(vmm_spinlock_t * lock, irq_flags_t flags);

#endif /* __VMM_SPINLOCKS_H__ */
