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
 * @file arch_types.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief common header file for typedefs
 */
#ifndef _ARCH_TYPES_H__
#define _ARCH_TYPES_H__

/** cpu specific types */
#ifdef CONFIG_64BIT
#if __riscv_xlen != 64
#error "Need 64bit toolchain for 64bit system"
#endif

typedef unsigned int irq_flags_t;
typedef unsigned long virtual_addr_t;
typedef unsigned long virtual_size_t;
typedef unsigned long physical_addr_t;
typedef unsigned long physical_size_t;

#define __ARCH_PRIADDR_PREFIX	"l"
#define __ARCH_PRIADDR_DIGITS	"16"
#define __ARCH_PRISIZE_PREFIX	"l"
#define __ARCH_PRIPADDR_PREFIX	"l"
#define __ARCH_PRIPADDR_DIGITS	"16"
#define __ARCH_PRIPSIZE_PREFIX	"l"
#define __ARCH_PRI64_PREFIX	"l"

#define ARCH_BITS_PER_LONG		64
#define ARCH_BITS_PER_LONG_LONG		64

#else /* assume 32bit system */
#if __riscv_xlen != 32
#error "Need 32bit toolchain for 32bit system"
#endif

typedef unsigned int irq_flags_t;
typedef unsigned long virtual_addr_t;
typedef unsigned long virtual_size_t;
typedef unsigned long physical_addr_t;
typedef unsigned long physical_size_t;

#define __ARCH_PRIADDR_PREFIX	"l"
#define __ARCH_PRIADDR_DIGITS	"8"
#define __ARCH_PRISIZE_PREFIX	"l"
#define __ARCH_PRIPADDR_PREFIX	"l"
#define __ARCH_PRIPADDR_DIGITS	"8"
#define __ARCH_PRIPSIZE_PREFIX	"l"
#define __ARCH_PRI64_PREFIX	"ll"

#define ARCH_BITS_PER_LONG		32
#define ARCH_BITS_PER_LONG_LONG		64
#endif

typedef struct {
	volatile long counter;
} atomic_t;

typedef struct {
	volatile long long counter;
} atomic64_t;

typedef struct {
	volatile long lock;
} arch_spinlock_t;

#define ARCH_ATOMIC_INIT(_lptr, val)		\
	(_lptr)->counter = (val)

#define ARCH_ATOMIC_INITIALIZER(val)		\
	{ .counter = (val), }

#define ARCH_ATOMIC64_INIT(_lptr, val)		\
	(_lptr)->counter = (val)

#define ARCH_ATOMIC64_INITIALIZER(val)		\
	{ .counter = (val), }

#define __ARCH_SPIN_UNLOCKED			0

/* FIXME: Need memory barrier for this. */
#define ARCH_SPIN_LOCK_INIT(_lptr)		\
	(_lptr)->lock = __ARCH_SPIN_UNLOCKED

#define ARCH_SPIN_LOCK_INITIALIZER		\
	{ .lock = __ARCH_SPIN_UNLOCKED, }
typedef struct {
	volatile long lock;
} arch_rwlock_t;

#define __ARCH_RW_LOCKED			-1
#define __ARCH_RW_UNLOCKED			0

/* FIXME: Need memory barrier for this. */
#define ARCH_RW_LOCK_INIT(_lptr)		\
	(_lptr)->lock = __ARCH_RW_UNLOCKED

#define ARCH_RW_LOCK_INITIALIZER		\
	{ .lock = __ARCH_RW_UNLOCKED, }

#endif /* _ARCH_TYPES_H__ */


