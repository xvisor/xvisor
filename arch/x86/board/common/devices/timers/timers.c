/**
 * Copyright (c) 2012 Himanshu Chauhan.
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
 * @file hpet.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief HPET access and configuration.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_clockchip.h>
#include <vmm_clocksource.h>
#include <vmm_host_irq.h>
#include <vmm_wallclock.h>
#include <acpi.h>
#include <timers/timer.h>

#if defined(CONFIG_LAPIC_TIMER)
#include <cpu_apic.h>
#endif

#if defined(CONFIG_HPET)
#include <timers/hpet.h>
#endif

int __init arch_clocksource_init(void)
{
	return hpet_clocksource_init(DEFAULT_HPET_SYS_TIMER,
				     "hpet_clksrc");
}

int __cpuinit arch_clockchip_init(void)
{
	return hpet_clockchip_init(DEFAULT_HPET_SYS_TIMER, 
				"hpet_clkchip", 0);
}

int __init timer_init(void)
{
    int rv = VMM_OK;

    rv = hpet_init();

    return rv;
}
