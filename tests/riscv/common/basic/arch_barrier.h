/**
 * Copyright (c) 2018 Anup Patel.
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
 * @brief architecure specific memory barriers
 */
#ifndef __ARCH_BARRIER_H__
#define __ARCH_BARRIER_H__

#ifdef CONFIG_SMP
#define RISCV_ACQUIRE_BARRIER		"\tfence r , rw\n"
#define RISCV_RELEASE_BARRIER		"\tfence rw,  w\n"
#else
#define RISCV_ACQUIRE_BARRIER
#define RISCV_RELEASE_BARRIER
#endif

#define RISCV_FENCE(p, s) \
	__asm__ __volatile__ ("fence " #p "," #s : : : "memory")

/* Read & Write Memory barrier */
#define arch_mb()			RISCV_FENCE(iorw, iorw)

/* Read Memory barrier */
#define arch_rmb()			RISCV_FENCE(ir, ir)

/* Write Memory barrier */
#define arch_wmb()			RISCV_FENCE(ow, ow)

/* SMP Read & Write Memory barrier */
#define arch_smp_mb()			RISCV_FENCE(rw, rw)

/* SMP Read Memory barrier */
#define arch_smp_rmb()			RISCV_FENCE(r, r)

/* SMP Write Memory barrier */
#define arch_smp_wmb()			RISCV_FENCE(w, w)

/* CPU relax for busy loop */
#define arch_cpu_relax()		asm volatile ("" : : : "memory")

#define __smp_store_release(p, v)				\
do {								\
	RISCV_FENCE(rw, w);					\
	*(p) = (v);						\
} while (0)

#define __smp_load_acquire(p)					\
({								\
	typeof(*p) ___p1 = *(p);				\
	RISCV_FENCE(r, rw);					\
	___p1;							\
})

#endif /* __ARCH_BARRIER_H__ */
