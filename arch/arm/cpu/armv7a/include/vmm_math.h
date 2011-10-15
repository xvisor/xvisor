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
 * @file vmm_math.h
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Architecture specific math related functions
 */

#ifndef __VMM_MATH_H__
#define __VMM_MATH_H__

#include <vmm_types.h>

void do_udiv64(u64 value, u64 divisor, u64 * remainder, u64 * quotient);

static inline u64 vmm_udiv64(u64 value, u64 divisor)
{
	u64 remainder, quotient;
	do_udiv64(value, divisor, &remainder, &quotient);
	return quotient;
}

static inline u64 vmm_umod64(u64 value, u64 divisor)
{
	u64 remainder, quotient;
	do_udiv64(value, divisor, &remainder, &quotient);
	return remainder;
}

void do_udiv32(u32 value, u32 divisor, u32 * remainder, u32 * quotient);

static inline u32 vmm_udiv32(u32 value, u32 divisor)
{
	u32 remainder, quotient;
	do_udiv32(value, divisor, &remainder, &quotient);
	return quotient;
}

static inline u32 vmm_umod32(u32 value, u32 divisor)
{
	u32 remainder, quotient;
	do_udiv32(value, divisor, &remainder, &quotient);
	return remainder;
}

#endif /* __VMM_MATH_H__ */
