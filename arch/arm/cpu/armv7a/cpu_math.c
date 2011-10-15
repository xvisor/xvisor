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
 * @file cpu_math.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Architecture specific math related functions
 */

#include <vmm_math.h>

void do_udiv64(u64 value, u64 divisor, u64 * remainder, u64 * quotient)
{
	u32 i, bit, num_bits;
	u64 t, q, d;

	*remainder = 0;
	*quotient = 0;

	if (divisor == 0) {
		while (1);
	}

	if (divisor > value) {
		*remainder = value;
		return;
	}

	if (divisor == value) {
		return;
	}

	num_bits = 64;

	while (*remainder < divisor) {
		bit = (value & 0x8000000000000000ULL) >> 63;
		*remainder = (*remainder << 1) | bit;
		d = value;
		value = value << 1;
		num_bits--;
	}


	/* The loop, above, always goes one iteration too far.
	 * To avoid inserting an "if" statement inside the loop
	 * the last iteration is simply reversed. 
	 */

	value = d;
	*remainder = *remainder >> 1;
	num_bits++;

	for (i = 0; i < num_bits; i++) {
		bit = (value & 0x8000000000000000ULL) >> 63;
		*remainder = (*remainder << 1) | bit;
		t = *remainder - divisor;
		q = !((t & 0x8000000000000000ULL) >> 63);
		value = value << 1;
		*quotient = (*quotient << 1) | q;
		if (q) {
			*remainder = t;
		}
	}

	return;
}

void do_udiv32(u32 value, u32 divisor, u32 * remainder, u32 * quotient)
{
	u32 i, bit, num_bits;
	u32 t, q, d;

	*remainder = 0;
	*quotient = 0;

	if (divisor == 0) {
		while (1);
	}

	if (divisor > value) {
		*remainder = value;
		return;
	}

	if (divisor == value) {
		return;
	}

	num_bits = 32;

	while (*remainder < divisor) {
		bit = (value & 0x80000000) >> 31;
		*remainder = (*remainder << 1) | bit;
		d = value;
		value = value << 1;
		num_bits--;
	}


	/* The loop, above, always goes one iteration too far.
	 * To avoid inserting an "if" statement inside the loop
	 * the last iteration is simply reversed. 
	 */

	value = d;
	*remainder = *remainder >> 1;
	num_bits++;

	for (i = 0; i < num_bits; i++) {
		bit = (value & 0x80000000) >> 31;
		*remainder = (*remainder << 1) | bit;
		t = *remainder - divisor;
		q = !((t & 0x80000000) >> 31);
		value = value << 1;
		*quotient = (*quotient << 1) | q;
		if (q) {
			*remainder = t;
		}
	}

	return;
}

