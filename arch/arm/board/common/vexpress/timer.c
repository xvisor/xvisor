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
 * @file timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Versatile Express Timer source
 */

#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vexpress_config.h>
#include <vexpress/plat.h>
#include <vexpress/timer.h>

void vexpress_timer_enable(virtual_addr_t base)
{
	u32 ctrl;

	ctrl = vmm_readl((void *)(base + TIMER_CTRL));
	ctrl |= TIMER_CTRL_ENABLE;
	vmm_writel(ctrl, (void *)(base + TIMER_CTRL));
}

void vexpress_timer_disable(virtual_addr_t base)
{
	u32 ctrl;

	ctrl = vmm_readl((void *)(base + TIMER_CTRL));
	ctrl &= ~TIMER_CTRL_ENABLE;
	vmm_writel(ctrl, (void *)(base + TIMER_CTRL));
}

int vexpress_timer_event_stop(virtual_addr_t base)
{
	vmm_writel(0x0, (void *)(base + TIMER_CTRL));

	return VMM_OK;
}

void vexpress_timer_event_clearirq(virtual_addr_t base)
{
	vmm_writel(1, (void *)(base + TIMER_INTCLR));
}

bool vexpress_timer_event_checkirq(virtual_addr_t base)
{
	return vmm_readl((void *)(base + TIMER_MIS)) ? TRUE : FALSE;
}

int vexpress_timer_event_start(virtual_addr_t base, u64 nsecs)
{
	u32 ctrl, usecs;

	/* Expected microseconds is usecs = (nsecs / 1000).
	 * In integer arithmetic this can be approximated 
	 * as follows:
	 * usecs = (nsecs / 1000)
	 *       = (nsecs / 1024) * (1024 / 1000)
	 *       = (nsecs / 1024) + (nsecs / 1024) * (24 / 1000)
	 *       = (nsecs >> 10) + (nsecs >> 10) * (3 / 125)
	 *       = (nsecs >> 10) + (nsecs >> 10) * (3 / 128) * (128 / 125)
	 *       = (nsecs >> 10) + (nsecs >> 10) * (3 / 128) + 
	 *                                (nsecs >> 10) * (3 / 128) * (3 / 125)
	 *       ~ (nsecs >> 10) + (nsecs >> 10) * (3 / 128) + 
	 *                                (nsecs >> 10) * (3 / 128) * (3 / 128)
	 *       ~ (nsecs >> 10) + (((nsecs >> 10) * 3) >> 7) + 
	 *                                          (((nsecs >> 10) * 9) >> 14)
	 */
	nsecs = nsecs >> 10;
	usecs = nsecs + ((nsecs * 3) >> 7) + ((nsecs * 9) >> 14);
	if (!usecs) {
		usecs = 1;
	}

	/* Setup the registers */
	ctrl = vmm_readl((void *)(base + TIMER_CTRL));
	/* Stop the timer if started */
	if (ctrl & TIMER_CTRL_ENABLE) {
		ctrl &= ~TIMER_CTRL_ENABLE;
		ctrl |= TIMER_CTRL_32BIT; /* Ensure 32-bit mode */
		vmm_writel(ctrl, (void *)(base + TIMER_CTRL));
	}
	/* write the new timer value */
	vmm_writel(usecs, (void *)(base + TIMER_LOAD));
	/* start the timer in One Shot mode */
	ctrl |=	(TIMER_CTRL_32BIT | 
		 TIMER_CTRL_ONESHOT | 
		 TIMER_CTRL_IE |
		 TIMER_CTRL_ENABLE);
	vmm_writel(ctrl, (void *)(base + TIMER_CTRL));

	return VMM_OK;
}

u32 vexpress_timer_counter_value(virtual_addr_t base)
{
	return vmm_readl((void *)(base + TIMER_VALUE));
}

int vexpress_timer_counter_start(virtual_addr_t base)
{
	u32 ctrl;

	vmm_writel(0x0, (void *)(base + TIMER_CTRL));
	vmm_writel(0xFFFFFFFF, (void *)(base + TIMER_LOAD));
	ctrl = (TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC);
	vmm_writel(ctrl, (void *)(base + TIMER_CTRL));

	return VMM_OK;
}

int __init vexpress_timer_init(virtual_addr_t sctl_base,
			       virtual_addr_t base,
			       u32 ensel,
			       u32 hirq, vmm_host_irq_handler_t hirq_handler)
{
	int ret = VMM_OK;
	u32 val;

	/* 
	 * set clock frequency: 
	 *      VEXPRESS_REFCLK is 32KHz
	 *      VEXPRESS_TIMCLK is 1MHz
	 */
	val = vmm_readl((void *)sctl_base) | (VEXPRESS_TIMCLK << ensel);
	vmm_writel(val, (void *)sctl_base);

	/*
	 * Initialise to a known state (all timers off)
	 */
	vmm_writel(0, (void *)(base + TIMER_CTRL));

	/* Register interrupt handler */
	if (hirq_handler) {
		ret = vmm_host_irq_register(hirq, hirq_handler, NULL);
		if (ret) {
			return ret;
		}
		ret = vmm_host_irq_enable(hirq);
		if (ret) {
			return ret;
		}
	}

	return ret;
}
