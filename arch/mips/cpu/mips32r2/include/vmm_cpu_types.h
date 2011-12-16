/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_types.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief common header file for typedefs
 */
#ifndef _VMM_CPU_TYPES_H__
#define _VMM_CPU_TYPES_H__

/** cpu specific types */
typedef unsigned long long u64;
typedef unsigned int irq_flags_t;
typedef unsigned int virtual_addr_t;
typedef unsigned int virtual_size_t;
typedef unsigned long physical_addr_t;
typedef unsigned long physical_size_t;
typedef volatile unsigned long long jiffies_t;
typedef unsigned int clock_freq_t;

typedef struct {
	volatile long counter;
} atomic_t;

typedef struct {
	atomic_t __cpu_lock;
}spinlock_t;

#define __ARCH_SPIN_UNLOCKED	0

/* FIXME: Need memory barrier for this. */
#define VMM_CPU_SPIN_LOCK_INIT(_lptr)				\
	(_lptr)->__cpu_lock.counter = __ARCH_SPIN_UNLOCKED

#endif /* __VMM_CPU_TYPES_H__ */
