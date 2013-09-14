/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file generic_timer.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARMv8 Generic Timer driver source
 *
 */

#include <arm_irq.h>
#include <arm_math.h>
#include <arm_stdio.h>
#include <arm_inline_asm.h>
#include <timer/generic_timer.h>

#define GENERIC_TIMER_CTRL_ENABLE		(1 << 0)
#define GENERIC_TIMER_CTRL_IT_MASK		(1 << 1)
#define GENERIC_TIMER_CTRL_IT_STAT		(1 << 2)

static u64 timer_irq_count;
static u64 timer_irq_tcount;
static u64 timer_irq_delay;
static u64 timer_irq_tstamp;
static u64 timer_freq;
static u64 timer_period_ticks;
static u64 timer_mult;
static u32 timer_shift;

void generic_timer_enable(void)
{
	unsigned long ctrl;

	ctrl = mrs(cntv_ctl_el0);
	ctrl |= GENERIC_TIMER_CTRL_ENABLE;
	ctrl &= ~GENERIC_TIMER_CTRL_IT_MASK;
	msr(cntv_ctl_el0, ctrl);
}

void generic_timer_disable(void)
{
	unsigned long ctrl;

	ctrl = mrs(cntv_ctl_el0);
	ctrl &= ~GENERIC_TIMER_CTRL_ENABLE;
	msr(cntv_ctl_el0, ctrl);
}

void generic_timer_change_period(u32 usec)
{
	timer_period_ticks = ((timer_freq / 1000000) * usec);

	msr(cntv_tval_el0, timer_period_ticks);
}

u64 generic_timer_irqcount(void)
{
	return timer_irq_count;
}

u64 generic_timer_irqdelay(void)
{
	return timer_irq_delay;
}

u64 generic_timer_timestamp(void)
{
	return (mrs(cntvct_el0) * timer_mult) >> timer_shift;
}

int generic_timer_irqhndl(u32 irq_no, struct pt_regs *regs)
{
	unsigned long ctrl;
	u64 tstamp;

	ctrl = mrs(cntv_ctl_el0);
	if (ctrl & GENERIC_TIMER_CTRL_IT_STAT) {
		ctrl |= GENERIC_TIMER_CTRL_IT_MASK;
		msr(cntv_ctl_el0, ctrl);
	}

	timer_irq_count++;
	timer_irq_tcount++;

	tstamp = generic_timer_timestamp();
	if (!timer_irq_tstamp) {
		timer_irq_tstamp = tstamp;
	}
	if (timer_irq_tcount == 128) {
		timer_irq_delay = (tstamp - timer_irq_tstamp) >> 7;
		timer_irq_tcount = 0;
		timer_irq_tstamp = tstamp;
	}

	ctrl = mrs(cntv_ctl_el0);
	ctrl |= GENERIC_TIMER_CTRL_ENABLE;
	ctrl &= ~GENERIC_TIMER_CTRL_IT_MASK;

	msr(cntv_tval_el0, timer_period_ticks);
	msr(cntv_ctl_el0, GENERIC_TIMER_CTRL_ENABLE);

	return 0;
}

static void calc_mult_shift(u64 *mult, u32 *shift,
		            u32 from, u32 to, u32 maxsec)
{
        u64 tmp;
        u32 sft, sftacc= 32;

	/* Calculate the shift factor which is limiting 
	 * the conversion range:
	 */
        tmp = ((u64)maxsec * from) >> 32;
        while (tmp) {
                tmp >>=1;
                sftacc--;
        }

        /* Find the conversion shift/mult pair which has the best
	 * accuracy and fits the maxsec conversion range:
	 */
        for (sft = 32; sft > 0; sft--) {
                tmp = (u64) to << sft;
                tmp += from / 2;
                tmp /= from;
                if ((tmp >> sftacc) == 0)
                        break;
        }
        *mult = tmp;
        *shift = sft;
}

int generic_timer_init(u32 usecs, u32 irq)
{
	timer_freq = mrs(cntfrq_el0);
	if (timer_freq == 0) {
		/* Assume 100 Mhz clock if cntfrq_el0 not programmed */
		timer_freq = 100000000;
	}

	calc_mult_shift(&timer_mult, &timer_shift, timer_freq, 1000000000, 1);

	timer_period_ticks = ((timer_freq / 1000000) * usecs);

	arm_irq_register(irq, &generic_timer_irqhndl);

	msr(cntv_tval_el0, timer_period_ticks);
	msr(cntv_ctl_el0, GENERIC_TIMER_CTRL_IT_MASK);

	return 0;
}
