/**
 * Copyright (c) 2018 Himanshu Chauhan.
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
 * @file tsc.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief x86 Time Stamp Counter (TSC) related functions.
 */

#ifndef _TSC_H
#define _TSC_H

#include <vmm_types.h>
#include <vmm_compiler.h>
#include <processor.h>

typedef u64 cycles_t;

/* Returns the current value of the TSC counter. */
static __always_inline cycles_t get_tsc(void)
{
	cycles_t ret = 0;
	rdtscll(ret);
	return ret;
}

/* Serialized version of get_tsc. It ensures that all the previous instructions
 * have completed before reading the TSC.
 */
static __always_inline cycles_t get_tsc_serialized(void)
{
	__sync();
	return get_tsc();
}
#endif /* _TSC_H */
