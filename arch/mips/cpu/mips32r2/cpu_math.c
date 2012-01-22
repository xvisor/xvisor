/**
 * Copyright (c) 2011 Himanshu Chauhan
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
 * @file cpu_math.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Architecture specific math related functions
 */

#include <arch_math.h>

u64 do_udiv64(u64 dividend, u64 divisor, u64 * remainder)
{
	if (remainder)
		*remainder = dividend % divisor;

	return (dividend / divisor);
}

u32 do_udiv32(u32 dividend, u32 divisor, u32 * remainder)
{
	if (remainder)
		*remainder = dividend % divisor;

	return (dividend/divisor);
}

