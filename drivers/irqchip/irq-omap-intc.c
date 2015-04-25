/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file irq-omap-intc.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief OMAP2 interrupt controller APIs
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_host_aspace.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>

/** OMAP3/OMAP343X INTC IRQ Count */
#define INTC_NRIRQ				96

#define INTC_BITS_PER_REG			32

#define INTC_REVISION				0x00000000
#define INTC_REVISION_REV_S			0
#define INTC_REVISION_REV_M			0x000000FF

#define INTC_SYSCONFIG				0x00000010
#define INTC_SYSCONFIG_SOFTRST_S		1
#define INTC_SYSCONFIG_SOFTRST_M		0x00000002
#define INTC_SYSCONFIG_AUTOIDLE_S		0
#define INTC_SYSCONFIG_AUTOIDLE_M		0x00000001

#define INTC_SYSSTATUS				0x00000014
#define INTC_SYSSTATUS_RESETDONE_S 		0
#define INTC_SYSSTATUS_RESETDONE_M		0x00000001

#define INTC_SIR_IRQ				0x00000040
#define INTC_SIR_IRQ_SPURIOUSFLAG_S		7
#define INTC_SIR_IRQ_SPURIOUSFLAG_M		0xFFFFFF80
#define INTC_SIR_IRQ_ACTIVEIRQ_S		0
#define INTC_SIR_IRQ_ACTIVEIRQ_M		0x0000007F

#define INTC_SIR_FIQ				0x00000044
#define INTC_SIR_FIQ_SPURIOUSFLAG_S		7
#define INTC_SIR_FIQ_SPURIOUSFLAG_M		0xFFFFFF80
#define INTC_SIR_FIQ_ACTIVEIRQ_S		0
#define INTC_SIR_FIQ_ACTIVEIRQ_M		0x0000007F

#define INTC_CONTROL				0x00000048
#define INTC_CONTROL_NEWFIQAGR_S		1
#define INTC_CONTROL_NEWFIQAGR_M		0x00000002
#define INTC_CONTROL_NEWIRQAGR_S		0
#define INTC_CONTROL_NEWIRQAGR_M		0x00000001

#define INTC_PROTECTION				0x0000004C
#define INTC_PROTECTION_PROTECTION_S		0
#define INTC_PROTECTION_PROTECTION_M		0x00000001

#define INTC_IDLE				0x00000050
#define INTC_IDLE_TURBO_S			1
#define INTC_IDLE_TURBO_M			0x00000002
#define INTC_IDLE_FUNCIDLE_S			0
#define INTC_IDLE_FUNCIDLE_M			0x00000001

#define INTC_IRQ_PRIORITY			0x00000060
#define INTC_IRQ_PRIORITY_SPURIOUSFLAG_S	6
#define INTC_IRQ_PRIORITY_SPURIOUSFLAG_M	0xFFFFFFC0
#define INTC_IRQ_PRIORITY_ACTIVEIRQ_S		0
#define INTC_IRQ_PRIORITY_IRQPRIORITY_M		0x0000003F

#define INTC_FIQ_PRIORITY			0x00000064
#define INTC_FIQ_PRIORITY_SPURIOUSFLAG_S	6
#define INTC_FIQ_PRIORITY_SPURIOUSFLAG_M	0xFFFFFFC0
#define INTC_FIQ_PRIORITY_ACTIVEIRQ_S		0
#define INTC_FIQ_PRIORITY_IRQPRIORITY_M		0x0000003F

#define INTC_THRESHOLD				0x00000068
#define INTC_THRESHOLD_PRIOTHRESHOLD_S		0
#define INTC_THRESHOLD_PRIOTHRESHOLD_M		0x000000FF

#define INTC_ITR(n)				(0x00000080+(0x20*(n)))

#define INTC_MIR(n)				(0x00000084+(0x20*(n)))

#define INTC_MIR_CLEAR(n)			(0x00000088+(0x20*(n)))

#define INTC_MIR_SET(n)				(0x0000008C+(0x20*(n)))

#define INTC_ISR_SET(n)				(0x00000090+(0x20*(n)))

#define INTC_ISR_CLEAR(n)			(0x00000094+(0x20*(n)))

#define INTC_PENDING_IRQ(n)			(0x00000098+(0x20*(n)))

#define INTC_PENDING_FIQ(n)			(0x0000009C+(0x20*(n)))

#define INTC_ILR(m)				(0x00000100+(0x04*(m)))
#define INTC_ILR_PRIORITY_S			2
#define INTC_ILR_PRIORITY_M			0x000000FC
#define INTC_ILR_FIQNIRQ_S			1
#define INTC_ILR_FIQNIRQ_M			0x00000001

static virtual_addr_t intc_base;
static u32 intc_nrirq;

#define intc_write(reg, val)	vmm_writel((val), \
				(void *)(intc_base + (reg)))
#define intc_read(reg)		vmm_readl((void *)(intc_base + (reg)))

static u32 intc_active_irq(u32 cpu_irq)
{
	u32 ret;

	if (cpu_irq == CPU_EXTERNAL_IRQ) {	/* armv7a IRQ */
		ret = intc_read(INTC_SIR_IRQ);
		/* Spurious IRQ ? */
		if (ret & INTC_SIR_IRQ_SPURIOUSFLAG_M) {
			return UINT_MAX;
		}
		ret = (ret & INTC_SIR_IRQ_ACTIVEIRQ_M);
		if (intc_nrirq <= ret) {
			return UINT_MAX;
		}
	} else if (cpu_irq == CPU_EXTERNAL_FIQ) {	/* armv7a FIQ */
		ret = intc_read(INTC_SIR_FIQ);
		/* Spurious FIQ ? */
		if (ret & INTC_SIR_FIQ_SPURIOUSFLAG_M) {
			return UINT_MAX;
		}
		ret = (ret & INTC_SIR_FIQ_ACTIVEIRQ_M);
		if (intc_nrirq <= ret) {
			return UINT_MAX;
		}
	} else {
		ret = UINT_MAX;
	}

	return ret;
}

static void intc_mask(struct vmm_host_irq *irq)
{
	intc_write(INTC_MIR((irq->num / INTC_BITS_PER_REG)),
		   0x1 << (irq->num & (INTC_BITS_PER_REG - 1)));
}

static void intc_unmask(struct vmm_host_irq *irq)
{
	intc_write(INTC_MIR_CLEAR((irq->num / INTC_BITS_PER_REG)),
		   0x1 << (irq->num & (INTC_BITS_PER_REG - 1)));
}

static void intc_eoi(struct vmm_host_irq *irq)
{
	intc_write(INTC_CONTROL, INTC_CONTROL_NEWIRQAGR_M);
}

static struct vmm_host_irq_chip intc_chip = {
	.name			= "INTC",
	.irq_mask		= intc_mask,
	.irq_unmask		= intc_unmask,
	.irq_eoi		= intc_eoi,
};

static int __init intc_init(physical_addr_t base, u32 nrirq)
{
	u32 i, tmp;

	intc_base = vmm_host_iomap(base, 0x1000);
	intc_nrirq = nrirq;

	tmp = intc_read(INTC_SYSCONFIG);
	tmp |= INTC_SYSCONFIG_SOFTRST_M;	/* soft reset */
	intc_write(INTC_SYSCONFIG, tmp);

	/* Wait for reset to complete */
	while (!(intc_read(INTC_SYSSTATUS) &
		 INTC_SYSSTATUS_RESETDONE_M)) ;

	/* Enable autoidle */
	intc_write(INTC_SYSCONFIG, INTC_SYSCONFIG_AUTOIDLE_M);

	/*
	 * Setup the Host IRQ subsystem.
	 */
	for (i = 0; i < intc_nrirq; i++) {
		vmm_host_irq_set_chip(i, &intc_chip);
		vmm_host_irq_set_handler(i, vmm_handle_fast_eoi);
	}

	/* Set active IRQ callback */
	vmm_host_irq_set_active_callback(intc_active_irq);

	return VMM_OK;
}

static int __cpuinit intc_init_dt(struct vmm_devtree_node *node)
{
	int rc;
	physical_addr_t intc_pa;

	rc = vmm_devtree_regaddr(node, &intc_pa, 0);
	if (rc) {
		return rc;
	}

	return intc_init(intc_pa, INTC_NRIRQ);
}
VMM_HOST_IRQ_INIT_DECLARE(ointc, "ti,omap2-intc", intc_init_dt);
