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
 * @file brd_timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief board specific progammable timer
 */

#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <arch_cpu.h>
#include <arch_board.h>
#include <realview/plat.h>
#include <pba8_board.h>
#include <sp804_timer.h>

static virtual_addr_t pba8_timer0_base;
static virtual_addr_t pba8_timer1_base;

u64 arch_cpu_clocksource_cycles(void)
{
	return ~sp804_timer_counter_value(pba8_timer1_base);
}

u64 arch_cpu_clocksource_mask(void)
{
	return 0xFFFFFFFF;
}

u32 arch_cpu_clocksource_mult(void)
{
	return vmm_timer_clocksource_khz2mult(1000, 20);
}

u32 arch_cpu_clocksource_shift(void)
{
	return 20;
}

int __init arch_cpu_clocksource_init(void)
{
	int rc;
	u32 val;
	virtual_addr_t sctl_base;

	/* Map control registers */
	sctl_base = vmm_host_iomap(REALVIEW_SCTL_BASE, 0x1000);

	/* 
	 * set clock frequency: 
	 *      REALVIEW_REFCLK is 32KHz
	 *      REALVIEW_TIMCLK is 1MHz
	 */
	val = vmm_readl((void *)sctl_base) | 
			(REALVIEW_TIMCLK << REALVIEW_TIMER2_EnSel);
	vmm_writel(val, (void *)sctl_base);

	/* Unmap control register */
	rc = vmm_host_iounmap(sctl_base, 0x1000);
	if (rc) {
		return rc;
	}

	/* Map timer registers */
	pba8_timer1_base = vmm_host_iomap(REALVIEW_PBA8_TIMER0_1_BASE, 0x1000);
	pba8_timer1_base += 0x20;

	/* Initialize timers */
	rc = sp804_timer_init(pba8_timer1_base,
			      IRQ_PBA8_TIMER0_1,
			      NULL);
	if (rc) {
		return rc;
	}

	/* Configure timer1 as free running source */
	rc = sp804_timer_counter_start(pba8_timer1_base);
	if (rc) {
		return rc;
	}
	sp804_timer_enable(pba8_timer1_base);

	return VMM_OK;
}

int arch_cpu_clockevent_stop(void)
{
	return sp804_timer_event_stop(pba8_timer0_base);
}

static int pba8_timer0_handler(u32 irq_no, arch_regs_t * regs, void *dev)
{
	sp804_timer_event_clearirq(pba8_timer0_base);

	vmm_timer_clockevent_process(regs);

	return VMM_OK;
}

int arch_cpu_clockevent_expire(void)
{
	int rc;

	rc = sp804_timer_event_start(pba8_timer0_base, 0);

	if (!rc) {
		/* FIXME: The polling loop below is fine with emulators but,
		 * for real hardware we might require some soft delay to
		 * avoid bus contention.
		 */
		while (!sp804_timer_event_checkirq(pba8_timer0_base));
	}

	return rc;
}

int arch_cpu_clockevent_start(u64 tick_nsecs)
{
	return sp804_timer_event_start(pba8_timer0_base, tick_nsecs);
}

int __init arch_cpu_clockevent_init(void)
{
	int rc;
	u32 val;
	virtual_addr_t sctl_base;

	/* Map control registers */
	sctl_base = vmm_host_iomap(REALVIEW_SCTL_BASE, 0x1000);

	/* 
	 * set clock frequency: 
	 *      REALVIEW_REFCLK is 32KHz
	 *      REALVIEW_TIMCLK is 1MHz
	 */
	val = vmm_readl((void *)sctl_base) | 
			(REALVIEW_TIMCLK << REALVIEW_TIMER1_EnSel);
	vmm_writel(val, (void *)sctl_base);

	/* Unmap control register */
	rc = vmm_host_iounmap(sctl_base, 0x1000);
	if (rc) {
		return rc;
	}

	/* Map timer registers */
	pba8_timer0_base = vmm_host_iomap(REALVIEW_PBA8_TIMER0_1_BASE, 0x1000);

	/* Initialize timers */
	rc = sp804_timer_init(pba8_timer0_base,
			      IRQ_PBA8_TIMER0_1,
			      pba8_timer0_handler);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

