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
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Architecture specific implementation of synchronization mechanisms.
 */

#include <cpu_atomic.h>

void __lock __cpu_atomic_inc (atomic_t *atom)
{
        int tmp;

	__asm__ __volatile__ (
		"1: ll %0, 0(%2)\n\t"
		"addiu %0,%0,1\n\t"
		"sc %0,0(%2)\n\t"
		"beq %0, $0, 1b\n\t"
		:"=&r"(tmp), "+m"(atom->counter)
		:"r"(&atom->counter));
}

void __lock __cpu_atomic_dec (atomic_t *atom)
{
        int tmp;

	__asm__ __volatile__ (
		"1: ll %0, 0(%2)\n\t"
		"addiu %0,%0,-1\n\t"
		"sc %0,0(%2)\n\t"
		"beq %0, $0, 1b\n\t"
		:"=&r"(tmp), "+m"(atom->counter)
		:"r"(&atom->counter));
}

void __lock vmm_cpu_atomic_inc (atomic_t *atom)
{
	__cpu_atomic_inc(atom);
}

void __lock vmm_cpu_atomic_dec (atomic_t *atom)
{
	__cpu_atomic_dec(atom);
}
