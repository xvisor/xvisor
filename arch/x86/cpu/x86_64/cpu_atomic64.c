/**
 * Copyright (c) 2013 Jean-Christophe Dubois
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
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Architecture specific 64bits synchronization mechanisms.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <arch_atomic64.h>
#include <arch_barrier.h>

#ifdef CONFIG_SMP
#define LOCK_PREFIX "\n\tlock;\n"
#else
#define LOCK_PREFIX ""
#endif

u64  __lock arch_atomic64_read(atomic64_t *atom)
{
	u64 ret;
	ret = atom->counter;
	arch_rmb();
	return ret;
}

void  __lock arch_atomic64_write(atomic64_t *atom, u64 value)
{
	atom->counter = value;
	arch_wmb();
}

void __lock arch_atomic64_add(atomic64_t *atom, u64 value)
{
	asm volatile(LOCK_PREFIX " addq %1,%0\n\t"
		     :"+m"(atom->counter)
		     :"ir"(value));
}

void __lock arch_atomic64_sub(atomic64_t *atom, u64 value)
{
	asm volatile (LOCK_PREFIX " subq %1,%0\n\t"
		      :"+m"(atom->counter)
		      :"ir"(value));
}

u64 __lock arch_atomic64_add_return(atomic64_t *atom, u64 value)
{
	long oval;
	asm volatile(LOCK_PREFIX " xaddq %0,%1\n\t"
		     :"=r"(oval),"+m"(atom->counter)
		     :"0"(value):"cc");

	return value + oval;
}

u64 __lock arch_atomic64_sub_return(atomic64_t *atom, u64 value)
{
	return arch_atomic64_add_return(atom, -value);
}

u64 __lock arch_atomic64_cmpxchg(atomic64_t *atom, u64 oldval, u64 newval)
{
	long ret;
	asm volatile(LOCK_PREFIX "cmpxchgq %2,%1\n\t"
		     :"=a"(ret),"+m"(atom->counter)
		     :"r"(newval), "0"(oldval)
		     :"memory");
	return ret;
}
