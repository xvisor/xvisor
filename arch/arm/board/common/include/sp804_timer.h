/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file sp804_timer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief SP804 Dual-Mode Timer Interface
 */
#ifndef __SP804_TIMER_H__
#define __SP804_TIMER_H__

#include <vmm_types.h>

#define TIMER_LOAD		0x00
#define TIMER_VALUE		0x04
#define TIMER_CTRL		0x08
#define TIMER_CTRL_ONESHOT	(1 << 0)
#define TIMER_CTRL_32BIT	(1 << 1)
#define TIMER_CTRL_DIV1		(0 << 2)
#define TIMER_CTRL_DIV16	(1 << 2)
#define TIMER_CTRL_DIV256	(2 << 2)
#define TIMER_CTRL_IE		(1 << 5)
#define TIMER_CTRL_PERIODIC	(1 << 6)
#define TIMER_CTRL_ENABLE	(1 << 7)
#define TIMER_INTCLR		0x0c
#define TIMER_RIS		0x10
#define TIMER_MIS		0x14
#define TIMER_BGLOAD		0x18

int sp804_clocksource_init(virtual_addr_t base, 
			   const char *name, 
			   int rating, 
			   u32 freq_hz,
			   u32 shift);

int sp804_clockchip_init(virtual_addr_t base, 
			 u32 hirq,
			 const char *name, 
			 int rating, 
			 u32 freq_hz,
			 u32 target_cpu);

#endif /* __SP804_TIMER_H__ */
