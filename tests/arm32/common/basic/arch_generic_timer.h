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
 * @file arch_generic_timer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for arch specific generic timer access
 */

#ifndef __ARCH_GENERIC_TIMER_H__
#define __ARCH_GENERIC_TIMER_H__

#include <arch_types.h>
#include <arm_inline_asm.h>

#define arch_read_cntfrq()	({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c14, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define arch_read_cntv_ctl()	({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c14, c3, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define arch_write_cntv_ctl(val) asm volatile(\
				" mcr     p15, 0, %0, c14, c3, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define arch_read_cntv_cval()	({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 3, %0, %1, c14\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define arch_write_cntv_cval(val) asm volatile(\
				" mcrr     p15, 3, %0, %1, c14\n\t" \
				:: "r" ((val) & 0xFFFFFFFF), "r" ((val) >> 32) \
				: "memory", "cc")

#define arch_read_cntv_tval()	({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c14, c3, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define arch_write_cntv_tval(val) asm volatile(\
				" mcr     p15, 0, %0, c14, c3, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define arch_read_cntvct()	({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 1, %0, %1, c14\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#endif /* __ARCH_GENERIC_TIMER_H__ */
