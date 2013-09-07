/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
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
 * au64 with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file arch_atomic64.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief generic interface for arch specific 64 bits atomic operations
 */
#ifndef _ARCH_ATOMIC64_H__
#define _ARCH_ATOMIC64_H__

#include <vmm_types.h>

/** Atomic64 operations required by VMM core */
u64 arch_atomic64_read(atomic64_t *atom);
void arch_atomic64_write(atomic64_t *atom, u64 value);
void arch_atomic64_add(atomic64_t *atom, u64 value);
u64 arch_atomic64_add_return(atomic64_t *atom, u64 value);
void arch_atomic64_sub(atomic64_t *atom, u64 value);
u64 arch_atomic64_sub_return(atomic64_t *atom, u64 value);
u64 arch_atomic64_cmpxchg(atomic64_t *atom, u64 oldval, u64 newval);

/** Derived Atomic64 operations required by VMM core 
 *  NOTE: Architecture specific code does not provide
 *  this operations.
 */
#define arch_atomic64_inc(atom)	arch_atomic64_add(atom, 1)
#define arch_atomic64_dec(atom)	arch_atomic64_sub(atom, 1)

#endif
