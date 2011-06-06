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
 * @file realview_timer.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Realview Timer source
 */

#include <vmm_error.h>
#include <vmm_host_io.h>
#include <realview_config.h>
#include <realview/realview_plat.h>
#include <realview/realview_timer.h>

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

void realview_timer_clearirq(virtual_addr_t base)
{
	vmm_writel(1, (void *)(base + TIMER_INTCLR));
}

int realview_timer_setup(virtual_addr_t base,
			 u32 usecs,
			 u32 hirq, vmm_host_irq_handler_t hirq_handler)
{
	int ret;

	/* Register interrupt handler */
	ret = vmm_host_irq_register(hirq, hirq_handler);
	if (ret) {
		return ret;
	}

	vmm_writel(0, (void *)(base + TIMER_CTRL));
	vmm_writel(usecs, (void *)(base + TIMER_LOAD));
	vmm_writel(usecs, (void *)(base + TIMER_VALUE));
	vmm_writel(TIMER_CTRL_32BIT | TIMER_CTRL_PERIODIC | TIMER_CTRL_IE,
		   (void *)(base + TIMER_CTRL));

	return VMM_OK;
}

int realview_timer_init(virtual_addr_t sctl_base,
			virtual_addr_t base, u32 ensel)
{
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

	return VMM_OK;
}
