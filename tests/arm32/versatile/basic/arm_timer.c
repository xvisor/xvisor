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
 * @file arm_timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Timer source
 */

#include <arm_io.h>
#include <arm_irq.h>
#include <arm_math.h>
#include <arm_plat.h>
#include <arm_timer.h>

#include <arm_stdio.h>

#define TIMER_LOAD              0x00
#define TIMER_VALUE             0x04
#define TIMER_CTRL              0x08
#define TIMER_CTRL_ONESHOT      (1 << 0)
#define TIMER_CTRL_32BIT        (1 << 1)
#define TIMER_CTRL_DIV1         (0 << 2)
#define TIMER_CTRL_DIV16        (1 << 2)
#define TIMER_CTRL_DIV256       (2 << 2)
#define TIMER_CTRL_IE           (1 << 5)        /* Interrupt Enable (versatile only) */
#define TIMER_CTRL_PERIODIC     (1 << 6)
#define TIMER_CTRL_ENABLE       (1 << 7)

#define TIMER_INTCLR            0x0c
#define TIMER_RIS               0x10
#define TIMER_MIS               0x14
#define TIMER_BGLOAD            0x18

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

	ctrl = arm_readl((void *)(VERSATILE_TIMER0_1_BASE + TIMER_CTRL));
	ctrl |= TIMER_CTRL_ENABLE;
	arm_writel(ctrl, (void *)(VERSATILE_TIMER0_1_BASE + TIMER_CTRL));
}

void arm_timer_disable(void)
{
	u32 ctrl;

	ctrl = arm_readl((void *)(VERSATILE_TIMER0_1_BASE + TIMER_CTRL));
	ctrl &= ~TIMER_CTRL_ENABLE;
	arm_writel(ctrl, (void *)(VERSATILE_TIMER0_1_BASE + TIMER_CTRL));
}

void arm_timer_change_period(u32 usec)
{
        arm_writel(usec, (void *)(VERSATILE_TIMER0_1_BASE + TIMER_LOAD));
}

void arm_timer_clearirq(void)
{
	arm_writel(1, (void *)(VERSATILE_TIMER0_1_BASE + TIMER_INTCLR));
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
	timer_counter_now = ~arm_readl((void *)(VERSATILE_TIMER2_3_BASE + 0x20 + TIMER_VALUE));
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

	timer_irq_count = arm_readl((void *)VERSATILE_SYS_100HZ);
	timer_irq_tcount = 0;
	timer_irq_tstamp = 0;
	timer_irq_delay = 0;

	/* 
	 * set clock frequency: 
	 *      VERSATILE_REFCLK is 32KHz
	 *      VERSATILE_TIMCLK is 1MHz
	 */
	val = arm_readl((void *)VERSATILE_SCTL_BASE) | (VERSATILE_TIMCLK << ensel);
	arm_writel(val, (void *)VERSATILE_SCTL_BASE);

	/* Register interrupt handler */
	arm_irq_register(INT_TIMERINT0_1, &arm_timer_irqhndl);

	/* Setup Timer0 for generating irq */
	val = arm_readl((void *)(VERSATILE_TIMER0_1_BASE + TIMER_CTRL));
	val &= ~TIMER_CTRL_ENABLE;
	val |= (TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC | TIMER_CTRL_IE);
	arm_writel(val, (void *)(VERSATILE_TIMER0_1_BASE + TIMER_CTRL));
	arm_timer_change_period(usecs);

	/* Setup Timer3 for free running counter */
	arm_writel(0x0, (void *)(VERSATILE_TIMER2_3_BASE + 0x20 + TIMER_CTRL));
	arm_writel(0xFFFFFFFF, (void *)(VERSATILE_TIMER2_3_BASE + 0x20 + TIMER_LOAD));
	val = (TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC | TIMER_CTRL_ENABLE);
	arm_writel(val, (void *)(VERSATILE_TIMER2_3_BASE + 0x20 + TIMER_CTRL));

	return 0;
}
