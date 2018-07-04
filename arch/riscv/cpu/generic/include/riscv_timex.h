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
 * @file riscv_timex.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Read APIs for time counter register
 *
 * The source has been largely adapted from Linux 4.x or higher:
 * arch/riscv/include/asm/timex.h
 *
 * Copyright (C) 2012 Regents of the University of California
 *
 * The original code is licensed under the GPL.
 */

#ifndef __RISCV_TIMEX_H__
#define __RISCV_TIMEX_H__

#include <vmm_types.h>
#include <vmm_compiler.h>

typedef unsigned long cycles_t;

static inline __notrace cycles_t get_cycles_inline(void)
{
	cycles_t n;

	__asm__ __volatile__ (
		"rdtime %0"
		: "=r" (n));
	return n;
}

#define get_cycles get_cycles_inline

#ifdef CONFIG_64BIT
static inline __notrace u64 get_cycles64(void)
{
        return get_cycles();
}
#else
static inline __notrace u64 get_cycles64(void)
{
	u32 lo, hi, tmp;
	__asm__ __volatile__ (
		"1:\n"
		"rdtimeh %0\n"
		"rdtime %1\n"
		"rdtimeh %2\n"
		"bne %0, %2, 1b"
		: "=&r" (hi), "=&r" (lo), "=&r" (tmp));
	return ((u64)hi << 32) | lo;
}
#endif

static inline int read_current_timer(unsigned long *timer_val)
{
	*timer_val = get_cycles();
	return 0;
}

#endif /* __RISCV_TIMEX_H__ */
