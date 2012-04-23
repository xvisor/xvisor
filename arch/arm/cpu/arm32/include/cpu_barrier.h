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
 * @file cpu_barrier.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief  ARM specific memory barriers
 */
#ifndef __CPU_BARRIER_H__
#define __CPU_BARRIER_H__

#if defined CONFIG_ARMV5

#define isb()			asm volatile ("" : : : "memory")
#define dsb()			asm volatile ("mcr p15, 0, %0, c7, c10, 4" \
					      : : "r" (0) : "memory")
#define dmb()			asm volatile ("" : : : "memory")

#elif defined CONFIG_ARMV7A

#define isb() 			asm volatile ("isb" : : : "memory")
#define dsb() 			asm volatile ("dsb" : : : "memory")
#define dmb() 			asm volatile ("dmb" : : : "memory")

#endif

#define mb()			dsb()
#define rmb()			dsb()
#define wmb()			dsb()
#define smp_mb()		dmb()
#define smp_rmb()		dmb()
#define smp_wmb()		dmb()

#endif /* __CPU_BARRIER_H__ */
