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
 * @file arch_barrier.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief  architecure specific memory barriers
 */
#ifndef __ARCH_BARRIER_H__
#define __ARCH_BARRIER_H__

/* FIXME: Read & Write Memory barrier */
#define arch_mb()	asm volatile("mfence\n");

/* FIXME: Read Memory barrier */
#define arch_rmb()	asm volatile("lfence\n");

/* FIXME: Write Memory barrier */
#define arch_wmb()	asm volatile("sfence\n");

/* FIXME: SMP Read & Write Memory barrier */
#define arch_smp_mb()	asm volatile("mfence":::"memory");

/* FIXME: SMP Read Memory barrier */
#define arch_smp_rmb()	asm volatile("lfence":::"memory");

/* FIXME: SMP Write Memory barrier */
#define arch_smp_wmb()	asm volatile("sfence":::"memory");

/* FIXME: CPU relax for busy loop */
#define arch_cpu_relax()	asm volatile ("" : : : "memory")

#endif /* __ARCH_BARRIER_H__ */
