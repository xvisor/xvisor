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
 * @file intc.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief OMAP3 interrupt controller APIs
 */

#include <vmm_error.h>
#include <vmm_host_aspace.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <cpu_defines.h>
#include <omap3/intc.h>

static virtual_addr_t omap3_intc_base;

#define intc_write(reg, val)	vmm_writel((val), \
				(void *)(omap3_intc_base + (reg)))
#define intc_read(reg)		vmm_readl((void *)(omap3_intc_base + (reg)))

u32 omap3_intc_active_irq(u32 cpu_irq)
{
	u32 ret = 0xFFFFFFFF;
	if (cpu_irq == CPU_EXTERNAL_IRQ) {	/* armv7a IRQ */
		ret = intc_read(OMAP3_INTC_SIR_IRQ);
		/* Spurious IRQ ? */
		if(((u32) ret & OMAP3_INTC_SIR_IRQ_SPURIOUSFLAG_M)) {
			ret = -1;
		}
		ret = ((u32) ret & OMAP3_INTC_SIR_IRQ_ACTIVEIRQ_M);
		if (OMAP3_MPU_INTC_NRIRQ <= ret) {
			ret = -1;
		}
	} else if (cpu_irq == CPU_EXTERNAL_FIQ) {	/* armv7a FIQ */
		ret = intc_read(OMAP3_INTC_SIR_FIQ);
		/* Spurious FIQ ? */
		if(((u32) ret & OMAP3_INTC_SIR_FIQ_SPURIOUSFLAG_M)) {
			ret = -1;
		}
		ret = ((u32) ret & OMAP3_INTC_SIR_FIQ_ACTIVEIRQ_M);
		if (OMAP3_MPU_INTC_NRIRQ <= ret) {
			ret = -1;
		}
	}
	return ret;
}

void omap3_intc_ack(struct vmm_host_irq *irq)
{
	intc_write(OMAP3_INTC_CONTROL, OMAP3_INTC_CONTROL_NEWIRQAGR_M);
}

void omap3_intc_mask(struct vmm_host_irq *irq)
{
	intc_write(OMAP3_INTC_MIR((irq->num / OMAP3_INTC_BITS_PER_REG)),
		   0x1 << (irq->num & (OMAP3_INTC_BITS_PER_REG - 1)));
}

void omap3_intc_unmask(struct vmm_host_irq *irq)
{
	intc_write(OMAP3_INTC_MIR_CLEAR((irq->num / OMAP3_INTC_BITS_PER_REG)),
		   0x1 << (irq->num & (OMAP3_INTC_BITS_PER_REG - 1)));
}

static struct vmm_host_irq_chip intc_chip = {
	.name			= "INTC",
	.irq_ack		= omap3_intc_ack,
	.irq_mask		= omap3_intc_mask,
	.irq_unmask		= omap3_intc_unmask,
};

int __init omap3_intc_init(void)
{
	u32 i, tmp;

	omap3_intc_base = vmm_host_iomap(OMAP3_MPU_INTC_BASE, 0x1000);

	tmp = intc_read(OMAP3_INTC_SYSCONFIG);
	tmp |= OMAP3_INTC_SYSCONFIG_SOFTRST_M;	/* soft reset */
	intc_write(OMAP3_INTC_SYSCONFIG, tmp);

	/* Wait for reset to complete */
	while (!(intc_read(OMAP3_INTC_SYSSTATUS) &
		 OMAP3_INTC_SYSSTATUS_RESETDONE_M)) ;

	/* Enable autoidle */
	intc_write(OMAP3_INTC_SYSCONFIG, OMAP3_INTC_SYSCONFIG_AUTOIDLE_M);

	/*
	 * Setup the Host IRQ subsystem.
	 */
	for (i = 0; i < OMAP3_MPU_INTC_NRIRQ; i++) {
		vmm_host_irq_set_chip(i, &intc_chip);
	}

	return VMM_OK;
}
