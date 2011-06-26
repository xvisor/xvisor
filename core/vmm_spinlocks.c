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
 * @file vmm_locks.c
 * @version 0.01
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @brief header file for spinlock synchronization mechanisms.
 */

#include <vmm_cpu.h>
#include <vmm_spinlocks.h>

void __lock_section vmm_spin_lock(vmm_spinlock_t * lock)
{
	/* Disable irq on current CPU */
	vmm_cpu_irq_disable();
	/* Call CPU specific locking routine */
	vmm_cpu_spin_lock(&lock->__the_lock);
}

void __lock_section vmm_spin_unlock(vmm_spinlock_t * lock)
{
	/* Call CPU specific unlocking routine */
	vmm_cpu_spin_unlock(&lock->__the_lock);
	/* Enable irq on current CPU */
	vmm_cpu_irq_enable();
}

irq_flags_t __lock_section vmm_spin_lock_irqsave(vmm_spinlock_t * lock)
{
	return vmm_cpu_spin_lock_irqsave(&lock->__the_lock);
}

void __lock_section vmm_spin_unlock_irqrestore(vmm_spinlock_t * lock,
					       irq_flags_t flags)
{
	vmm_cpu_spin_unlock_irqrestore(&lock->__the_lock, flags);
}
