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
 * @brief Architecture specific math related functions
 */

#ifndef __ARCH_MATH_H__
#define __ARCH_MATH_H__

#include <vmm_types.h>

static inline u64 arch_udiv64(u64 value, u64 divisor)
{
	return value / divisor;
}

static inline u64 arch_umod64(u64 value, u64 divisor)
{
	return value % divisor;
}

static inline s64 arch_sdiv64(s64 value, s64 divisor)
{
	return value / divisor;
}

static inline s64 arch_smod64(s64 value, s64 divisor)
{
	return value % divisor;
}

static inline u32 arch_udiv32(u32 value, u32 divisor)
{
	return value / divisor;
}

static inline u32 arch_umod32(u32 value, u32 divisor)
{
	return value % divisor;
}

static inline s32 arch_sdiv32(s32 value, s32 divisor)
{
	return value / divisor;
}

static inline s32 arch_smod32(s32 value, s32 divisor)
{
	return value % divisor;
}

#endif /* __ARCH_MATH_H__ */
