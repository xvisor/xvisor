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
 * @file arm_math.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief commonly required math operations
 */

#ifndef __ARM_MATH_H_
#define __ARM_MATH_H_

#include <arm_types.h>

#define do_abs(x)  ((x) < 0 ? -(x) : (x))

static inline u64 arm_udiv64(u64 value, u64 divisor)
{
	return (value / divisor);
}

static inline u64 arm_umod64(u64 value, u64 divisor)
{
	return (value % divisor);
}

static inline u32 arm_udiv32(u32 value, u32 divisor)
{
	return (value / divisor);
}

static inline u32 arm_umod32(u32 value, u32 divisor)
{
	return (value % divisor);
}

static inline s32 arm_sdiv32(s32 value, s32 divisor)
{
	if ((value * divisor) < 0) {
		return -(do_abs(value) / do_abs(divisor));
	} else { /* positive value */
		return  (do_abs(value) / do_abs(divisor));
	}
}

#endif /* __ARM_MATH_H_ */
