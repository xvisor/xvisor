/**
 * Copyright (c) 2010-20 Himanshu Chauhan.
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
#include <cpu_interrupts.h>
#include <cpu_timer.h>

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

void arch_cpu_timer_enable(void)
{
}

u64 arch_cpu_clocksource_cycles(void)
{
        return 0;
}

u32 ns2count(u64 ticks_nsecs)
{
        return 0;
}

int arch_cpu_clockevent_start(u64 ticks_nsecs)
{
	return VMM_OK;
}

int arch_cpu_clockevent_setup(void)
{
	return VMM_OK;
}

int arch_cpu_clockevent_shutdown(void)
{
	return VMM_OK;
}

u64 arch_cpu_clocksource_mask(void)
{
	return 0xFFFFFFFF;
}

u32 arch_cpu_clocksource_mult(void)
{
        return 0;
}

u32 arch_cpu_clocksource_shift(void)
{
        return 0;
}

int arch_cpu_clockevent_stop(void)
{
	return 0;
}

int arch_cpu_clockevent_expire(void)
{
	return 0;
}

int arch_cpu_clockevent_init(void)
{
	return VMM_OK;
}

int arch_cpu_clocksource_init(void)
{
	return VMM_OK;
}
