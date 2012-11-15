/**
 * Copyright (c) 2011 Sukanto Ghosh.
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
 * @file s32k-timer.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source code for OMAP3 32K sync timer
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_clocksource.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <omap/prcm.h>
#include <omap/s32k-timer.h>

static virtual_addr_t omap35x_32k_synct_base = 0;

u32 s32k_get_counter(void)
{
	return vmm_readl((void *)(omap35x_32k_synct_base + OMAP3_S32K_CR));
}

int s32k_init(void)
{
	if(!omap35x_32k_synct_base) {
		omap35x_32k_synct_base = 
			vmm_host_iomap(OMAP3_S32K_BASE, 0x1000);
		/* Enable I-clock for S32K */
		omap3_cm_setbits(OMAP3_WKUP_CM, OMAP3_CM_ICLKEN_WKUP,
				OMAP3_CM_ICLKEN_WKUP_EN_32KSYNC_M);
	}
	return VMM_OK;
}

static u64 s32k_clocksource_read(struct vmm_clocksource *cs)
{
	return vmm_readl((void *)(omap35x_32k_synct_base + OMAP3_S32K_CR));
}

static struct vmm_clocksource s32k_clksrc = {
	.name = "s32k_clksrc",
	.rating = 200,
	.shift = 15,
	.mask = 0xFFFFFFFF,
	.read = &s32k_clocksource_read
};

int __init s32k_clocksource_init(void)
{
	int rc;

	/* Initialize omap3 s32k timer HW */
	if ((rc = s32k_init())) {
		return rc;
	}

	/* Register clocksource */
	s32k_clksrc.mult = vmm_clocksource_hz2mult(OMAP3_S32K_FREQ_HZ, 15);
	if ((rc = vmm_clocksource_register(&s32k_clksrc))) {
		return rc;
	}

	return VMM_OK;
}

