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
 * @brief math related functions
 */

#ifndef __VMM_MATH_H__
#define __VMM_MATH_H__

#include <arch_math.h>
#include <vmm_types.h>

/** Unsigned 64-bit divison.
 *  Prototype: u64 vmm_udiv64(u64 value, u64 divisor) 
 */
#define vmm_udiv64(value, divisor)	arch_udiv64(value, divisor)

/** Unsigned 64-bit modulus.
 *  Prototype: u64 vmm_umod64(u64 value, u64 divisor) 
 */
#define vmm_umod64(value, divisor)	arch_umod64(value, divisor)

/** Signed 64-bit divison.
 *  Prototype: s64 vmm_sdiv64(s64 value, s64 divisor) 
 */
#define vmm_sdiv64(value, divisor)	arch_sdiv64(value, divisor)

/** Signed 64-bit modulus.
 *  Prototype: s64 vmm_smod64(s64 value, s64 divisor) 
 */
#define vmm_smod64(value, divisor)	arch_smod64(value, divisor)

/** Unsigned 32-bit divison.
 *  Prototype: u32 vmm_udiv32(u32 value, u32 divisor) 
 */
#define vmm_udiv32(value, divisor)	arch_udiv32(value, divisor)

/** Unsigned 32-bit modulus.
 *  Prototype: u32 vmm_umod32(u32 value, u32 divisor) 
 */
#define vmm_umod32(value, divisor)	arch_umod32(value, divisor)

/** Signed 32-bit divison.
 *  Prototype: s32 vmm_sdiv32(s32 value, s32 divisor) 
 */
#define vmm_sdiv32(value, divisor)	arch_sdiv32(value, divisor)

/** Signed 32-bit modulus.
 *  Prototype: s32 vmm_smod32(s32 value, s32 divisor) 
 */
#define vmm_smod32(value, divisor)	arch_smod32(value, divisor)

#endif /* __VMM_MATH_H__ */
