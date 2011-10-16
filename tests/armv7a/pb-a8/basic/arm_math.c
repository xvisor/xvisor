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
 * @file arm_math.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief commonly required math operations
 */

#include <arm_math.h>

static inline u32 do_fls64(u64 value) 
{
	u32 num_bits = 0;
	if (value & 0xFFFF000000000000ULL) {
		num_bits += 16;
		value = value >> 16;
	}
	if (value & 0x0000FFFF00000000ULL) {
		num_bits += 16;
		value = value >> 16;
	}
	if (value & 0x00000000FFFF0000ULL) {
		num_bits += 16;
		value = value >> 16;
	}
	if (value & 0x000000000000FF00ULL) {
		num_bits += 8;
		value = value >> 8;
	}
	if (value & 0x00000000000000F0ULL) {
		num_bits += 4;
		value = value >> 4;
	}
	if (value & 0x000000000000000CULL) {
		num_bits += 2;
		value = value >> 2;
	}
	if (value & 0x0000000000000003ULL) {
		num_bits += 2;
		value = value >> 2;
	}
	return num_bits;
}

u64 do_udiv64(u64 dividend, u64 divisor, u64 * remainder)
{
	u32 i, num_bits;
	u64 t, remaind, quotient;

	remaind = 0;
	quotient = 0;

	if (divisor == 0) {
		while (1);
	}

	if (divisor > dividend) {
		if (remainder) {
			*remainder = dividend;
		}
		return 0;
	}

	if (divisor == dividend) {
		return 1;
	}

	num_bits = do_fls64(dividend);
	dividend = dividend << (64 - num_bits);

	while (1) {
		remaind = (remaind << 1) | 
			  ((dividend & 0x8000000000000000ULL) ? 1 : 0);
		if (remaind < divisor) {
			break;
		}
		dividend = dividend << 1;
		num_bits--;
	}

	remaind = remaind >> 1;

	for (i = 0; i < num_bits; i++) {
		remaind = (remaind << 1) | 
			  ((dividend & 0x8000000000000000ULL) ? 1 : 0);
		dividend = dividend << 1;
		t = remaind - divisor;
		if (!(t & 0x8000000000000000ULL)) {
			quotient = (quotient << 1) | 1;
			remaind = t;
		} else {
			quotient = (quotient << 1) | 0;
		}
	}

	if (remainder) {
		*remainder = remaind;
	}

	return quotient;
}

static inline u32 do_fls32(u32 value) 
{
	u32 num_bits = 0;
	if (value & 0xFFFF0000) {
		num_bits += 16;
		value = value >> 16;
	}
	if (value & 0x0000FF00) {
		num_bits += 8;
		value = value >> 8;
	}
	if (value & 0x000000F0) {
		num_bits += 4;
		value = value >> 4;
	}
	if (value & 0x0000000C) {
		num_bits += 2;
		value = value >> 2;
	}
	if (value & 0x00000003) {
		num_bits += 2;
		value = value >> 2;
	}
	return num_bits;
}

u32 do_udiv32(u32 dividend, u32 divisor, u32 * remainder)
{
	u32 i, num_bits;
	u32 t, quotient, remaind;

	remaind = 0;
	quotient = 0;

	if (divisor == 0) {
		while (1);
	}

	if (divisor > dividend) {
		if (remainder) {
			*remainder = dividend;
		}
		return 0;
	}

	if (divisor == dividend) {
		if (remainder) {
			*remainder = 0;
		}
		return 1;
	}

	num_bits = do_fls32(dividend);
	dividend = dividend << (32 - num_bits);

	while (1) {
		remaind = (remaind << 1) | ((dividend & 0x80000000) ? 1 : 0);
		if (remaind < divisor) {
			break;
		}
		dividend = dividend << 1;
		num_bits--;
	}

	remaind = remaind >> 1;

	for (i = 0; i < num_bits; i++) {
		remaind = (remaind << 1) | ((dividend & 0x80000000) ? 1 : 0);
		dividend = dividend << 1;
		t = remaind - divisor;
		if (!(t & 0x80000000)) {
			quotient = ((quotient) << 1) | 1;
			remaind = t;
		} else {
			quotient = ((quotient) << 1) | 0;
		}
	}

	if (remainder) {
		*remainder = remaind;
	}

	return quotient;
}

