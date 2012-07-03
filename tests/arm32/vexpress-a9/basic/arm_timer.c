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
 * @file arm_timer.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARM Timer source
 *
 * Adapted from tests/arm32/pb-a8/basic/arm_timer.c
 */

#include <arm_io.h>
#include <arm_irq.h>
#include <arm_math.h>
#include <arm_config.h>
#include <arm_plat.h>
#include <arm_timer.h>

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

static u64 timer_irq_count;
static u64 timer_irq_tcount;
static u64 timer_irq_tstamp;
static u64 timer_irq_delay;
static u64 timer_counter_mask;
static u64 timer_counter_shift;
static u64 timer_counter_mult;
static u64 timer_counter_last;
static u64 timer_time_stamp;

void arm_timer_enable(void)
{
	u32 ctrl;

	ctrl = arm_readl((void *)(V2M_TIMER0 + TIMER_CTRL));
	ctrl |= TIMER_CTRL_ENABLE;
	arm_writel(ctrl, (void *)(V2M_TIMER0 + TIMER_CTRL));
}

void arm_timer_disable(void)
{
	u32 ctrl;

	ctrl = arm_readl((void *)(V2M_TIMER0 + TIMER_CTRL));
	ctrl &= ~TIMER_CTRL_ENABLE;
	arm_writel(ctrl, (void *)(V2M_TIMER0 + TIMER_CTRL));
}

void arm_timer_change_period(u32 usec)
{
        arm_writel(usec, (void *)(V2M_TIMER0 + TIMER_LOAD));
}

void arm_timer_clearirq(void)
{
	arm_writel(1, (void *)(V2M_TIMER0 + TIMER_INTCLR));
}

u64 arm_timer_irqcount(void)
{
	return timer_irq_count;
}

u64 arm_timer_irqdelay(void)
{
	return timer_irq_delay;
}

u64 arm_timer_timestamp(void)
{
	u64 timer_counter_now, timer_counter_delta, offset;
	timer_counter_now = ~arm_readl((void *)(V2M_TIMER1 + TIMER_VALUE));
	timer_counter_delta = (timer_counter_now - timer_counter_last) & timer_counter_mask;
	timer_counter_last = timer_counter_now;
	offset = (timer_counter_delta * timer_counter_mult) >> timer_counter_shift;
	timer_time_stamp += offset;
	return timer_time_stamp;
}

int arm_timer_irqhndl(u32 irq_no, struct pt_regs * regs)
{
	u64 tstamp = arm_timer_timestamp();
	if (!timer_irq_tstamp) {
		timer_irq_tstamp = tstamp;
	}
	if (timer_irq_tcount == 256) {
		timer_irq_delay = (tstamp - timer_irq_tstamp) >> 8;
		timer_irq_tcount = 0;
		timer_irq_tstamp = tstamp;
	}
	timer_irq_tcount++;
	timer_irq_count++;
	arm_timer_clearirq();
	return 0;
}

int arm_timer_init(u32 usecs, u32 ensel)
{
	u32 val;

	timer_counter_mask = 0xFFFFFFFFULL;
	timer_counter_shift = 20;
	timer_counter_mult = ((u64)1000000) << timer_counter_shift;
	timer_counter_mult += (((u64)1000) >> 1);
	timer_counter_mult = arm_udiv64(timer_counter_mult, ((u64)1000));
	timer_counter_last = 0; 
	timer_time_stamp = 0;

	timer_irq_count = arm_readl((void *)(V2M_SYS_100HZ));
	timer_irq_tcount = 0;
	timer_irq_tstamp = 0;
	timer_irq_delay = 0;

	val = arm_readl((void *)V2M_SYSCTL) | SCCTRL_TIMEREN0SEL_TIMCLK;
	arm_writel(val, (void *)V2M_SYSCTL);

	/* Register interrupt handler */
	arm_irq_register(IRQ_V2M_TIMER0, &arm_timer_irqhndl);

	/* Setup Timer0 for generating irq */
	val = arm_readl((void *)(V2M_TIMER0 + TIMER_CTRL));
	val &= ~TIMER_CTRL_ENABLE;
	val |= (TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC | TIMER_CTRL_IE);
	arm_writel(val, (void *)(V2M_TIMER0 + TIMER_CTRL));
	arm_timer_change_period(usecs);

	/* Setup Timer3 for free running counter */
	arm_writel(0x0, (void *)(V2M_TIMER1 + TIMER_CTRL));
	arm_writel(0xFFFFFFFF, (void *)(V2M_TIMER1 + TIMER_LOAD));
	val = (TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC | TIMER_CTRL_ENABLE);
	arm_writel(val, (void *)(V2M_TIMER1 + TIMER_CTRL));

	return 0;
}
