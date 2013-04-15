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
 * @file cpu.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Exynos CPU support code
 *
 * Adapted from linux/arch/arm/plat-samsung/cpu.c
 *
 * Copyright (c) 2009-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung CPU Support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <vmm_macros.h>
#include <vmm_types.h>
#include <vmm_host_aspace.h>
#include <vmm_host_io.h>

#include <exynos/plat/cpu.h>

u32 samsung_cpu_id = 0;
static u32 samsung_cpu_rev = 0;

unsigned int samsung_rev(void)
{
	return samsung_cpu_rev;
}

void __init exynos_init_cpu(physical_addr_t cpuid_addr)
{
	virtual_addr_t virt_addr = vmm_host_iomap(cpuid_addr, sizeof(samsung_cpu_id));

	if (virt_addr) {
		samsung_cpu_id = vmm_readl((void *)virt_addr);
		samsung_cpu_rev = samsung_cpu_id & 0xFF;

		vmm_host_iounmap(virt_addr, sizeof(samsung_cpu_id));
	}
}
