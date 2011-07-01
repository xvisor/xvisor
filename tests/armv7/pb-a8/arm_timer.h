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
 * @file arm_timer.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Timer header
 */
#ifndef _ARM_TIMER_H__
#define _ARM_TIMER_H__

#include <arm_types.h>
#include <arm_irq.h>
#include <arm_plat.h>

#define TIMER_LOAD	0x00
#define TIMER_VALUE	0x04
#define TIMER_CTRL	0x08
#define TIMER_CTRL_ONESHOT	(1 << 0)
#define TIMER_CTRL_32BIT	(1 << 1)
#define TIMER_CTRL_DIV1		(0 << 2)
#define TIMER_CTRL_DIV16	(1 << 2)
#define TIMER_CTRL_DIV256	(2 << 2)
#define TIMER_CTRL_IE		(1 << 5)	/* Interrupt Enable (versatile only) */
#define TIMER_CTRL_PERIODIC	(1 << 6)
#define TIMER_CTRL_ENABLE	(1 << 7)

#define TIMER_INTCLR	0x0c
#define TIMER_RIS	0x10
#define TIMER_MIS	0x14
#define TIMER_BGLOAD	0x18

void arm_timer_enable(virtual_addr_t base);
void arm_timer_disable(virtual_addr_t base);
void arm_timer_clearirq(virtual_addr_t base);
int arm_timer_setup(virtual_addr_t base,
			 u32 usecs,
			 u32 hirq, arm_irq_handler_t hirq_handler);
int arm_timer_init(virtual_addr_t sctl_base,
			virtual_addr_t base, u32 ensel);

#endif
