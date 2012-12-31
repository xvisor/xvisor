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
 * @brief OMAP interrupt controller APIs
 */

#include <vmm_error.h>
#include <vmm_host_aspace.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <cpu_defines.h>
#include <omap/intc.h>

static virtual_addr_t intc_base;
static u32 intc_nrirq;

#define intc_write(reg, val)	vmm_writel((val), \
				(void *)(intc_base + (reg)))
#define intc_read(reg)		vmm_readl((void *)(intc_base + (reg)))

u32 intc_active_irq(u32 cpu_irq)
{
	u32 ret = 0xFFFFFFFF;
	if (cpu_irq == CPU_EXTERNAL_IRQ) {	/* armv7a IRQ */
		ret = intc_read(INTC_SIR_IRQ);
		/* Spurious IRQ ? */
		if(((u32) ret & INTC_SIR_IRQ_SPURIOUSFLAG_M)) {
			ret = -1;
		}
		ret = ((u32) ret & INTC_SIR_IRQ_ACTIVEIRQ_M);
		if (intc_nrirq <= ret) {
			ret = -1;
		}
	} else if (cpu_irq == CPU_EXTERNAL_FIQ) {	/* armv7a FIQ */
		ret = intc_read(INTC_SIR_FIQ);
		/* Spurious FIQ ? */
		if(((u32) ret & INTC_SIR_FIQ_SPURIOUSFLAG_M)) {
			ret = -1;
		}
		ret = ((u32) ret & INTC_SIR_FIQ_ACTIVEIRQ_M);
		if (intc_nrirq <= ret) {
			ret = -1;
		}
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

int __init intc_init(physical_addr_t base, u32 nrirq)
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

	return VMM_OK;
}
