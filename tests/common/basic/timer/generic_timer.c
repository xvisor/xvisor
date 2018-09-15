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
 * @brief ARMv7/ARMv8 Generic Timer driver source
 */

#include <arch_generic_timer.h>
#include <arch_math.h>
#include <basic_irq.h>
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

	ctrl = arch_read_cntv_ctl();
	ctrl |= GENERIC_TIMER_CTRL_ENABLE;
	ctrl &= ~GENERIC_TIMER_CTRL_IT_MASK;
	arch_write_cntv_ctl(ctrl);
}

void generic_timer_disable(void)
{
	unsigned long ctrl;

	ctrl = arch_read_cntv_ctl();
	ctrl &= ~GENERIC_TIMER_CTRL_ENABLE;
	arch_write_cntv_ctl(ctrl);
}

void generic_timer_change_period(u32 usec)
{
	timer_period_ticks = (arch_udiv64(timer_freq, 1000000) * usec);

	arch_write_cntv_tval(timer_period_ticks);
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
	return (arch_read_cntvct() * timer_mult) >> timer_shift;
}

int generic_timer_irqhndl(u32 irq_no, struct pt_regs *regs)
{
	unsigned long ctrl;
	u64 tstamp;

	ctrl = arch_read_cntv_ctl();
	if (ctrl & GENERIC_TIMER_CTRL_IT_STAT) {
		ctrl |= GENERIC_TIMER_CTRL_IT_MASK;
		arch_write_cntv_ctl(ctrl);
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

	ctrl = arch_read_cntv_ctl();
	ctrl |= GENERIC_TIMER_CTRL_ENABLE;
	ctrl &= ~GENERIC_TIMER_CTRL_IT_MASK;

	arch_write_cntv_tval(timer_period_ticks);
	arch_write_cntv_ctl(GENERIC_TIMER_CTRL_ENABLE);

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
                tmp = arch_udiv64(tmp, from);
                if ((tmp >> sftacc) == 0)
                        break;
        }
        *mult = tmp;
        *shift = sft;
}

int generic_timer_init(u32 usecs, u32 irq)
{
	timer_freq = arch_read_cntfrq();
	if (timer_freq == 0) {
		/* Assume 100 Mhz clock if cntfrq_el0 not programmed */
		timer_freq = 100000000;
	}

	calc_mult_shift(&timer_mult, &timer_shift, timer_freq, 1000000000, 1);

	timer_period_ticks = (arch_udiv64(timer_freq, 1000000) * usecs);

	basic_irq_register(irq, &generic_timer_irqhndl);

	arch_write_cntv_tval(timer_period_ticks);
	arch_write_cntv_ctl(GENERIC_TIMER_CTRL_IT_MASK);

	return 0;
}
