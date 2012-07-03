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
 * @file arm_board.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief various platform specific functions
 */

#include <arm_plat.h>
#include <arm_types.h>
#include <arm_io.h>
#include <arm_board.h>

void arm_board_reset(void)
{
	arm_writel(0x101,
		   (void *)(VERSATILE_SYS_BASE + VERSATILE_SYS_RESETCTL_OFFSET));
}

void arm_board_init(void)
{
	/* Unlock Lockable reigsters */
	arm_writel(VERSATILE_SYS_LOCKVAL,
		   (void *)(VERSATILE_SYS_BASE + VERSATILE_SYS_LOCK_OFFSET));
}

char *arm_board_name(void)
{
	return "ARM Versatile PB";
}

u32 arm_board_flash_addr(void)
{
	return (u32)(VERSATILE_FLASH_BASE);
}
