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
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source code for handling cpu timer functions
 */

#include "vmm_types.h"
#include "vmm_error.h"
#include "vmm_host_irq.h"
#include "vmm_stdio.h"
#include "vmm_timer.h"
#include "cpu_interrupts.h"
#include "cpu_timer.h"
#include "cpu_asm_macros.h"

/** CPU frequency in MHz */
#define VMM_CPU_FREQ_MHZ				100

/** Delay of VMM ticks in microseconds */
#define VMM_CPU_TICK_DELAY_MICROSECS 			1000

/** Counter Jiffies */
#define VMM_COUNTER_JIFFIES				(VMM_CPU_FREQ_MHZ * VMM_CPU_TICK_DELAY_MICROSECS)


unsigned long long jiffies;

void vmm_cpu_timer_enable(void)
{
	u32 sr = read_c0_status();
	sr |= ((0x1UL << 7) << 8);
	write_c0_status(sr);

	u32 cause = read_c0_cause();
	cause &= ~(0x1UL << 27);
	write_c0_cause(cause);
	write_c0_compare(read_c0_count() + VMM_COUNTER_JIFFIES);
}

void vmm_cpu_timer_disable(void) 
{
}

s32 handle_internal_timer_interrupt(vmm_user_regs_t *uregs)
{
        jiffies++;
	vmm_timer_tick_process(uregs);
	write_c0_compare(read_c0_count() + VMM_COUNTER_JIFFIES);
        return 0;
}

int vmm_cpu_timer_setup(u64 tick_nsecs)
{
        jiffies = 0;
	return VMM_OK;
}

int vmm_cpu_timer_init(void)
{
	return VMM_OK;
}

