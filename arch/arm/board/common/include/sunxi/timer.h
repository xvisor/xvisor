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
 * @file timer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Allwinner Sunxi timer interface
 */
#ifndef __SUNXI_TIMER_H__
#define __SUNXI_TIMER_H__

/** Possible Sunxi chip versions */
enum aw_chip_ver {
	AW_CHIP_VER_A = 0,
	AW_CHIP_VER_B,
	AW_CHIP_VER_C
};

/** Get chip version */
enum aw_chip_ver aw_timer_chip_ver(void);

/** Initialize Sunxi timer misc APIs */
int aw_timer_misc_init(void);

#endif /* __SUNXI_TIMER_H__ */
