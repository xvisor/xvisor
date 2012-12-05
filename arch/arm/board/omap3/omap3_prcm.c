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
 * @file omap3_prcm.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source code for OMAP3 Power, Reset, and Clock Managment
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <omap3_plat.h>
#include <omap3_prcm.h>

static virtual_addr_t cm_base = 0;

int __init cm_init(void)
{
	if(!cm_base) {
		cm_base = vmm_host_iomap(OMAP3_CM_BASE, OMAP3_CM_SIZE);
		if(!cm_base)
			return VMM_EFAIL;
	}
	return VMM_OK;
}

u32 cm_read(u32 domain, u32 offset)
{
	return vmm_readl((void *)(cm_base + domain + offset));
}

void cm_write(u32 domain, u32 offset, u32 val)
{
	vmm_writel(val, (void *)(cm_base + domain + offset));
}

void cm_setbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(cm_base + domain + offset)) | mask,
		(void *)(cm_base + domain + offset));
}

void cm_clrbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(cm_base + domain + offset)) & (~mask),
		(void *)(cm_base + domain + offset));
}

static virtual_addr_t prm_base = 0;

int __init prm_init(void)
{
	if(!prm_base) {
		prm_base = vmm_host_iomap(OMAP3_PRM_BASE, OMAP3_PRM_SIZE);
		if(!prm_base)
			return VMM_EFAIL;
	}
	return VMM_OK;
}

u32 prm_read(u32 domain, u32 offset)
{
	return vmm_readl((void *)(prm_base + domain + offset));
}

void prm_write(u32 domain, u32 offset, u32 val)
{
	vmm_writel(val, (void *)(prm_base + domain + offset));
}

void prm_setbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(prm_base + domain + offset)) | mask,
		(void *)(prm_base + domain + offset));
}

void prm_clrbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(prm_base + domain + offset)) & (~mask),
		(void *)(prm_base + domain + offset));
}

