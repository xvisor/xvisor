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
 * @file cpu_atomic.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Architecture specific implementation of synchronization mechanisms.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <arch_atomic.h>
#include <arch_barrier.h>

#if defined(CONFIG_SMP)
#define LOCK_PREFIX "lock;\n"
#else
#define LOCK_PREFIX ""
#endif

long  __lock arch_atomic_read(atomic_t *atom)
{
	long ret = atom->counter;
	arch_rmb();
	return ret;
}

void  __lock arch_atomic_write(atomic_t *atom, long value)
{
	atom->counter = value;
	arch_wmb();
}

void __lock arch_atomic_add(atomic_t *atom, long value)
{
	asm volatile(LOCK_PREFIX " addl %k1,%k0\n\t"
		     :"+m"(atom->counter)
		     :"ir"(value));
}

void __lock arch_atomic_sub(atomic_t *atom, long value)
{
	asm volatile (LOCK_PREFIX " subl %k1,%k0\n\t"
		      :"+m"(atom->counter)
		      :"ir"(value));
}

long __lock arch_atomic_add_return(atomic_t *atom, long value)
{
	long oval;
	asm volatile(LOCK_PREFIX " xaddl %k0,%k1\n\t"
		     :"=r"(oval),"+m"(atom->counter)
		     :"0"(value):"cc");

	return value + oval;
}

long __lock arch_atomic_sub_return(atomic_t *atom, long value)
{
	return arch_atomic_add_return(atom, -value);
}

long __lock arch_atomic_cmpxchg(atomic_t *atom, long oldval, long newval)
{
	long ret;
	asm volatile(LOCK_PREFIX "cmpxchgl %k2,%k1\n\t"
		     :"=a"(ret),"+m"(atom->counter)
		     :"r"(newval), "0"(oldval)
		     :"memory");
	return ret;
}
