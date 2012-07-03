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
#include <arm_config.h>
#include <arm_types.h>
#include <arm_io.h>
#include <arm_board.h>

void arm_board_reset(void)
{
	arm_writel(~0x0, (void *)(V2M_SYS_FLAGSCLR));
	arm_writel(0x0, (void *)(V2M_SYS_FLAGSSET));
	arm_writel(0xc0900000, (void *)(V2M_SYS_CFGCTRL));
}

void arm_board_init(void)
{
	/* Nothing to do */
}

char *arm_board_name(void)
{
	return "ARM VExpress-A9";
}

u32 arm_board_ram_start(void)
{
	return 0x60000000;
}

u32 arm_board_ram_size(void)
{
	return 0x6000000;
}

u32 arm_board_linux_machine_type(void)
{
	return 0x8e0;
}

u32 arm_board_flash_addr(void)
{
	return (u32)(V2M_NOR0);
}
