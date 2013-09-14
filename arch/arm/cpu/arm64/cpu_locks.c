/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARM64 specific synchronization mechanisms.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_smp.h>
#include <vmm_compiler.h>
#include <arch_barrier.h>

bool __lock arch_spin_lock_check(arch_spinlock_t *lock)
{
	return (lock->lock == __ARCH_SPIN_UNLOCKED) ? FALSE : TRUE;
}

void __lock arch_spin_lock(arch_spinlock_t *lock)
{
	u32 cpu = vmm_smp_processor_id();
	unsigned long tmp;

	__asm__ __volatile__(
"	sevl\n"
"1:	wfe\n"
"2:	ldaxr	%w0, %1\n"
"	cmp	%w0, %w3\n"
"	b.ne	1b\n"
"	stxr	%w0, %w2, %1\n"
"	cbnz	%w0, 2b\n"
	: "=&r" (tmp), "+Q" (lock->lock)
	: "r" (cpu), "r" (__ARCH_SPIN_UNLOCKED)
	: "cc", "memory");

	arch_smp_mb();
}

int arch_spin_trylock(arch_spinlock_t *lock)
{
	u32 cpu = vmm_smp_processor_id();
	unsigned int tmp;

	asm volatile(
"	ldaxr	%w0, %1\n"
"	cmp	%w0, %w3\n"
"	b.ne	1f\n"
"	stxr	%w0, %w2, %1\n"
"	b	2f\n"
"1:	mov	%w0, #1\n"
"2:\n"
	: "=&r" (tmp), "+Q" (lock->lock)
	: "r" (cpu), "r" (__ARCH_SPIN_UNLOCKED)
	: "cc", "memory");

	if (tmp == 0) {
		arch_smp_mb();	/* do mb if we succeeded */
		return 1;
	} else {
		return 0;
	}
}

void __lock arch_spin_unlock(arch_spinlock_t *lock)
{
	arch_smp_mb();

	__asm__ __volatile__(
"	stlr	%w1, %0\n"
	: "=Q" (lock->lock) : "r" (__ARCH_SPIN_UNLOCKED)
	: "memory");
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
	unsigned int tmp;

	__asm__ __volatile__(
	"	sevl\n"
	"1:	wfe\n"
	"2:	ldaxr	%w0, [%1]\n"
	"	cbnz	%w0, 1b\n"
	"	stxr	%w0, %w2, [%1]\n"
	"	cbnz	%w0, 2b\n"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (__ARCH_RW_LOCKED)
	: "memory");

	arch_smp_mb();
}

int __lock arch_write_trylock(arch_rwlock_t *lock)
{
	unsigned int tmp;

	asm volatile(
	"	ldaxr	%w0, [%1]\n"
	"	cbnz	%w0, 1f\n"
	"	stxr	%w0, %w2, [%1]\n"
	"1:\n"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (__ARCH_RW_LOCKED)
	: "memory");

	if (tmp == 0) {
		 arch_smp_mb();
		 return 1;
	} else {
		 return 0;
	}
}

void __lock arch_write_unlock(arch_rwlock_t *lock)
{
	asm volatile(
	"	stlr	%w1, [%0]\n"
	: : "r" (&lock->lock), "r" (0) : "memory");
}

bool __lock arch_read_lock_check(arch_rwlock_t *lock)
{
	return (lock->lock == __ARCH_RW_UNLOCKED) ? FALSE : TRUE;
}

void __lock arch_read_lock(arch_rwlock_t *lock)
{
	unsigned int tmp, tmp2;

	asm volatile(
	"	sevl\n"
	"1:	wfe\n"
	"2:	ldaxr	%w0, [%2]\n"
	"	add	%w0, %w0, #1\n"
	"	tbnz	%w0, #31, 1b\n"
	"	stxr	%w1, %w0, [%2]\n"
	"	cbnz	%w1, 2b\n"
	: "=&r" (tmp), "=&r" (tmp2)
	: "r" (&lock->lock)
	: "memory");
}

int __lock arch_read_trylock(arch_rwlock_t *lock)
{
	unsigned int tmp, tmp2 = 1;

	asm volatile(
	"	ldaxr	%w0, [%2]\n"
	"	add	%w0, %w0, #1\n"
	"	tbnz	%w0, #31, 1f\n"
	"	stxr	%w1, %w0, [%2]\n"
	"1:\n"
	: "=&r" (tmp), "+r" (tmp2)
	: "r" (&lock->lock)
	: "memory");

	if (tmp2 == 0) {
		arch_smp_mb();	/* do mb if we succeeded */
		return 1;
	} else {
		return 0;
	}
}

void __lock arch_read_unlock(arch_rwlock_t *lock)
{
	unsigned int tmp, tmp2;

	arch_smp_mb();

	asm volatile(
	"1:	ldxr	%w0, [%2]\n"
	"	sub	%w0, %w0, #1\n"
	"	stlxr	%w1, %w0, [%2]\n"
	"	cbnz	%w1, 1b\n"
	: "=&r" (tmp), "=&r" (tmp2)
	: "r" (&lock->lock)
	: "memory");
}
