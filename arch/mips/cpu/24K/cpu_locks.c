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
 * @file cpu_locks.c
 * @version 0.01
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @brief Architecture specific implementation of synchronization mechanisms.
 */

#include <vmm_cpu.h>
#include <cpu_locks.h>

void __lock_section __cpu_spin_lock(vmm_cpu_spinlock_t *lock)
{
        int tmp;
	u32 *lcounter = (u32 *)&lock->__cpu_lock.counter;

	__asm__ __volatile__ (
		"1: ll %0, 0(%2)\n\t"
		"bgtz %0, 1b\n\t"
		"nop\n\t"
		"addiu %0,%0,1\n\t"
		"sc %0,0(%2)\n\t"
		"beq %0, $0, 1b\n\t"
		:"=&r"(tmp), "+m"(lcounter)
		:"r"(lcounter));
}

void __lock_section __cpu_spin_unlock(vmm_cpu_spinlock_t *lock)
{
	int tmp;
	u32 *lcounter = (u32 *)&lock->__cpu_lock.counter;

	__asm__ __volatile__ (
		"1: ll %0, 0(%2)\n\t"
		"2:\n\t"
		"beq %0, $0, 2b\n\t"
		"nop\n\t"
		"addiu %0, %0, -1\n\t"
		"sc %0, 0(%2)\n\t"
		"beq %0, $0, 1b\n\t"
		:"=&r"(tmp), "+m"(lcounter)
		:"r"(lcounter));
}

irq_flags_t __lock_section __cpu_spin_lock_irqsave (vmm_cpu_spinlock_t *lock)
{
        irq_flags_t flags;
        flags = vmm_interrupts_save();
	__cpu_spin_lock(lock);
        return flags;
}

void __lock_section __cpu_spin_unlock_irqrestore (vmm_cpu_spinlock_t *lock,
						irq_flags_t flags)
{
	__cpu_spin_unlock(lock);
        vmm_interrupts_restore(flags);
}

void __lock_section vmm_cpu_spin_lock (vmm_cpu_spinlock_t *lock)
{
	__cpu_spin_lock(lock);
}

void __lock_section vmm_cpu_spin_unlock (vmm_cpu_spinlock_t *lock)
{
	__cpu_spin_unlock(lock);
}

irq_flags_t vmm_cpu_spin_lock_irqsave(vmm_cpu_spinlock_t *lock)
{
	return __cpu_spin_lock_irqsave(lock);
}

void vmm_cpu_spin_unlock_irqrestore(vmm_cpu_spinlock_t *lock, 
				irq_flags_t flags)
{
	__cpu_spin_unlock_irqrestore(lock, flags);
}
