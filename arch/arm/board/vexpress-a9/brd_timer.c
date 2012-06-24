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
 * @file brd_timer.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief board specific progammable timer
 */

#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_host_aspace.h>
#include <ca9x4_board.h>
#include <vexpress_plat.h>
#include <sp810.h>
#include <sp804_timer.h>
#include <smp_twd.h>

static virtual_addr_t ca9x4_twd_base;
static virtual_addr_t ca9x4_sys_24mhz;
static virtual_addr_t ca9x4_timer1_base;

int __init arch_clocksource_init(void)
{
	int rc;
	u32 val;
	virtual_addr_t sctl_base;

	/* Map control registers */
	sctl_base = vmm_host_iomap(V2M_SYSCTL, 0x1000);

	/* Select 1MHz TIMCLK as the reference clock for SP804 timers */
	val = vmm_readl((void *)sctl_base) | SCCTRL_TIMEREN1SEL_TIMCLK;
	vmm_writel(val, (void *)sctl_base);

	/* Unmap control register */
	rc = vmm_host_iounmap(sctl_base, 0x1000);
	if (rc) {
		return rc;
	}

	/* Map timer1 registers */
	ca9x4_timer1_base = vmm_host_iomap(V2M_TIMER1, 0x1000);

	/* Initialize timer1 as clocksource */
	rc = sp804_clocksource_init(ca9x4_timer1_base, 
				    "sp804_timer1", 300, 1000000, 20);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int __init arch_clockchip_init(void)
{
	int rc;

	/* Map reference counter register */
	if (!ca9x4_sys_24mhz) {
		ca9x4_sys_24mhz = 
			vmm_host_iomap(V2M_SYSREGS + V2M_SYS_24MHZ, 0x1000);
	}

	/* Map SMP twd local timer registers */
	if (!ca9x4_twd_base) {
		ca9x4_twd_base = vmm_host_iomap(A9_MPCORE_TWD, 0x1000);
	}

	/* Initialize SMP twd local timer as clockchip */
	rc = twd_clockchip_init(ca9x4_twd_base, ca9x4_sys_24mhz, 24000000, 
				IRQ_CT_CA9X4_LOCALTIMER, 400);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

