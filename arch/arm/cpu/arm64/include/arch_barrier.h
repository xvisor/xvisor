/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file arch_barrier.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief  architecure specific memory barriers
 */
#ifndef __ARCH_BARRIER_H__
#define __ARCH_BARRIER_H__

#define isb() 			asm volatile ("isb" : : : "memory")
#define dsb() 			asm volatile ("dsb sy" : : : "memory")
#define dmb() 			asm volatile ("dmb sy" : : : "memory")

/* Read & Write Memory barrier */
#define arch_mb()			dmb()

/* Read Memory barrier */
#define arch_rmb()			asm volatile ("dmb ld" : : : "memory")

/* Write Memory barrier */
#define arch_wmb()			asm volatile ("dmb st" : : : "memory")

/* SMP Read & Write Memory barrier */
#define arch_smp_mb()			asm volatile ("dmb ish" : : : "memory")

/* SMP Read Memory barrier */
#define arch_smp_rmb()			asm volatile ("dmb ishld" : : : "memory")

/* SMP Write Memory barrier */
#define arch_smp_wmb()			asm volatile ("dmb ishst" : : : "memory")

#endif /* __ARCH_BARRIER_H__ */
