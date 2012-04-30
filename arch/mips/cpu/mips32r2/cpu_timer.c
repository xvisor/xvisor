/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file cpu_timer.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source code for handling cpu timer functions
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_host_irq.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_clocksource.h>
#include <cpu_interrupts.h>
#include <cpu_timer.h>
#include <cpu_asm_macros.h>

/** CPU frequency in MHz */
#define VMM_CPU_FREQ_MHZ			100

#define VMM_CLOCK_SOURCE_RATE			VMM_CPU_FREQ_MHZ

/** Delay of VMM ticks in microseconds */
#define VMM_CPU_TICK_DELAY_MICROSECS 		1000

/** Counter Jiffies */
#define VMM_COUNTER_JIFFIES			(VMM_CPU_FREQ_MHZ \
						 * VMM_CPU_TICK_DELAY_MICROSECS)

#define MHZ2HZ(_x_)				(u64)(_x_ * 1000 * 1000)
#define SEC2NSEC(__x)				(__x * 1000 * 1000 * 1000)
#define NS2COUNT(_x)				((MHZ2HZ(VMM_CPU_FREQ_MHZ) \
						  * _x) / SEC2NSEC(1)) 

unsigned long long jiffies;

void arch_timer_enable(void)
{
	u32 sr = read_c0_status();
	sr |= ((0x1UL << 7) << 8);
	write_c0_status(sr);

	write_c0_compare(read_c0_count() + VMM_COUNTER_JIFFIES);
}

s32 handle_internal_timer_interrupt(arch_regs_t *uregs)
{
	jiffies++;
	vmm_timer_clockevent_process(uregs);
	write_c0_compare(read_c0_count() + VMM_COUNTER_JIFFIES);
	return 0;
}

u32 ns2count(u64 ticks_nsecs)
{
	u32 req_count = ((u64)(MHZ2HZ(VMM_CPU_FREQ_MHZ) * ticks_nsecs))/SEC2NSEC(1);

	return req_count;
}

int arch_clockevent_start(u64 ticks_nsecs)
{
	/* Enable the timer interrupts. */
	u32 sr = read_c0_status();
	sr |= ((0x1UL << 7) << 8);
	write_c0_status(sr);

	u32 next_ticks = ns2count(ticks_nsecs);
	write_c0_compare(read_c0_count() + next_ticks);

	return VMM_OK;
}

int arch_clockevent_setup(void)
{
	return VMM_OK;
}

int arch_clockevent_shutdown(void)
{
	/* Disable the timer interrupts. */
	u32 sr = read_c0_status();
	sr &= ~((0x1UL << 7) << 8);
	write_c0_status(sr);

	return VMM_OK;
}

int arch_clockevent_stop(void)
{
	return 0;
}

int arch_clockevent_expire(void)
{
	return 0;
}

int arch_clockevent_init(void)
{
	/* Disable the timer interrupts. */
	u32 sr = read_c0_status();
	sr &= ~((0x1UL << 7) << 8);
	write_c0_status(sr);

	jiffies = 0;

	return VMM_OK;
}

static u64 mips_clocksource_read(struct vmm_clocksource *cs)
{
	return read_c0_count();
}

static struct vmm_clocksource mips_cs =  
{
	.name = "mips_clksrc",
	.rating = 300,
	.mask = 0xFFFFFFFF,
	.shift = 20,
	.read = &mips_clocksource_read
};

int arch_clocksource_init(void)
{
	int rc;

	/* Register clocksource */
	mips_cs.mult = vmm_clocksource_khz2mult(1000, 20);
	if ((rc = vmm_clocksource_register(&mips_cs))) {
		return rc;
	}

	/* Enable the monotonic count. */
	u32 cause = read_c0_cause();
	cause &= ~(0x1UL << 27);
	write_c0_cause(cause);

	/* Initialize the counter to 0. */
	write_c0_count(0);

	return VMM_OK;
}
