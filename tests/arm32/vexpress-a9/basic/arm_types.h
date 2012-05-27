/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file arm_types.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief common header file for typedefs
 *
 * Adapted from tests/arm32/pb-a8/basic/arm_types.h
 */
#ifndef _ARM_TYPES_H__
#define _ARM_TYPES_H__

typedef char s8;
typedef short s16;
typedef int s32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned int size_t;
typedef unsigned int bool;
typedef unsigned int ulong;

/** Boolean macros */
#define TRUE		1
#define FALSE		0
#define NULL 		((void *)0)

#define stringify(s)	tostring(s)
#define tostring(s)	#s

typedef long long s64;
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

struct pt_regs {
	u32 cpsr;	// Current Program Status
	u32 gpr[13];	// R0 - R12
	u32 sp;
	u32 lr;
	u32 pc;
} __attribute ((packed)) ;

#endif /* __ARM_TYPES_H__ */
