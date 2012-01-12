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
 * @file vmm_spinlocks.c
 * @version 0.01
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @brief header file for spinlock synchronization mechanisms.
 */

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_scheduler.h>
#include <vmm_spinlocks.h>

bool __lock vmm_spin_lock_check(vmm_spinlock_t * lock)
{
#if defined(CONFIG_SMP)
	/* Call CPU specific locking routine */
	return arch_cpu_spin_lock_check(&lock->__the_lock);
#else
	return FALSE;
#endif
}

void __lock vmm_spin_lock(vmm_spinlock_t * lock)
{
	/* Disable preemption in scheduler */
	vmm_scheduler_preempt_disable();
#if defined(CONFIG_SMP)
	/* Call CPU specific locking routine */
	arch_cpu_spin_lock(&lock->__the_lock);
#endif
}

void __lock vmm_spin_unlock(vmm_spinlock_t * lock)
{
#if defined(CONFIG_SMP)
	/* Call CPU specific unlocking routine */
	arch_cpu_spin_unlock(&lock->__the_lock);
#endif
	/* Enable preemption in scheduler */
	vmm_scheduler_preempt_enable();
}

irq_flags_t __lock vmm_spin_lock_irqsave(vmm_spinlock_t * lock)
{
	irq_flags_t flags;
	/* Disable and save interrupt flags*/
	flags = arch_cpu_irq_save();
#if defined(CONFIG_SMP)
	/* Call CPU specific locking routine */
	arch_cpu_spin_lock(&lock->__the_lock);
#endif
	/* Return saved interrupt flags*/
	return flags;
}

void __lock vmm_spin_unlock_irqrestore(vmm_spinlock_t * lock,
					irq_flags_t flags)
{
#if defined(CONFIG_SMP)
	/* Call CPU specific unlocking routine */
	arch_cpu_spin_unlock(&lock->__the_lock);
#endif
	/* Restore saved interrupt flags */
	arch_cpu_irq_restore(flags);
}

