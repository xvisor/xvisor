/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file arch_locks.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific locking operations
 */
#ifndef _ARCH_LOCKS_H__
#define _ARCH_LOCKS_H__

#include <vmm_types.h>

/** Spin lock functions required by VMM core */
bool arch_spin_lock_check(arch_spinlock_t *lock);
void arch_spin_lock(arch_spinlock_t *lock);
int  arch_spin_trylock(arch_spinlock_t *lock);
void arch_spin_unlock(arch_spinlock_t *lock);

/** Write lock functions required by VMM core */
bool arch_write_lock_check(arch_rwlock_t *lock);
void arch_write_lock(arch_rwlock_t *lock);
int  arch_write_trylock(arch_rwlock_t *lock);
void arch_write_unlock(arch_rwlock_t *lock);

/** Read lock functions required by VMM core */
bool arch_read_lock_check(arch_rwlock_t *lock);
void arch_read_lock(arch_rwlock_t *lock);
int  arch_read_trylock(arch_rwlock_t *lock);
void arch_read_unlock(arch_rwlock_t *lock);

#endif
