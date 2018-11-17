/**
 * Copyright (c) 2018 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V specific synchronization mechanisms.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <arch_barrier.h>
#include <arch_atomic.h>

#include <riscv_lrsc.h>

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
	__asm__ __volatile__ (
		"	amoadd.w zero, %1, %0"
		: "+A" (atom->counter)
		: "r" (value)
		: "memory");
}

void __lock arch_atomic_sub(atomic_t *atom, long value)
{
	__asm__ __volatile__ (
		"	amoadd.w zero, %1, %0"
		: "+A" (atom->counter)
		: "r" (-value)
		: "memory");
}

long __lock arch_atomic_add_return(atomic_t *atom, long value)
{
	long ret;

	__asm__ __volatile__ (
		"	amoadd.w.aqrl  %1, %2, %0"
		: "+A" (atom->counter), "=r" (ret)
		: "r" (value)
		: "memory");

	return ret + value;
}

long __lock arch_atomic_sub_return(atomic_t *atom, long value)
{
	long ret;

	__asm__ __volatile__ (
		"	amoadd.w.aqrl  %1, %2, %0"
		: "+A" (atom->counter), "=r" (ret)
		: "r" (-value)
		: "memory");

	return ret - value;
}

long __lock arch_atomic_xchg(atomic_t *atom, long newval)
{
	return xchg(&atom->counter, newval);
}

long __lock arch_atomic_cmpxchg(atomic_t *atom, long oldval, long newval)
{
	return cmpxchg(&atom->counter, oldval, newval);
}
