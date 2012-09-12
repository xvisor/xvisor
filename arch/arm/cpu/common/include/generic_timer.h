/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file generic_timer.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief API for ARM architecture generic timers
 */

#ifndef __GENERIC_TIMER_H__
#define __GENERIC_TIMER_H__

enum {
	GENERIC_TIMER_REG_CTRL,
	GENERIC_TIMER_REG_FREQ,
	GENERIC_TIMER_REG_TVAL,
};

#define GENERIC_TIMER_CTRL_ENABLE		(1 << 0)
#define GENERIC_TIMER_CTRL_IT_MASK		(1 << 1)
#define GENERIC_TIMER_CTRL_IT_STAT		(1 << 2)

int generic_timer_clocksource_init(const char *name, 
				   int rating, 
				   u32 freq_hz,
				   u32 shift);

int generic_timer_clockchip_init(const char *name,
			         u32 hirq,
			         int rating,
			         u32 freq_hz);

#endif /* __GENERIC_TIMER_H__ */
