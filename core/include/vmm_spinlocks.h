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
 * @version 0.01
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @brief header file for spinlock synchronization mechanisms.
 */

#ifndef __VMM_SPINLOCKS_H__
#define __VMM_SPINLOCKS_H__

#include <vmm_types.h>
#include <vmm_sections.h>

/* 
 * FIXME: With SMP should rather be holding
 * more information like which core is holding the
 * lock.
 */
typedef struct {
	vmm_cpu_spinlock_t __the_lock;
} vmm_spinlock_t;

#define __INIT_SPIN_LOCK_UNLOCKED 	{ .__the_lock = __CPU_INIT_SPIN_LOCK_UNLOCKED }
#define __SPIN_LOCK_INITIALIZER(_lptr) 	__CPU_SPIN_LOCK_UNLOCKED(&((_lptr)->__the_lock))
#define DEFINE_SPIN_LOCK(_lock) 	vmm_spinlock_t _lock = __SPIN_LOCK_INITIALIZER(&_lock);

#define DECLARE_SPIN_LOCK(_lock)	vmm_spinlock_t _lock;
#define INIT_SPIN_LOCK(_lptr)		__SPIN_LOCK_INITIALIZER(_lptr)

void __lock_section vmm_spin_lock(vmm_spinlock_t * lock);
void __lock_section vmm_spin_unlock(vmm_spinlock_t * lock);
irq_flags_t __lock_section vmm_spin_lock_irqsave(vmm_spinlock_t * lock);
void __lock_section vmm_spin_unlock_irqrestore(vmm_spinlock_t * lock,
					       irq_flags_t flags);

#endif /* __VMM_LOCKS_H__ */
