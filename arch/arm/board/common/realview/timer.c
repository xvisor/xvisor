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
 * @file timer.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Realview Timer source
 */

#include <vmm_error.h>
#include <vmm_host_io.h>
#include <realview_config.h>
#include <realview/plat.h>
#include <realview/timer.h>

void realview_timer_enable(virtual_addr_t base)
{
	u32 ctrl;

	ctrl = vmm_readl((void *)(base + TIMER_CTRL));
	ctrl |= TIMER_CTRL_ENABLE;
	vmm_writel(ctrl, (void *)(base + TIMER_CTRL));
}

void realview_timer_disable(virtual_addr_t base)
{
	u32 ctrl;

	ctrl = vmm_readl((void *)(base + TIMER_CTRL));
	ctrl &= ~TIMER_CTRL_ENABLE;
	vmm_writel(ctrl, (void *)(base + TIMER_CTRL));
}

int realview_timer_event_stop(virtual_addr_t base)
{
	vmm_writel(0x0, (void *)(base + TIMER_CTRL));

	return VMM_OK;
}

void realview_timer_event_clearirq(virtual_addr_t base)
{
	vmm_writel(1, (void *)(base + TIMER_INTCLR));
}

bool realview_timer_event_checkirq(virtual_addr_t base)
{
	return vmm_readl((void *)(base + TIMER_MIS)) ? TRUE : FALSE;
}

int realview_timer_event_start(virtual_addr_t base, u64 nsecs)
{
	u32 ctrl, usecs;

	/* Expected microseconds is usecs = (nsecs / 1000).
	 * In integer arithmetic this can be approximated 
	 * as follows:
	 * usecs = (nsecs / 1000)
	 *       = (nsecs / 1024) * (1024 / 1000)
	 *       = (nsecs / 1024) + (nsecs / 1024) * (24 / 1000)
	 *       = (nsecs >> 10) + (nsecs >> 10) * (3 / 125)
	 *       ~ (nsecs >> 10) + (nsecs >> 10) * (3 / 128)
	 *       ~ (nsecs >> 10) + (((nsecs >> 10) * 3) >> 7)
	 */
	usecs = (nsecs >> 10) + (((nsecs >> 10) * 3) >> 7);
	if (!usecs) {
		usecs = 1;
	}

	/* Setup the registers */
	ctrl = vmm_readl((void *)(base + TIMER_CTRL));
	ctrl &= ~TIMER_CTRL_ENABLE;
	ctrl |= (TIMER_CTRL_32BIT | TIMER_CTRL_ONESHOT | TIMER_CTRL_IE);
	vmm_writel(ctrl, (void *)(base + TIMER_CTRL));
	vmm_writel(usecs, (void *)(base + TIMER_LOAD));
	vmm_writel(usecs, (void *)(base + TIMER_VALUE));
	ctrl |= TIMER_CTRL_ENABLE;
	vmm_writel(ctrl, (void *)(base + TIMER_CTRL));

	return VMM_OK;
}

u32 realview_timer_counter_value(virtual_addr_t base)
{
	return vmm_readl((void *)(base + TIMER_VALUE));
}

int realview_timer_counter_start(virtual_addr_t base)
{
	u32 ctrl;

	vmm_writel(0x0, (void *)(base + TIMER_CTRL));
	vmm_writel(0xFFFFFFFF, (void *)(base + TIMER_LOAD));
	vmm_writel(0xFFFFFFFF, (void *)(base + TIMER_VALUE));
	ctrl = (TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC);
	vmm_writel(ctrl, (void *)(base + TIMER_CTRL));
	
	return VMM_OK;
}

int __init realview_timer_init(virtual_addr_t sctl_base,
				       virtual_addr_t base,
				       u32 ensel,
				       u32 hirq,
				       vmm_host_irq_handler_t hirq_handler)
{
	int ret = VMM_OK;
	u32 val;

	/* 
	 * set clock frequency: 
	 *      REALVIEW_REFCLK is 32KHz
	 *      REALVIEW_TIMCLK is 1MHz
	 */
	val = vmm_readl((void *)sctl_base) | (REALVIEW_TIMCLK << ensel);
	vmm_writel(val, (void *)sctl_base);

	/*
	 * Initialise to a known state (all timers off)
	 */
	vmm_writel(0, (void *)(base + TIMER_CTRL));

	/* Register interrupt handler */
	if (hirq_handler) {
		ret = vmm_host_irq_register(hirq, hirq_handler, NULL);
	}

	return ret;
}
