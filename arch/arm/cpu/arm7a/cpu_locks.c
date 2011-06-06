/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file cpu_locks.c
 * @version 0.01
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief Architecture specific implementation of synchronization mechanisms.
 */

#include <vmm_cpu.h>
#include <cpu_locks.h>

void __lock_section __cpu_spin_lock(vmm_cpu_spinlock_t * lock)
{
	while (__cpu_atomic_testnset(&lock->__cpu_lock, 0, 1)) ;
}

void __lock_section __cpu_spin_unlock(vmm_cpu_spinlock_t * lock)
{
	while (__cpu_atomic_testnset(&lock->__cpu_lock, 1, 0)) ;
}

irq_flags_t __lock_section __cpu_spin_lock_irqsave(vmm_cpu_spinlock_t * lock)
{
	irq_flags_t flags;

	while (__cpu_atomic_testnset(&lock->__cpu_lock, 0, 1)) ;
	flags = vmm_cpu_irq_save();

	return flags;
}

void __lock_section __cpu_spin_unlock_irqrestore(vmm_cpu_spinlock_t * lock,
						 irq_flags_t flags)
{
	while (__cpu_atomic_testnset(&lock->__cpu_lock, 1, 0)) ;
	vmm_cpu_irq_restore(flags);
}

void __lock_section vmm_cpu_spin_lock(vmm_cpu_spinlock_t * lock)
{
	return __cpu_spin_lock(lock);
}

void __lock_section vmm_cpu_spin_unlock(vmm_cpu_spinlock_t * lock)
{
	__cpu_spin_unlock(lock);
}

irq_flags_t __lock_section vmm_cpu_spin_lock_irqsave(vmm_cpu_spinlock_t * lock)
{
	return __cpu_spin_lock_irqsave(lock);
}

void __lock_section vmm_cpu_spin_unlock_irqrestore(vmm_cpu_spinlock_t * lock,
						   irq_flags_t flags)
{
	__cpu_spin_unlock_irqrestore(lock, flags);
}
