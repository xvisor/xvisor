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

#include <omap3/intc.h>
#include <omap3/gpt.h>
#include <omap3/prcm.h>
#include <omap3/s32k-timer.h>
#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>

static virtual_addr_t omap35x_32k_synct_base = 0;

u32 omap3_s32k_get_counter(void)
{
	return vmm_readl((void *)(omap35x_32k_synct_base + OMAP3_S32K_CR));
}

int __init omap3_s32k_init(void)
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

