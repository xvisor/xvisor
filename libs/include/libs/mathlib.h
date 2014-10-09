/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file mathlib.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief library for common math operations
 */

#ifndef __MATHLIB_H__
#define __MATHLIB_H__

#include <vmm_types.h>
#include <arch_config.h>

#if !defined(ARCH_HAS_DIVISON_OPERATION)

#define do_abs(x)  ((x) < 0 ? -(x) : (x))

extern u64 do_udiv64(u64 dividend, u64 divisor, u64 * remainder);

static inline u64 udiv64(u64 value, u64 divisor)
{
	u64 r;
	return do_udiv64(value, divisor, &r);
}

static inline u64 umod64(u64 value, u64 divisor)
{
	u64 r;
	do_udiv64(value, divisor, &r);
	return r;
}

static inline s64 sdiv64(s64 value, s64 divisor)
{
	u64 r;
	if ((value * divisor) < 0) {
		return -do_udiv64(do_abs(value), do_abs(divisor), &r);
	} else { /* positive value */
		return do_udiv64(do_abs(value), do_abs(divisor), &r);
	}
}

static inline s64 smod64(s64 value, s64 divisor)
{
	u64 r;
	do_udiv64( do_abs(value), do_abs(divisor), &r);
	if (value < 0) {
		return -r;
	} else { /* positive value */
		return r;
	}
}

extern u32 do_udiv32(u32 dividend, u32 divisor, u32 * remainder);

static inline u32 udiv32(u32 value, u32 divisor)
{
	u32 r;
	return do_udiv32(value, divisor, &r);
}

static inline u32 umod32(u32 value, u32 divisor)
{
	u32 r;
	do_udiv32(value, divisor, &r);
	return r;
}

static inline s32 sdiv32(s32 value, s32 divisor)
{
	u32 r;
	if ((value * divisor) < 0) {
		return -do_udiv32(do_abs(value), do_abs(divisor), &r);
	} else { /* positive value */
		return do_udiv32(do_abs(value), do_abs(divisor), &r);
	}
}

static inline s32 smod32(s32 value, s32 divisor)
{
	u32 r;
	do_udiv32( do_abs(value), do_abs(divisor), &r );
	if (value < 0) {
		return -r;
	} else { /* positive value */
		return r;
	}
}

#else

#include <arch_math.h>

/** Unsigned 64-bit divison.
 *  Prototype: u64 udiv64(u64 value, u64 divisor) 
 */
#define udiv64(value, divisor)	arch_udiv64(value, divisor)

/** Unsigned 64-bit modulus.
 *  Prototype: u64 umod64(u64 value, u64 divisor) 
 */
#define umod64(value, divisor)	arch_umod64(value, divisor)

/** Signed 64-bit divison.
 *  Prototype: s64 sdiv64(s64 value, s64 divisor) 
 */
#define sdiv64(value, divisor)	arch_sdiv64(value, divisor)

/** Signed 64-bit modulus.
 *  Prototype: s64 smod64(s64 value, s64 divisor) 
 */
#define smod64(value, divisor)	arch_smod64(value, divisor)

/** Unsigned 32-bit divison.
 *  Prototype: u32 udiv32(u32 value, u32 divisor) 
 */
#define udiv32(value, divisor)	arch_udiv32(value, divisor)

/** Unsigned 32-bit modulus.
 *  Prototype: u32 umod32(u32 value, u32 divisor) 
 */
#define umod32(value, divisor)	arch_umod32(value, divisor)

/** Signed 32-bit divison.
 *  Prototype: s32 sdiv32(s32 value, s32 divisor) 
 */
#define sdiv32(value, divisor)	arch_sdiv32(value, divisor)

/** Signed 32-bit modulus.
 *  Prototype: s32 smod32(s32 value, s32 divisor) 
 */
#define smod32(value, divisor)	arch_smod32(value, divisor)

#endif

/* Unsigned integer round-up macros */
#define DIV_ROUND_UP(n,d)	udiv64(((n) + (d) - 1), (d))
#define DIV_ROUND_UP_ULL(ll,d) \
	({ unsigned long long _tmp = (ll)+(d)-1; udiv64(_tmp, d); _tmp; })

/**
 * Rough approximation to sqrt
 * @x: integer of which to calculate the sqrt
 *
 * A very rough approximation to the sqrt() function.
 */
unsigned long int_sqrt(unsigned long x);

/**
 * Compute with 96 bit intermediate result: (a*b)/c
 */
static inline u64 muldiv64(u64 a, u32 b, u32 c)
{
	u64 rem;

	union {
		u64 ll;
		struct {
#if BYTE_ORDER == BIG_ENDIAN
			u32 high, low;
#else
			u32 low, high;
#endif
		} l;
	} u, res;
	u64 rl, rh;

	u.ll = a;
	rl = (u64)u.l.low * (u64)b;
	rh = (u64)u.l.high * (u64)b;
	rh += (rl >> 32);
	res.l.high = udiv64(rh, c);
	rem = umod64(rh, c);
	res.l.low = ((rem << 32) + (rl & 0xffffffff)) / c;
	return res.ll;
}

#endif /* __MATHLIB_H__ */
