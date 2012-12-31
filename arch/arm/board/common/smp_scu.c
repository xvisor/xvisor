/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file smp_scu.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief SCU API code
 *
 * Adapted from linux/arch/arm/kernel/smp_scu.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <vmm_host_io.h>
#include <vmm_cache.h>
#include <vmm_error.h>
#include <vmm_smp.h>
#include <smp_scu.h>

#define SCU_CTRL		0x00
#define SCU_CONFIG		0x04
#define SCU_CPU_STATUS		0x08
#define SCU_INVALIDATE		0x0c
#define SCU_FPGA_REVISION	0x10

#ifdef CONFIG_SMP
/*
 * Get the number of CPU cores from the SCU configuration
 */
u32 scu_get_core_count(void *scu_base)
{
	return (vmm_readl(scu_base + SCU_CONFIG) & 0x03) + 1;
}

bool scu_cpu_core_is_smp(void *scu_base, u32 cpu)
{
	return (vmm_readl(scu_base + SCU_CONFIG) >> (4 + cpu)) & 0x01;
}

/*
 * Enable the SCU
 */
void scu_enable(void *scu_base)
{
	u32 scu_ctrl;

#ifdef CONFIG_ARM_ERRATA_764369
	/*
	 * This code is mostly for TEGRA 2 and 3 processors. 
	 * This in not enabled or tested on Xvisor for now.
	 * We keep it as we might have to enable it someday.
	 */
	/* Cortex-A9 only */
	if ((read_cpuid(CPUID_ID) & 0xff0ffff0) == 0x410fc090) {

		scu_ctrl = vmm_readl(scu_base + 0x30);
		if (!(scu_ctrl & 1)) {
			vmm_writel(scu_ctrl | 0x1, scu_base + 0x30);
		}
	}
#endif

	scu_ctrl = vmm_readl(scu_base + SCU_CTRL);
	/* already enabled? */
	if (scu_ctrl & 1) {
		return;
	}

	scu_ctrl |= 1;
	vmm_writel(scu_ctrl, scu_base + SCU_CTRL);

	/*
	 * Ensure that the data accessed by CPU0 before the SCU was
	 * initialised is visible to the other CPUs.
	 */
	vmm_flush_cache_all();
}
#endif

/*
 * Set the executing CPUs power mode as defined.  This will be in
 * preparation for it executing a WFI instruction.
 *
 * This function must be called with preemption disabled, and as it
 * has the side effect of disabling coherency, caches must have been
 * flushed.  Interrupts must also have been disabled.
 */
int scu_power_mode(void *scu_base, u32 mode)
{
	u32 val, cpu;

	cpu = vmm_smp_processor_id();

	if (mode > SCU_PM_POWEROFF || mode == SCU_PM_EINVAL || cpu > 3) {
		return VMM_EFAIL;
	}

	val = vmm_readb(scu_base + SCU_CPU_STATUS + cpu) & ~0x03;
	val |= mode;
	vmm_writeb(val, scu_base + SCU_CPU_STATUS + cpu);

	return VMM_OK;
}
