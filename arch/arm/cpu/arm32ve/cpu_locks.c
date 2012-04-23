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
 * @file cpu_locks.c
 * @author Anup Patel (anup@brainfault.org)
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Jim Huang (jserv@0xlab.org)
 * @brief ARM specific synchronization mechanisms.
 */

#include <vmm_error.h>
#include <vmm_types.h>

void __lock __cpu_spin_lock(spinlock_t * lock)
{
	unsigned int tmp;
	__asm__ __volatile__("@ __cpu_spin_lock\n"
			     "1:\n"
			     "       ldrex   %0, [%2]\n"
				/* Load lock->__cpu_lock([%2]) to tmp (%0) */
			     "       teq     %0, #0\n"
				/* If the physical address is tagged as exclusive access,
				 * lock->__cpu_lock should equal to 0  */
			     "       it      eq\n"
			     "       strexeq %0, %1, [%2]\n"
				/* Save 1 to lock->__cpu_lock
				 * Result of this operation would be in tmp (%0).
				 * if store operation success, result will be 0 or else 1
				 */
			     "       teq     %0, #0\n"
				/* Compare result with 0 again */
			     "       bne     1b\n"
				/* If fails go back to 1 and retry else return */
			     :"=&r"(tmp)
			     :"r"(1), "r"(&lock->lock)
			     :"cc", "memory");
}

void __lock __cpu_spin_unlock(spinlock_t * lock)
{
	__asm__ __volatile__("@ __cpu_spin_unlock\n"
			     "       str     %0, [%1]\n"
				/* Save 0 to lock->__cpu_lock in order to unlock */
			     :
			     :"r"(0), "r"(&lock->lock)
			     :"memory");
}

bool __lock arch_cpu_spin_lock_check(spinlock_t * lock)
{
	return (lock->lock) ? TRUE : FALSE;
}

void __lock arch_cpu_spin_lock(spinlock_t * lock)
{
	return __cpu_spin_lock(lock);
}

void __lock arch_cpu_spin_unlock(spinlock_t * lock)
{
	__cpu_spin_unlock(lock);
}

