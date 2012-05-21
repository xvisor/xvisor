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
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <arch_board.h>
#include <realview_plat.h>
#include <pba8_board.h>
#include <sp804_timer.h>

static virtual_addr_t pba8_timer0_base;
static virtual_addr_t pba8_timer1_base;

int __init arch_clocksource_init(void)
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

	/* Map timer1 registers */
	pba8_timer1_base = vmm_host_iomap(REALVIEW_PBA8_TIMER0_1_BASE, 0x1000);
	pba8_timer1_base += 0x20;

	/* Initialize timer1 as clocksource */
	rc = sp804_clocksource_init(pba8_timer1_base, 
				    "sp804_timer1", 300, 1000000, 20);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int __init arch_clockchip_init(void)
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

	/* Map timer0 registers */
	pba8_timer0_base = vmm_host_iomap(REALVIEW_PBA8_TIMER0_1_BASE, 0x1000);

	/* Initialize timer0 as clockchip */
	rc = sp804_clockchip_init(pba8_timer0_base, IRQ_PBA8_TIMER0_1, 
				  "sp804_timer0", 300, 1000000, 0);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

