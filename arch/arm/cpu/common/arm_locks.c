/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file arm_locks.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Anup Patel (anup@brainfault.org)
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Jim Huang (jserv@0xlab.org)
 * @brief ARM32 and ARM32VE specific synchronization mechanisms.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <arch_barrier.h>
#include <vmm_smp.h>

bool __lock arch_spin_lock_check(arch_spinlock_t *lock)
{
	return (lock->lock == __ARCH_SPIN_UNLOCKED) ? FALSE : TRUE;
}

void __lock arch_spin_lock(arch_spinlock_t *lock)
{
	u32 cpu = vmm_smp_processor_id();
	unsigned long tmp;

	__asm__ __volatile__(
"1:	ldrex	%0, [%1]\n"	/* load the lock value */
"	teq	%0, %3\n"	/* is the lock free */
"	wfene\n"		/* if not, we sleep until some other core wake us up */
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
"	movne	%0, #1\n"	/* assume failure if lock not free */
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

void __lock arch_spin_unlock(arch_spinlock_t *lock)
{
	arch_smp_mb();		/* sync everything */

	lock->lock = __ARCH_SPIN_UNLOCKED;	/* free the lock */
	dsb();			/* sync again */
	sev();			/* notify all cores */
}

bool __lock arch_write_lock_check(arch_rwlock_t *lock)
{
	return (lock->lock & __ARCH_RW_LOCKED) ? TRUE : FALSE;
}

/*
 * To take a write lock we set bit 31 to 1
 */
void __lock arch_write_lock(arch_rwlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
"1:	ldrex   %0, [%1]\n"
"	teq     %0, #0\n"
"	wfene	\n"
"	strexeq %0, %2, [%1]\n"
"	teq     %0, #0\n"
"	bne     1b"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (__ARCH_RW_LOCKED)
	: "cc");

	arch_smp_mb();
}

int __lock arch_write_trylock(arch_rwlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
"	ldrex   %0, [%1]\n"
"	teq     %0, #0\n"
"	strexeq %0, %2, [%1]"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (__ARCH_RW_LOCKED)
	: "cc");

	if (tmp == 0) {
		 arch_smp_mb();
		 return 1;
	} else {
		 return 0;
	}
}

void __lock arch_write_unlock(arch_rwlock_t *lock)
{
	arch_smp_mb();
	lock->lock = __ARCH_RW_UNLOCKED;
	dsb();
	sev();
}

bool __lock arch_read_lock_check(arch_rwlock_t *lock)
{
	return (lock->lock == __ARCH_RW_UNLOCKED) ? FALSE : TRUE;
}

void __lock arch_read_lock(arch_rwlock_t *lock)
{
	unsigned long tmp, tmp2;

	__asm__ __volatile__(
"1:	ldrex   %0, [%2]\n"
"	adds    %0, %0, #1\n"
"	strexpl %1, %0, [%2]\n"
"	wfemi	\n"
"	rsbpls  %0, %1, #0\n"
"	bmi     1b"
	: "=&r" (tmp), "=&r" (tmp2)
	: "r" (&lock->lock)
	: "cc");

	arch_smp_mb();
}

int __lock arch_read_trylock(arch_rwlock_t *lock)
{
	unsigned long tmp, tmp2 = 1;

	__asm__ __volatile__(
"	ldrex   %0, [%2]\n"
"	adds    %0, %0, #1\n"
"	strexpl %1, %0, [%2]\n"
	: "=&r" (tmp), "+r" (tmp2)
	: "r" (&lock->lock)
	: "cc");

	if (tmp2 == 0) {
		arch_smp_mb();	/* do mb if we succeeded */
		return 1;
	} else {
		return 0;
	}
}

void __lock arch_read_unlock(arch_rwlock_t *lock)
{
	unsigned long tmp, tmp2;

	arch_smp_mb();

	__asm__ __volatile__(
"1:	ldrex   %0, [%2]\n"
"	sub     %0, %0, #1\n"
"	strex   %1, %0, [%2]\n"
"	teq     %1, #0\n"
"	bne     1b"
	: "=&r" (tmp), "=&r" (tmp2)
	: "r" (&lock->lock)
	: "cc");

	if (tmp == 0) {
		dsb();
		sev();
	}
}
