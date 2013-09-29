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
 * @file cpu_atomic64.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARM specific 64 bits synchronization mechanisms.
 *
 * derived from linux/arch/arm64/include/asm/atomic.h
 *
 * Copyright (C) 1996 Russell King.
 * Copyright (C) 2002 Deep Blue Solutions Ltd.
 * Copyright (C) 2012 ARM Ltd.
 *
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <arch_cpu_irq.h>
#include <arch_barrier.h>
#include <arch_atomic64.h>

u64 __lock arch_atomic64_read(atomic64_t *atom)
{
	return (*(volatile long *)&atom->counter);
}

void __lock arch_atomic64_write(atomic64_t *atom, u64 value)
{
	atom->counter = value;
}

void __lock arch_atomic64_add(atomic64_t *atom, u64 value)
{
	u64 result, tmp;

	asm volatile("// atomic64_add\n"
"1:	ldxr	%0, %2\n"
"	add	%0, %0, %3\n"
"	stxr	%w1, %0, %2\n"
"	cbnz	%w1, 1b"
	: "=&r" (result), "=&r" (tmp), "+Q" (atom->counter)
	: "Ir" (value)
	: "cc");
}

void __lock arch_atomic64_sub(atomic64_t *atom, u64 value)
{
	u64 result, tmp;

	asm volatile("// atomic64_sub\n"
"1:	ldxr	%0, %2\n"
"	sub	%0, %0, %3\n"
"	stxr	%w1, %0, %2\n"
"	cbnz	%w1, 1b"
	: "=&r" (result), "=&r" (tmp), "+Q" (atom->counter)
	: "Ir" (value)
	: "cc");
}

u64 __lock arch_atomic64_add_return(atomic64_t *atom, u64 value)
{
	u64 result, tmp;

	asm volatile("// atomic64_add_return\n"
"1:	ldaxr	%0, %2\n"
"	add	%0, %0, %3\n"
"	stlxr	%w1, %0, %2\n"
"	cbnz	%w1, 1b"
	: "=&r" (result), "=&r" (tmp), "+Q" (atom->counter)
	: "Ir" (value)
	: "cc", "memory");

	return result;
}

u64 __lock arch_atomic64_sub_return(atomic64_t *atom, u64 value)
{
	u64 result, tmp;

	asm volatile("// atomic64_sub_return\n"
"1:	ldaxr	%0, %2\n"
"	sub	%0, %0, %3\n"
"	stlxr	%w1, %0, %2\n"
"	cbnz	%w1, 1b"
	: "=&r" (result), "=&r" (tmp), "+Q" (atom->counter)
	: "Ir" (value)
	: "cc", "memory");

	return result;
}

u64 __lock arch_atomic64_cmpxchg(atomic64_t *atom, u64 oldval, u64 newval)
{
	u64 previous;
	unsigned long res;

	asm volatile("// atomic64_cmpxchg\n"
"1:	ldaxr	%1, %2\n"
"	cmp	%1, %3\n"
"	b.ne	2f\n"
"	stlxr	%w0, %4, %2\n"
"	cbnz	%w0, 1b\n"
"2:"
	: "=&r" (res), "=&r" (previous), "+Q" (atom->counter)
	: "Ir" (oldval), "r" (newval)
	: "cc", "memory");

	return previous;
}
