/**
 * Copyright (c) 2011 Sukanto Ghosh.
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
 * @file s32k-timer.h
 * @version 1.0
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief OMAP3 32K sync timer APIs
 */
#ifndef __OMAP3_S32K_TIMER_H__
#define __OMAP3_S32K_TIMER_H__

#include <vmm_types.h>

#define OMAP3_S32K_FREQ_HZ	32768

#define OMAP3_S32K_BASE 	0x48320000
#define OMAP3_S32K_CR	 	0x10

u32 omap3_s32k_get_counter(void);
int omap3_s32k_init(void);

#endif  /*  __OMAP3_S32K_TIMER_H__ */

