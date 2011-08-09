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
#include <pba8_board.h>
#include <realview/realview_timer.h>

virtual_addr_t pba8_cpu_timer_base;

void vmm_cpu_timer_enable(void)
{
	realview_timer_enable(pba8_cpu_timer_base);
}

void vmm_cpu_timer_disable(void)
{
	realview_timer_disable(pba8_cpu_timer_base);
}

int pba8_timer_handler(u32 irq_no, vmm_user_regs_t * regs)
{
	vmm_timer_tick_process(regs, 1);

	realview_timer_clearirq(pba8_cpu_timer_base);

	return VMM_OK;
}

int vmm_cpu_timer_setup(u32 tick_nsecs)
{
	pba8_cpu_timer_base = vmm_host_iomap(REALVIEW_PBA8_TIMER0_1_BASE,
					     0x1000);
	return realview_timer_setup(pba8_cpu_timer_base,
				    tick_nsecs,
				    IRQ_PBA8_TIMER0_1, &pba8_timer_handler);
}
