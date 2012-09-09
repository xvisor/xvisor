/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file vmm_delay.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for soft delay susbsystem
 */

#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_delay.h>
#include <mathlib.h>
#include <arch_delay.h>
#include <arch_cpu.h>

static u32 loops_per_msec;
static u32 loops_per_usec;

void vmm_udelay(u32 usecs)
{
	irq_flags_t flags;

	flags = arch_cpu_irq_save();

	arch_delay_loop(usecs * loops_per_usec);

	arch_cpu_irq_restore(flags);
}

void vmm_mdelay(u32 msecs)
{
	irq_flags_t flags;

	flags = arch_cpu_irq_save();

	arch_delay_loop(msecs * loops_per_msec);

	arch_cpu_irq_restore(flags);
}

u32 vmm_delay_estimate_cpu_mhz(void)
{
	return arch_delay_loop_cycles(loops_per_usec);
}

u32 vmm_delay_estimate_cpu_khz(void)
{
	return arch_delay_loop_cycles(loops_per_msec);
}

void vmm_delay_recaliberate(void)
{
	u64 nsecs, tstamp;
	irq_flags_t flags;

	flags = arch_cpu_irq_save();

	tstamp = vmm_timer_timestamp();

	arch_delay_loop(1000000);

	nsecs = vmm_timer_timestamp() - tstamp;

	loops_per_usec = udiv64(1000ULL * 1000000ULL, nsecs);
	loops_per_msec = udiv64(1000000ULL * 1000000ULL, nsecs);

	arch_cpu_irq_restore(flags);
}

int vmm_delay_init(void)
{
	loops_per_msec = 0;
	loops_per_usec = 0;

	vmm_delay_recaliberate();

	return VMM_OK;
}

