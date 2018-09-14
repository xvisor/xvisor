/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file arch_math.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief commonly required math operations
 */

#ifndef __ARCH_MATH_H__
#define __ARCH_MATH_H__

#include <arch_types.h>

#define do_abs(x)  ((x) < 0 ? -(x) : (x))

static inline u64 arch_udiv64(u64 value, u64 divisor)
{
	return (value / divisor);
}

static inline u64 arch_umod64(u64 value, u64 divisor)
{
	return (value % divisor);
}

static inline u64 do_udiv64(u64 value, u64 divisor, u64 *remainder)
{
	if (remainder)
		*remainder = arch_umod64(value, divisor);
	return arch_udiv64(value, divisor);
}

static inline u32 arch_udiv32(u32 value, u32 divisor)
{
	return (value / divisor);
}

static inline u32 arch_umod32(u32 value, u32 divisor)
{
	return (value % divisor);
}

static inline u32 do_udiv32(u32 value, u32 divisor, u32 *remainder)
{
	if (remainder)
		*remainder = arch_umod32(value, divisor);
	return arch_udiv32(value, divisor);
}

static inline s32 arch_sdiv32(s32 value, s32 divisor)
{
	if ((value * divisor) < 0) {
		return -(do_abs(value) / do_abs(divisor));
	} else { /* positive value */
		return  (do_abs(value) / do_abs(divisor));
	}
}

#endif /* __ARCH_MATH_H__ */
