/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file gpio_sync.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief GPIO emulator sync interface header
 */
#ifndef __GPIO_SYNC_H__
#define __GPIO_SYNC_H__

#include <vmm_types.h>

enum gpio_emu_sync_types {
	GPIO_EMU_SYNC_DIRECTION_IN = 0,
	GPIO_EMU_SYNC_DIRECTION_OUT,
	GPIO_EMU_SYNC_VALUE,
};

struct gpio_emu_sync {
	u32 irq;
};

#endif
