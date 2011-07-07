/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief ARM specific synchronization mechanisms.
 */

#ifndef __CPU_LOCKS_H__
#define __CPU_LOCKS_H__

#include <vmm_types.h>
#include <vmm_sections.h>
#include <cpu_atomic.h>

void __lock_section __cpu_spin_lock(vmm_cpu_spinlock_t * lock);
void __lock_section __cpu_spin_unlock(vmm_cpu_spinlock_t * lock);

irq_flags_t __lock_section __cpu_spin_lock_irqsave(vmm_cpu_spinlock_t * lock);
void __lock_section __cpu_spin_unlock_irqrestore(vmm_cpu_spinlock_t * lock,
						 irq_flags_t flags);

#endif /* __CPU_LOCKS_H__ */
