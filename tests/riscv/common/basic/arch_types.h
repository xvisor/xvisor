/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file arch_types.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief common header file for typedefs
 */
#ifndef __ARCH_TYPES_H__
#define __ARCH_TYPES_H__

typedef char s8;
typedef short s16;
typedef int s32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long size_t;
typedef unsigned int bool;
typedef unsigned long ulong;

/** Boolean macros */
#define TRUE		1
#define FALSE		0
#define NULL 		((void *)0)

#define stringify(s)	tostring(s)
#define tostring(s)	#s

#if __riscv_xlen == 32
typedef long long s64;
typedef unsigned long long u64;
#define PRu64	"%llx"
#else
typedef long s64;
typedef unsigned long u64;
#define PRu64	"%lx"
#endif
typedef unsigned long irq_flags_t;
typedef unsigned long virtual_addr_t;
typedef unsigned long virtual_size_t;
typedef unsigned long physical_addr_t;
typedef unsigned long physical_size_t;

typedef struct {
	volatile long counter;
} atomic_t;

struct pt_regs {
	unsigned long zero;
	unsigned long ra;
	unsigned long sp;
	unsigned long gp;
	unsigned long tp;
	unsigned long t0;
	unsigned long t1;
	unsigned long t2;
	unsigned long s0;
	unsigned long s1;
	unsigned long a0;
	unsigned long a1;
	unsigned long a2;
	unsigned long a3;
	unsigned long a4;
	unsigned long a5;
	unsigned long a6;
	unsigned long a7;
	unsigned long s2;
	unsigned long s3;
	unsigned long s4;
	unsigned long s5;
	unsigned long s6;
	unsigned long s7;
	unsigned long s8;
	unsigned long s9;
	unsigned long s10;
	unsigned long s11;
	unsigned long t3;
	unsigned long t4;
	unsigned long t5;
	unsigned long t6;
	unsigned long sepc;
	unsigned long sstatus;
} __attribute ((packed));

#define _swab32(x) ((u32)(				\
	(((u32)(x) & (u32)0x000000ffU) << 24) |	\
	(((u32)(x) & (u32)0x0000ff00U) <<  8) |	\
	(((u32)(x) & (u32)0x00ff0000U) >>  8) |	\
	(((u32)(x) & (u32)0xff000000U) >> 24)))

#define _swab64(x) ((u64)(				\
	(((u64)(x) & (u64)0x00000000000000ffULL) << 56) |	\
	(((u64)(x) & (u64)0x000000000000ff00ULL) << 40) |	\
	(((u64)(x) & (u64)0x0000000000ff0000ULL) << 24) |	\
	(((u64)(x) & (u64)0x00000000ff000000ULL) <<  8) |	\
	(((u64)(x) & (u64)0x000000ff00000000ULL) >>  8) |	\
	(((u64)(x) & (u64)0x0000ff0000000000ULL) >> 24) |	\
	(((u64)(x) & (u64)0x00ff000000000000ULL) >> 40) |	\
	(((u64)(x) & (u64)0xff00000000000000ULL) >> 56)))

#define cpu_to_be32(x)		_swab32(x)
#define cpu_to_be64(x)		_swab64(x)
#define be32_to_cpu(x)		_swab32(x)
#define be64_to_cpu(x)		_swab64(x)
#define be32_to_cpup(x)		_swab32(*(x))

#define max(a,b)	((a) < (b) ? (b) : (a))


#endif /* __ARCH_TYPES_H__ */
