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
 * @file cpu_locks.h
 * @version 0.01
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @brief Architecture specific implementation of synchronization mechanisms.
 */

#ifndef __CPU_LOCKS_H__
#define __CPU_LOCKS_H__

#include <vmm_types.h>
#include <cpu_atomic.h>

void __lock __cpu_spin_lock (vmm_cpu_spinlock_t *lock);
void __lock __cpu_spin_unlock (vmm_cpu_spinlock_t *lock);

#endif /* __CPU_LOCKS_H__ */

