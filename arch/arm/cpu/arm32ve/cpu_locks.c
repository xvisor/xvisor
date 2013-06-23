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
"1:	ldrex	%0, [%1]\n"
"	teq	%0, %3\n"
"	wfene\n"
"	strexeq	%0, %2, [%1]\n"
"	teqeq	%0, #0\n"
"	bne	1b"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (cpu), "r" (__ARCH_SPIN_UNLOCKED)
	: "cc");

	arch_smp_mb();
}

int __lock arch_spin_trylock(arch_spinlock_t *lock)
{
	u32 cpu = vmm_smp_processor_id();
	unsigned long tmp;

	__asm__ __volatile__(
"	ldrex	%0, [%2]\n"
"	teq	%0, %3\n"
"	strexeq	%0, %2, [%1]"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (cpu), "r" (__ARCH_SPIN_UNLOCKED)
	: "cc");

	if (tmp == 0) {
		arch_smp_mb();
		return 1;
	} else {
		return 0;
	}
}

void __lock arch_spin_unlock(arch_spinlock_t * lock)
{
	arch_smp_mb();

	lock->lock = __ARCH_SPIN_UNLOCKED;
	dsb();
	sev();
}

