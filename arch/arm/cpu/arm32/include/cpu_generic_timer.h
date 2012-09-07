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
 * @file cpu_generic_timer.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief CPU specific functions for ARM architecture generic timers
 */

#ifndef __CPU_GENERIC_TIMER_H__
#define __CPU_GENERIC_TIMER_H__

#include <vmm_types.h>
#include <cpu_inline_asm.h>

#define generic_timer_counter_read()	read_cntpct()

static inline void generic_timer_reg_write(int reg, u32 val)
{
	switch (reg) {
	case GENERIC_TIMER_REG_CTRL:
		write_cnthp_ctl(val);
		break;
	case GENERIC_TIMER_REG_TVAL:
		write_cnthp_tval(val);
		break;
	default:
		vmm_panic("Trying to write invalid arch-hyp-timer register\n");
	}

	isb();
}

static inline u32 generic_timer_reg_read(int reg)
{
	u32 val;

	switch (reg) {
	case GENERIC_TIMER_REG_CTRL:
		val = read_cnthp_ctl();
		break;
	case GENERIC_TIMER_REG_FREQ:
		val = read_cntfrq();
		break;
	case GENERIC_TIMER_REG_TVAL:
		val = read_cnthp_tval();
		break;
	default:
		vmm_panic("Trying to read invalid arch-hyp-timer register\n");
	}

	return val;
}

#endif	/* __CPU_GENERIC_TIMER_H__ */
