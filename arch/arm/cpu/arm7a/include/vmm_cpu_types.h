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
 * @file vmm_types.h
 * @version 1.0
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief common header file for typedefs
 */
#ifndef _VMM_CPU_TYPES_H__
#define _VMM_CPU_TYPES_H__

/** cpu specific types */
typedef unsigned long long u64;
typedef unsigned int irq_flags_t;
typedef unsigned int virtual_addr_t;
typedef unsigned int virtual_size_t;
typedef unsigned int physical_addr_t;
typedef unsigned int physical_size_t;
typedef unsigned int clock_freq_t;
typedef unsigned long long jiffies_t;

typedef struct {
	volatile long counter;
} atomic_t;

typedef struct {
	atomic_t __cpu_lock;
} vmm_cpu_spinlock_t;

#define __ARCH_SPIN_UNLOCKED	0
#define __CPU_INIT_SPIN_LOCK_UNLOCKED				\
	{ .__cpu_lock = { .counter = __ARCH_SPIN_UNLOCKED } }
#define __CPU_SPIN_LOCK_UNLOCKED(_lptr)		\
	(_lptr)->__cpu_lock.counter = __ARCH_SPIN_UNLOCKED

#endif /* __VMM_CPU_TYPES_H__ */
