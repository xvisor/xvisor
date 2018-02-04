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
#include <tsc.h>

#if defined(CONFIG_LAPIC_TIMER)
#include <cpu_apic.h>
#endif

#if defined(CONFIG_HPET)
#include <timers/hpet.h>
#endif

struct x86_system_timer_ops sys_timer_ops = { NULL };

int __init arch_clocksource_init(void)
{
	int rc;

	BUG_ON(!sys_timer_ops.sys_cs_init);

	rc = sys_timer_ops.sys_cs_init();

	return rc;
}

int __cpuinit arch_clockchip_init(void)
{
	int rc;

	BUG_ON(!sys_timer_ops.sys_cc_init);

	rc = sys_timer_ops.sys_cc_init();

	return rc;
}

void __init
x86_register_system_timer_ops(struct x86_system_timer_ops *ops)
{
	sys_timer_ops.sys_cc_init = ops->sys_cc_init;
	sys_timer_ops.sys_cs_init = ops->sys_cs_init;
}

int __init x86_timer_init(void)
{
	int rv = VMM_EFAIL;

#if defined(CONFIG_HPET)
	if ((rv = hpet_init()) == VMM_OK) {
		vmm_printf("HPET Init Succeeded!\n");
		goto _init_done;
	}
#endif

#if defined(CONFIG_LAPIC_TIMER)
	if ((rv = lapic_timer_init()) == VMM_OK) {
		vmm_printf("LAPIC timer init succeeded!\n");
		goto _init_done;
	}
#endif

 _init_done:
	return rv;
}
