/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
 * Copyright (c) 2011 Jim Huang.
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
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief ARM specific synchronization mechanisms.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <arch_barrier.h>
#include <vmm_smp.h>

bool __lock arch_spin_lock_check(arch_spinlock_t * lock)
{
	return (lock->lock == __ARCH_SPIN_UNLOCKED) ? TRUE : FALSE;
}

void __lock arch_spin_lock(arch_spinlock_t * lock)
{
	u32 cpu = vmm_smp_processor_id();
	unsigned long tmp;

	__asm__ __volatile__(
"1:	ldrex	%0, [%1]\n"	/* load the lock value */
"	teq	%0, %3\n"	/* is the lock free */
#ifdef CONFIG_SMP
"	wfene\n"		/* if not, we sleep until some other core wake us up */
#endif
"	strexeq	%0, %2, [%1]\n" /* store (cpu) as a lock value */
"	teqeq	%0, #0\n"	/* did we succeed */
"	bne	1b"		/* if not try again */
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (cpu), "r" (__ARCH_SPIN_UNLOCKED)
	: "cc");

	arch_smp_mb();		/* do a mb to sync everything */
}

int __lock arch_spin_trylock(arch_spinlock_t *lock)
{
	u32 cpu = vmm_smp_processor_id();
	unsigned long tmp;

	__asm__ __volatile__(
"	ldrex	%0, [%1]\n"	/* load the lock value */
"	teq	%0, %3\n"	/* is the lock free */
"	strexeq	%0, %2, [%1]"	/* store cpu as a lock value */
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (cpu), "r" (__ARCH_SPIN_UNLOCKED)
	: "cc");

	if (tmp == 0) {
		arch_smp_mb();	/* do mb if we succeeded */
		return 1;
	} else {
		return 0;
	}
}

void __lock arch_spin_unlock(arch_spinlock_t * lock)
{
	arch_smp_mb();		/* sync everything */

	lock->lock = __ARCH_SPIN_UNLOCKED;	/* free the lock */
	dsb();			/* sync again */

#ifdef CONFIG_SMP
	sev();			/* notify all cores */
#endif
}

