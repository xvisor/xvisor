/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file riscv_timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RISC-V Timer Source
 */

#include <arch_asm.h>
#include <arch_math.h>
#include <arch_sbi.h>
#include <basic_irq.h>
#include <timer/riscv_timer.h>

static u64 timer_irq_count;
static u64 timer_irq_tcount;
static u64 timer_irq_delay;
static u64 timer_irq_tstamp;
static u64 timer_freq;
static u64 timer_period_ticks;
static u64 timer_mult;
static u32 timer_shift;

static u64 riscv_rdtime(void)
{
#if __riscv_xlen == 32
	u32 lo, hi, tmp;

	__asm__ __volatile__ (
		"1:\n"
		"rdtimeh %0\n"
		"rdtime %1\n"
		"rdtimeh %2\n"
		"bne %0, %2, 1b"
		: "=&r" (hi), "=&r" (lo), "=&r" (tmp));

	return ((u64)hi << 32) | lo;
#else
	u64 n;

	__asm__ __volatile__ (
		"rdtime %0"
		: "=r" (n));

	return n;
#endif
}

static void riscv_timer_config(u64 evt)
{
	csr_set(sie, SIE_STIE);
	sbi_set_timer(riscv_rdtime() + evt);
}

void riscv_timer_enable(void)
{
	riscv_timer_config(timer_period_ticks);
}

void riscv_timer_disable(void)
{
	/*
	 * There are no direct SBI calls to clear pending timer interrupt bit.
	 * Disable timer interrupt to ignore pending interrupt until next
	 * interrupt.
	 */
	csr_clear(sie, SIE_STIE);
}

u64 riscv_timer_irqcount(void)
{
	return timer_irq_count;
}

u64 riscv_timer_irqdelay(void)
{
	return timer_irq_delay;
}

u64 riscv_timer_timestamp(void)
{
	return (riscv_rdtime() * timer_mult) >> timer_shift;
}

int riscv_timer_irqhndl(u32 irq_no, struct pt_regs *regs)
{
	u64 tstamp;

	riscv_timer_disable();

	timer_irq_count++;
	timer_irq_tcount++;

	tstamp = riscv_timer_timestamp();
	if (!timer_irq_tstamp) {
		timer_irq_tstamp = tstamp;
	}
	if (timer_irq_tcount == 128) {
		timer_irq_delay = (tstamp - timer_irq_tstamp) >> 7;
		timer_irq_tcount = 0;
		timer_irq_tstamp = tstamp;
	}

	riscv_timer_config(timer_period_ticks);

	return 0;
}

void riscv_timer_change_period(u32 usecs)
{
	timer_period_ticks = (arch_udiv64(timer_freq, 1000000) * usecs);
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

int riscv_timer_init(u32 usecs, u64 freq)
{
	timer_freq = freq;
	if (timer_freq == 0) {
		/* Assume 10 Mhz clock */
		timer_freq = 10000000;
	}

	calc_mult_shift(&timer_mult, &timer_shift, timer_freq, 1000000000, 1);

	timer_period_ticks = (arch_udiv64(timer_freq, 1000000) * usecs);

	basic_irq_register(IRQ_S_TIMER, &riscv_timer_irqhndl);

	return 0;
}
