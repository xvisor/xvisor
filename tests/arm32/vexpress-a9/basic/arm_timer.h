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
 * @file arm_timer.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARM Timer header
 *
 * Adapted from tests/arm32/pb-a8/basic/arm_timer.h
 */
#ifndef _ARM_TIMER_H__
#define _ARM_TIMER_H__

#include <arm_types.h>
#include <arm_irq.h>
#include <arm_plat.h>

#define TIMER_LOAD		0x00
#define TIMER_VALUE		0x04
#define TIMER_CTRL		0x08
#define TIMER_CTRL_ONESHOT	(1 << 0)
#define TIMER_CTRL_32BIT	(1 << 1)
#define TIMER_CTRL_DIV1		(0 << 2)
#define TIMER_CTRL_DIV16	(1 << 2)
#define TIMER_CTRL_DIV256	(2 << 2)
#define TIMER_CTRL_IE		(1 << 5)	/* Interrupt Enable (versatile only) */
#define TIMER_CTRL_PERIODIC	(1 << 6)
#define TIMER_CTRL_ENABLE	(1 << 7)

#define TIMER_INTCLR		0x0c
#define TIMER_RIS		0x10
#define TIMER_MIS		0x14
#define TIMER_BGLOAD		0x18

void arm_timer_enable(void);
void arm_timer_disable(void);
void arm_timer_clearirq(void);
u64 arm_timer_irqcount(void);
u64 arm_timer_irqdelay(void);
u64 arm_timer_timestamp(void);
int arm_timer_init(u32 usecs, u32 init_irqcount, u32 ensel);
void arm_timer_change_period(u32 usec);

/* Following taken from sp810.h */

/* sysctl registers offset */
#define SCCTRL				0x000
#define SCSYSSTAT			0x004
#define SCIMCTRL			0x008
#define SCIMSTAT			0x00C
#define SCXTALCTRL			0x010
#define SCPLLCTRL			0x014
#define SCPLLFCTRL			0x018
#define SCPERCTRL0			0x01C
#define SCPERCTRL1			0x020
#define SCPEREN				0x024
#define SCPERDIS			0x028
#define SCPERCLKEN			0x02C
#define SCPERSTAT			0x030
#define SCSYSID0			0xEE0
#define SCSYSID1			0xEE4
#define SCSYSID2			0xEE8
#define SCSYSID3			0xEEC
#define SCITCR				0xF00
#define SCITIR0				0xF04
#define SCITIR1				0xF08
#define SCITOR				0xF0C
#define SCCNTCTRL			0xF10
#define SCCNTDATA			0xF14
#define SCCNTSTEP			0xF18
#define SCPERIPHID0			0xFE0
#define SCPERIPHID1			0xFE4
#define SCPERIPHID2			0xFE8
#define SCPERIPHID3			0xFEC
#define SCPCELLID0			0xFF0
#define SCPCELLID1			0xFF4
#define SCPCELLID2			0xFF8
#define SCPCELLID3			0xFFC
	
#define SCCTRL_TIMEREN0SEL_REFCLK	(0 << 15)
#define SCCTRL_TIMEREN0SEL_TIMCLK	(1 << 15)

#define SCCTRL_TIMEREN1SEL_REFCLK	(0 << 17)
#define SCCTRL_TIMEREN1SEL_TIMCLK	(1 << 17)

#include <arm_io.h>

static inline void sysctl_soft_reset(void *base)
{
	/* switch to slow mode */
	arm_writel(0x2, base + SCCTRL);

	/* writing any value to SCSYSSTAT reg will reset system */
	arm_writel(0, base + SCSYSSTAT);
}

#endif
