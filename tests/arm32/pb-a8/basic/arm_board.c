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
#if 0 /* QEMU checks bit 8 which is wrong */
        arm_writel(0x100,
                   (void *)(REALVIEW_SYS_BASE + REALVIEW_SYS_RESETCTL_OFFSET));
#else
        arm_writel(0x0,
                   (void *)(REALVIEW_SYS_BASE+ REALVIEW_SYS_RESETCTL_OFFSET));
        arm_writel(REALVIEW_SYS_CTRL_RESET_PLLRESET,
                   (void *)(REALVIEW_SYS_BASE+ REALVIEW_SYS_RESETCTL_OFFSET));
#endif
}

void arm_board_init(void)
{
	/* Unlock Lockable reigsters */
	arm_writel(REALVIEW_SYS_LOCKVAL,
                   (void *)(REALVIEW_SYS_BASE + REALVIEW_SYS_LOCK_OFFSET));
}

char *arm_board_name(void)
{
	return "ARM PB-A8";
}

u32 arm_board_ram_start(void)
{
	return 0x70000000;
}

u32 arm_board_ram_size(void)
{
	return 0x6000000;
}

u32 arm_board_linux_machine_type(void)
{
	return 0x769;
}

u32 arm_board_flash_addr(void)
{
	return (u32)(REALVIEW_PBA8_FLASH0_BASE);
}
