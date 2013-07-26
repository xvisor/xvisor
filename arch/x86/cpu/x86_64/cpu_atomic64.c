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
 * @brief Architecture specific 64bits synchronization mechanisms.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <arch_atomic64.h>

/* FIXME: Need memory barrier for this. */
u64  __lock arch_atomic64_read(atomic64_t * atom)
{
	return atom->counter;
}

/* FIXME: Need memory barrier for this. */
void  __lock arch_atomic64_write(atomic64_t * atom, u64 value)
{
	atom->counter = value;
}

/* FIXME: Implement This. */
void __lock arch_atomic64_add(atomic64_t * atom, u64 value)
{
}

/* FIXME: Implement This. */
void __lock arch_atomic64_sub(atomic64_t * atom, u64 value)
{
}

/* FIXME: Implement This. */
u64 __lock arch_atomic64_add_return(atomic64_t * atom, u64 value)
{
	return 0;
}

/* FIXME: Implement This. */
u64 __lock arch_atomic64_sub_return(atomic64_t * atom, u64 value)
{
	return 0;
}
