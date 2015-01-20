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
 * @file irq-avic.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief source file for AVIC interrupt controller support.
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
#include <vmm_limits.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>

#define AVIC_INTCNTL		0x00	/* int control reg */
#define AVIC_NIMASK		0x04	/* int mask reg */
#define AVIC_INTENNUM		0x08	/* int enable number reg */
#define AVIC_INTDISNUM		0x0C	/* int disable number reg */
#define AVIC_INTENABLEH		0x10	/* int enable reg high */
#define AVIC_INTENABLEL		0x14	/* int enable reg low */
#define AVIC_INTTYPEH		0x18	/* int type reg high */
#define AVIC_INTTYPEL		0x1C	/* int type reg low */
#define AVIC_NIPRIORITY(x)	(0x20 + 4 * (7 - (x))) /* int priority */
#define AVIC_NIVECSR		0x40	/* norm int vector/status */
#define AVIC_FIVECSR		0x44	/* fast int vector/status */
#define AVIC_INTSRCH		0x48	/* int source reg high */
#define AVIC_INTSRCL		0x4C	/* int source reg low */
#define AVIC_INTFRCH		0x50	/* int force reg high */
#define AVIC_INTFRCL		0x54	/* int force reg low */
#define AVIC_NIPNDH		0x58	/* norm int pending high */
#define AVIC_NIPNDL		0x5C	/* norm int pending low */
#define AVIC_FIPNDH		0x60	/* fast int pending high */
#define AVIC_FIPNDL		0x64	/* fast int pending low */

#define AVIC_NUM_IRQS 64

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

static u32 avic_active_irq(u32 cpu_irq_no)
{
	u32 ret, int_status;

	if ((int_status = vmm_readl((void *)avic_base + AVIC_NIPNDH))) {
		ret = 32 + avic_pending_int(int_status);
	} else if ((int_status = vmm_readl((void *)avic_base + AVIC_NIPNDL))) {
		ret = avic_pending_int(int_status);
	} else {
		ret = UINT_MAX;
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

static int __init avic_init(struct vmm_devtree_node *node)
{
	int rc;
	u32 i;

	/* Map AVIC registers */
	rc = vmm_devtree_request_regmap(node, &avic_base, 0, "AVIC");
	WARN(rc, "unable to map avic registers\n");
	if (rc) {
		return rc;
	}

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

	/* Set active irq callback */
	vmm_host_irq_set_active_callback(avic_active_irq);

	return VMM_OK;
}
VMM_HOST_IRQ_INIT_DECLARE(favic, "freescale,avic", avic_init);

