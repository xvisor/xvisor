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

/** Configure clockevent device to expire after given nano-seconds */
int arch_clockevent_start(u64 tick_nsecs);

/** Forcefully expire clockevent device */
int arch_clockevent_expire(void);

/** Stop clockevent device */
int arch_clockevent_stop(void);

/** Initialize clockevent device */
int arch_clockevent_init(void);

/** Get clocksource cycles passed */
u64 arch_clocksource_cycles(void);

/** Mask of clocksource device */
u64 arch_clocksource_mask(void);

/** Multiplier of clocksource device */
u32 arch_clocksource_mult(void);

/** Shift of clocksource device */
u32 arch_clocksource_shift(void);

/** Initialize clocksource device */
int arch_clocksource_init(void);

#endif
