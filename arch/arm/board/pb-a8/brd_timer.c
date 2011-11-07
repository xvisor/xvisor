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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief board specific progammable timer
 */

#include <vmm_cpu.h>
#include <vmm_board.h>
#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_host_aspace.h>
#include <vmm_math.h>
#include <pba8_board.h>
#include <realview/timer.h>

virtual_addr_t pba8_timer0_base;
virtual_addr_t pba8_timer1_base;
virtual_addr_t pba8_timer2_base;
virtual_addr_t pba8_timer3_base;

u64 vmm_cpu_clocksource_cycles(void)
{
	return ~realview_timer_counter_value(pba8_timer3_base);
}

u64 vmm_cpu_clocksource_mask(void)
{
	return 0xFFFFFFFF;
}

u32 vmm_cpu_clocksource_mult(void)
{
	u32 khz = 1000;
	u64 tmp = ((u64)1000000) << 20;
	tmp += khz >> 1;
	tmp = vmm_udiv64(tmp, khz);
	return (u32)tmp;
}

u32 vmm_cpu_clocksource_shift(void)
{
	return 20;
}

int vmm_cpu_clocksource_init(void)
{
	int rc;
	virtual_addr_t sctl_base;

	/* Map control registers */
	sctl_base = vmm_host_iomap(REALVIEW_SCTL_BASE, 0x1000);

	/* Map timer registers */
	pba8_timer2_base = vmm_host_iomap(REALVIEW_PBA8_TIMER2_3_BASE, 0x1000);
	pba8_timer3_base = pba8_timer2_base + 0x20;

	/* Initialize timers */
	rc = realview_timer_init(sctl_base, 
				 pba8_timer2_base,
				 REALVIEW_TIMER3_EnSel,
				 IRQ_PBA8_TIMER2_3,
				 NULL);
	if (rc) {
		return rc;
	}
	rc = realview_timer_init(sctl_base, 
				 pba8_timer3_base,
				 REALVIEW_TIMER4_EnSel,
				 IRQ_PBA8_TIMER2_3,
				 NULL);
	if (rc) {
		return rc;
	}

	/* Unmap control register */
	rc = vmm_host_iounmap(sctl_base, 0x1000);
	if (rc) {
		return rc;
	}

	/* Configure timer3 as free running source */
	rc = realview_timer_counter_setup(pba8_timer3_base);
	if (rc) {
		return rc;
	}
	realview_timer_enable(pba8_timer3_base);

	return VMM_OK;
}

int vmm_cpu_clockevent_shutdown(void)
{
	return realview_timer_event_shutdown(pba8_timer0_base);
}

int pba8_timer_handler(u32 irq_no, vmm_user_regs_t * regs, void *dev)
{
	realview_timer_event_clearirq(pba8_timer0_base);

	vmm_timer_clockevent_process(regs);

	return VMM_OK;
}

int vmm_cpu_clockevent_start(u64 tick_nsecs)
{
	return realview_timer_event_start(pba8_timer0_base, tick_nsecs);
}

int vmm_cpu_clockevent_setup(void)
{
	return realview_timer_event_setup(pba8_timer0_base);
}

int vmm_cpu_clockevent_init(void)
{
	int rc;
	virtual_addr_t sctl_base;

	/* Map control registers */
	sctl_base = vmm_host_iomap(REALVIEW_SCTL_BASE, 0x1000);

	/* Map timer registers */
	pba8_timer0_base = vmm_host_iomap(REALVIEW_PBA8_TIMER0_1_BASE, 0x1000);
	pba8_timer1_base = pba8_timer0_base + 0x20;

	/* Initialize timers */
	rc = realview_timer_init(sctl_base, 
				 pba8_timer0_base,
				 REALVIEW_TIMER1_EnSel,
				 IRQ_PBA8_TIMER0_1,
				 &pba8_timer_handler);
	if (rc) {
		return rc;
	}
	rc = realview_timer_init(sctl_base, 
				 pba8_timer1_base,
				 REALVIEW_TIMER2_EnSel,
				 IRQ_PBA8_TIMER0_1,
				 NULL);
	if (rc) {
		return rc;
	}

	/* Unmap control register */
	rc = vmm_host_iounmap(sctl_base, 0x1000);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

