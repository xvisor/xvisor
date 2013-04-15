/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
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
 * @file avic.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source file for AVIC interrupt conctoler support.
 *
 * Based on linux/arch/arm/mach-imx/avic.c
 *
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2008 Juergen Beisert, kernel@pengutronix.de
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>

#include <imx/avic.h>

static virtual_addr_t avic_base = 0;

static u32 avic_pending_int(u32 status)
{
	u32 ret;

	for (ret = 0; ret < 32; ret++) {
		if ((status >> ret) & 1) {
			return ret;
		}
	}

	/* This should not happen */
	return 0;
}

u32 avic_active_irq(void)
{
	u32 ret = 0;

	u32 int_status;

	if((int_status = vmm_readl((void *)avic_base + AVIC_NIPNDH))) {
		ret = 32 + avic_pending_int(int_status);
	} else if ((int_status = vmm_readl((void *)avic_base + AVIC_NIPNDL))) {
		ret = avic_pending_int(int_status);
	}

	return ret;
}

static void avic_mask_irq(struct vmm_host_irq *irq)
{
	vmm_writel(irq->num, (void *)avic_base + AVIC_INTDISNUM);
}

static void avic_unmask_irq(struct vmm_host_irq *irq)
{
	vmm_writel(irq->num, (void *)avic_base + AVIC_INTENNUM);
}

static void avic_eoi_irq(struct vmm_host_irq *irq)
{
	/* Nothing to do */
}

static int avic_set_type(struct vmm_host_irq *irq, u32 type)
{
	/* For now, nothing to do */
	return VMM_OK;
}

static struct vmm_host_irq_chip avic_chip = {
	.name		= "AVIC",
	.irq_mask	= avic_mask_irq,
	.irq_unmask	= avic_unmask_irq,
	.irq_eoi	= avic_eoi_irq,
	.irq_set_type	= avic_set_type,
};

int __init avic_init(virtual_addr_t base)
{
	u32 i;

	if (!base) {
		return VMM_EFAIL;
	}

	avic_base = base;

	/* put the AVIC into the reset value with
	 * all interrupts disabled
	 */
	vmm_writel(0, (void *)avic_base + AVIC_INTCNTL);
	vmm_writel(0x1f, (void *)avic_base + AVIC_NIMASK);

	/* disable all interrupts */
	vmm_writel(0, (void *)avic_base + AVIC_INTENABLEH);
	vmm_writel(0, (void *)avic_base + AVIC_INTENABLEL);

	/* all IRQ no FIQ */
	vmm_writel(0, (void *)avic_base + AVIC_INTTYPEH);
	vmm_writel(0, (void *)avic_base + AVIC_INTTYPEL);

	/* Set default priority value (0) for all IRQ's */
	for (i = 0; i < 8; i++) {
		vmm_writel(0, (void *)avic_base + AVIC_NIPRIORITY(i));
	}

	/* Set default priority value (0) for all IRQ's */
	for (i = 0; i < AVIC_NUM_IRQS; i++) {
		vmm_host_irq_set_chip(i, &avic_chip);
		vmm_host_irq_set_handler(i, vmm_handle_fast_eoi);
	}

	return VMM_OK;
}
