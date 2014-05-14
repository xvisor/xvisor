/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief arch specific delay routines
 */
#ifndef _ARCH_DELAY_H__
#define _ARCH_DELAY_H__

#include <vmm_types.h>

/** Low-level delay loop */
void arch_delay_loop(unsigned long count);

/** Estimated cycles for given loop count 
 *  Note: This can be processor specific
 */
unsigned long arch_delay_loop_cycles(unsigned long count);

#endif
