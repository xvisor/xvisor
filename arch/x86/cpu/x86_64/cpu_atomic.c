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

/* FIXME: Need memory barrier for this. */
long  __lock arch_atomic_read(atomic_t *atom)
{
	return atom->counter;
}

/* FIXME: Need memory barrier for this. */
void  __lock arch_atomic_write(atomic_t *atom, long value)
{
	atom->counter = value;
}

/* FIXME: Implement This. */
void __lock arch_atomic_add(atomic_t *atom, long value)
{
}

/* FIXME: Implement This. */
void __lock arch_atomic_sub(atomic_t *atom, long value)
{
}

/* FIXME: Implement This. */
long __lock arch_atomic_add_return(atomic_t *atom, long value)
{
	return 0;
}

/* FIXME: Implement This. */
long __lock arch_atomic_sub_return(atomic_t *atom, long value)
{
	return 0;
}

/* FIXME: Implement This. */
long __lock arch_atomic_cmpxchg(atomic_t *atom, long oldval, long newval)
{
	return 0;
}
