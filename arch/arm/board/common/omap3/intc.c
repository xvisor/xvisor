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
#include <cpu_defines.h>
#include <omap3/intc.h>

static virtual_addr_t omap3_intc_base;

static inline void omap3_intc_write(u32 reg, u32 val)
{
	vmm_writel(val, (void *)(omap3_intc_base + reg));
}

static inline u32 omap3_intc_read(u32 reg)
{
	return vmm_readl((void *)(omap3_intc_base + reg));
}

int omap3_intc_active_irq(u32 cpu_irq)
{
	int ret = -1;
	if (cpu_irq == CPU_EXTERNAL_IRQ) {	/* armv7a IRQ */
		ret = omap3_intc_read(OMAP3_INTC_SIR_IRQ);
		/* Spurious IRQ ? */
		if(((u32) ret & OMAP3_INTC_SIR_IRQ_SPURIOUSFLAG_M)) {
			ret = -1;
		}
		ret = ((u32) ret & OMAP3_INTC_SIR_IRQ_ACTIVEIRQ_M);
		if (OMAP3_MPU_INTC_NRIRQ <= ret) {
			ret = -1;
		}
	} else if (cpu_irq == CPU_EXTERNAL_FIQ) {	/* armv7a FIQ */
		ret = omap3_intc_read(OMAP3_INTC_SIR_FIQ);
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

int omap3_intc_ack_irq(u32 irq)
{
	if (OMAP3_MPU_INTC_NRIRQ <= irq)
		return VMM_EFAIL;

	omap3_intc_write(OMAP3_INTC_CONTROL, OMAP3_INTC_CONTROL_NEWIRQAGR_M);

	return VMM_OK;
}

int omap3_intc_mask(u32 irq)
{
	if (OMAP3_MPU_INTC_NRIRQ <= irq)
		return VMM_EFAIL;

	omap3_intc_write(OMAP3_INTC_MIR((irq / OMAP3_INTC_BITS_PER_REG)),
			 0x1 << (irq & (OMAP3_INTC_BITS_PER_REG - 1)));

	return VMM_OK;
}

int omap3_intc_unmask(u32 irq)
{
	if (OMAP3_MPU_INTC_NRIRQ <= irq)
		return VMM_EFAIL;

	omap3_intc_write(OMAP3_INTC_MIR_CLEAR((irq / OMAP3_INTC_BITS_PER_REG)),
			 0x1 << (irq & (OMAP3_INTC_BITS_PER_REG - 1)));

	return VMM_OK;
}

int omap3_intc_init(void)
{
	u32 tmp;

	omap3_intc_base = vmm_host_iomap(OMAP3_MPU_INTC_BASE, 0x1000);

	tmp = omap3_intc_read(OMAP3_INTC_SYSCONFIG);
	tmp |= OMAP3_INTC_SYSCONFIG_SOFTRST_M;	/* soft reset */
	omap3_intc_write(OMAP3_INTC_SYSCONFIG, tmp);

	/* Wait for reset to complete */
	while (!(omap3_intc_read(OMAP3_INTC_SYSSTATUS) &
		 OMAP3_INTC_SYSSTATUS_RESETDONE_M)) ;

	/* Enable autoidle */
	omap3_intc_write(OMAP3_INTC_SYSCONFIG, OMAP3_INTC_SYSCONFIG_AUTOIDLE_M);

	return VMM_OK;
}
