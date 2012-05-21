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
 * @file arch_timer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific timer functions
 */
#ifndef _ARCH_TIMER_H__
#define _ARCH_TIMER_H__

#include <vmm_types.h>

/** Initialize clockchip devices 
 * Note: In-case of SMP this function is called for each CPU
 */
int arch_clockchip_init(void);

/** Initialize clocksource devices */
int arch_clocksource_init(void);

#endif
