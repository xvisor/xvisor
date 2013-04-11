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
 * @brief source code for OMAP 32K sync timer
 */

#include <vmm_error.h>
#include <vmm_clockchip.h>
#include <vmm_clocksource.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <omap/s32k-timer.h>

static u64 s32k_clocksource_read(struct vmm_clocksource *cs)
{
	return vmm_readl((void *)(cs->priv + S32K_CR));
}

static struct vmm_clocksource s32k_clksrc = {
	.name = "s32k_clksrc",
	.rating = 200,
	.shift = 15,
	.mask = VMM_CLOCKSOURCE_MASK(32),
	.read = &s32k_clocksource_read
};

int __init s32k_clocksource_init(physical_addr_t base)
{
	int rc;
	virtual_addr_t synct_base;

	/* Map registers */
	synct_base = vmm_host_iomap(base, 0x1000);

	/* Save pointer to registers in clocksource private */
	s32k_clksrc.priv = (void *)synct_base;

	/* Compute mult for clocksource */
	vmm_clocks_calc_mult_shift(&s32k_clksrc.mult, &s32k_clksrc.shift, 
				   S32K_FREQ_HZ, VMM_NSEC_PER_SEC, 10);

	/* Register clocksource */
	if ((rc = vmm_clocksource_register(&s32k_clksrc))) {
		return rc;
	}

	return VMM_OK;
}

