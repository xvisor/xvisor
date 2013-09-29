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
 * @file cpu_atomic.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARM64 specific synchronization mechanisms.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <arch_barrier.h>
#include <arch_atomic.h>

long __lock arch_atomic_read(atomic_t *atom)
{
	long ret = atom->counter;
	arch_rmb();
	return ret;
}

void __lock arch_atomic_write(atomic_t *atom, long value)
{
	atom->counter = value;
	arch_wmb();
}

void __lock arch_atomic_add(atomic_t *atom, long value)
{
	unsigned int tmp;
	long result;

	asm volatile("// atomic_add\n"
"1:	ldxr	%w0, [%3]\n"
"	add	%w0, %w0, %w4\n"
"	stxr	%w1, %w0, [%3]\n"
"	cbnz	%w1,1b"
	: "=&r" (result), "=&r" (tmp), "+o" (atom->counter)
	: "r" (&atom->counter), "Ir" (value)
	: "cc");
}

void __lock arch_atomic_sub(atomic_t *atom, long value)
{
	unsigned int tmp;
	long result;

	asm volatile("// atomic_sub\n"
"1:	ldxr	%w0, [%3]\n"
"	sub	%w0, %w0, %w4\n"
"	stxr	%w1, %w0, [%3]\n"
"	cbnz	%w1, 1b"
	: "=&r" (result), "=&r" (tmp), "+o" (atom->counter)
	: "r" (&atom->counter), "Ir" (value)
	: "cc");
}

long __lock arch_atomic_add_return(atomic_t *atom, long value)
{
	unsigned int tmp;
	long result;

	asm volatile("// atomic_add_return\n"
"1:	ldaxr	%w0, [%3]\n"
"	add	%w0, %w0, %w4\n"
"	stlxr	%w1, %w0, [%3]\n"
"	cbnz	%w1, 1b"
	: "=&r" (result), "=&r" (tmp), "+o" (atom->counter)
	: "r" (&atom->counter), "Ir" (value)
	: "cc");

	return result;
}

long __lock arch_atomic_sub_return(atomic_t *atom, long value)
{
	unsigned int tmp;
	long result;

	asm volatile("// atomic_sub_return\n"
"1:	ldaxr	%w0, [%3]\n"
"	sub	%w0, %w0, %w4\n"
"	stlxr	%w1, %w0, [%3]\n"
"	cbnz	%w1, 1b"
	: "=&r" (result), "=&r" (tmp), "+o" (atom->counter)
	: "r" (&atom->counter), "Ir" (value)
	: "cc");

	return result;
}

long __lock arch_atomic_cmpxchg(atomic_t *atom, long oldval, long newval)
{
	unsigned int tmp;
	long previous;

	asm volatile("// atomic_cmpxchg\n"
"1:	ldaxr	%w1, [%3]\n"
"	cmp	%w1, %w4\n"
"	b.ne	2f\n"
"	stlxr	%w0, %w5, [%3]\n"
"	cbnz	%w0, 1b\n"
"2:"
	: "=&r" (tmp), "=&r" (previous), "+o" (atom->counter)
	: "r" (&atom->counter), "Ir" (oldval), "r" (newval)
	: "cc");

	return previous;
}
