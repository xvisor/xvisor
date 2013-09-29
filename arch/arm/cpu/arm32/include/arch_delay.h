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
 * @file arch_delay.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief arch specific delay routines
 */
#ifndef _ARCH_DELAY_H__
#define _ARCH_DELAY_H__

#include <vmm_types.h>

/** Low-level delay loop */
void arch_delay_loop(u32 count);

/** Estimated cycles for given loop count 
 *  Note: This can be processor specific
 */
static inline u32 arch_delay_loop_cycles(u32 count)
{
	return count * 3;
}

#endif
