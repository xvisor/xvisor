/**
 * Copyright (c) 2011 Sukanto Ghosh
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source code for Beagle board specific general purpose timers
 *
 */

#include <vmm_cpu.h>
#include <vmm_board.h>
#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_host_aspace.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_math.h>
#include <omap3/intc.h>
#include <omap3/gpt.h>
#include <omap3/s32k-timer.h>
#include <omap3/prcm.h>

#define OMAP3_SYS_TIMER_BASE	(OMAP3_GPT1_BASE)
#define OMAP3_SYS_TIMER_IRQ	(OMAP3_MPU_INTC_GPT1_IRQ)
#define OMAP3_CLKSRC_FREQ_HZ	(OMAP3_S32K_FREQ_HZ)

u64 vmm_cpu_clocksource_cycles(void)
{
	return ((u64)omap3_s32k_get_counter());
}

u64 vmm_cpu_clocksource_mask(void)
{
	return 0xFFFFFFFF;
}

u32 vmm_cpu_clocksource_mult(void)
{
	return vmm_timer_clocksource_hz2mult(OMAP3_CLKSRC_FREQ_HZ, 15);
}

u32 vmm_cpu_clocksource_shift(void)
{
	return 15;
}

int vmm_cpu_clocksource_init(void)
{
	return omap3_s32k_init();
}

int vmm_cpu_clockevent_shutdown(void)
{
	omap3_gpt_stop();
	return VMM_OK;
}

int vmm_cpu_timer_irq_handler(u32 irq_no, vmm_user_regs_t * regs, void *dev)
{
	omap3_gpt_ack_irq();
	omap3_gpt_stop();
	vmm_timer_clockevent_process(regs);

	return VMM_OK;
}

int vmm_cpu_clockevent_start(u64 tick_nsecs)
{
	u32 tick_usecs;

	/* Get granuality in microseconds */
	tick_usecs = vmm_udiv64(tick_nsecs, 1000);
	if (!tick_usecs) {
		tick_usecs = 1;
	}

	omap3_gpt_load_start(tick_usecs);
	return VMM_OK;
}

int vmm_cpu_clockevent_setup(void)
{
	omap3_gpt_disable();
	omap3_gpt_oneshot();

	vmm_host_irq_enable(OMAP3_SYS_TIMER_IRQ);

	return VMM_OK;
}

int vmm_cpu_clockevent_init(void)
{
	return omap3_gpt_init(OMAP3_SYS_TIMER_BASE,
			OMAP3_WKUP_CM_BASE,
			OMAP3_GLOBAL_REG_PRM_BASE,
			OMAP3_SYS_TIMER_IRQ,
			&vmm_cpu_timer_irq_handler);
}

