/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file bcm2836.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2836 SOC specific code
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_host_io.h>
#include <libs/mathlib.h>

#include <generic_timer.h>
#include <cpu_generic_timer.h>

#include <generic_board.h>

/* When setting bits 0-3, enables PMU interrupts on that CPU. */
#define LOCAL_TIMER_PRESCALER		0x008

/*
 * Initialization functions
 */

static int __init bcm2836_early_init(struct vmm_devtree_node *node)
{
	int rc = VMM_OK;
	void *base;
	u32 prescaler, cntfreq;
	virtual_addr_t base_va;
	struct vmm_devtree_node *np;

	np = vmm_devtree_find_compatible(NULL, NULL, "brcm,bcm2836-l1-intc");
	if (!np) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_regmap(np, &base_va, 0);
	if (rc) {
		goto done;
	}
	base = (void *)base_va;

	cntfreq = generic_timer_reg_read(GENERIC_TIMER_REG_FREQ);
	switch (cntfreq) {
	case 19200000:
		prescaler = 0x80000000;
	case 1000000:
		prescaler = 0x06AAAAAB;
	default:
		prescaler = (u32)udiv64((u64)0x80000000 * (u64)cntfreq,
					(u64)19200000);
		break;
	};

	if (!prescaler) {
		rc = VMM_EINVALID;
		goto done_unmap;
	}

	vmm_writel(prescaler, base + LOCAL_TIMER_PRESCALER);

done_unmap:
	vmm_devtree_regunmap(node, base_va, 0);

done:
	vmm_devtree_dref_node(np);

	return rc;
}

static int __init bcm2836_final_init(struct vmm_devtree_node *node)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static struct generic_board bcm2836_info = {
	.name		= "BCM2836",
	.early_init	= bcm2836_early_init,
	.final_init	= bcm2836_final_init,
};

GENERIC_BOARD_DECLARE(bcm2836, "brcm,bcm2836", &bcm2836_info);
