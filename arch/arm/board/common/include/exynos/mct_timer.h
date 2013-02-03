/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file mct_timer.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief MCT timer main header file
 */

#ifndef __MCT_TIMER_H
#define __MCT_TIMER_H

#include <exynos/regs-mct.h>

int exynos4_clocksource_init(virtual_addr_t base, const char *name, int rating,
			     u32 freq_hz, u32 shift);

int exynos4_clockchip_init(virtual_addr_t base, u32 hirq, const char *name,
			   int rating, u32 freq_hz, u32 target_cpu);

int exynos4_local_timer_init(virtual_addr_t base, u32 hirq, const char *name, 
			     int rating, u32 freq_hz);

#endif
