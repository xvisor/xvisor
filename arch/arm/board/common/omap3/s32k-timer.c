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
 * @version 1.0
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source code for OMAP3 32K sync timer
 */

#include <vmm_error.h>
#include <vmm_math.h>
#include <vmm_main.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <omap3/intc.h>
#include <omap3/gpt.h>
#include <omap3/prcm.h>
#include <omap3/s32k-timer.h>

static virtual_addr_t omap35x_32k_synct_base = 0;

u32 omap3_s32k_get_counter(void)
{
	return vmm_readl((void *)(omap35x_32k_synct_base + OMAP3_S32K_CR));
}

int omap3_s32k_init(void)
{
	u32 val;
	virtual_addr_t cm_wkup_base; 
	static int init_done = 0;

	if(!init_done) {
		if(!omap35x_32k_synct_base)
			omap35x_32k_synct_base = 
				vmm_host_iomap(OMAP3_S32K_BASE, 0x1000);
		cm_wkup_base = vmm_host_iomap(OMAP3_WKUP_CM_BASE, 0x100);

		/* Enable I-clock for S32K */
		val = vmm_readl((void *)(cm_wkup_base + OMAP3_CM_ICLKEN_WKUP)) 
			| OMAP3_CM_ICLKEN_WKUP_EN_32KSYNC_M;
		vmm_writel(val, (void *)(cm_wkup_base + OMAP3_CM_ICLKEN_WKUP)); 
		init_done = 1;
		vmm_host_iounmap(cm_wkup_base, 0x100);
	}
	return VMM_OK;
}

