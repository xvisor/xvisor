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
 * @file prcm.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source code for OMAP3 Power, Reset, and Clock Managment
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <omap/prcm.h>

virtual_addr_t omap3_cm_base = 0;
virtual_addr_t omap3_prm_base = 0;

int __init omap3_cm_init(void)
{
	if(!omap3_cm_base) {
		omap3_cm_base = vmm_host_iomap(OMAP3_CM_BASE, OMAP3_CM_SIZE);
		if(!omap3_cm_base)
			return VMM_EFAIL;
	}
	return VMM_OK;
}

int __init omap3_prm_init(void)
{
	if(!omap3_prm_base) {
		omap3_prm_base = vmm_host_iomap(OMAP3_PRM_BASE, OMAP3_PRM_SIZE);
		if(!omap3_prm_base)
			return VMM_EFAIL;
	}
	return VMM_OK;
}

u32 omap3_cm_read(u32 domain, u32 offset)
{
	return vmm_readl((void *)(omap3_cm_base + domain + offset));
}

void omap3_cm_write(u32 domain, u32 offset, u32 val)
{
	vmm_writel(val, (void *)(omap3_cm_base + domain + offset));
}

u32 omap3_prm_read(u32 domain, u32 offset)
{
	return vmm_readl((void *)(omap3_prm_base + domain + offset));
}

void omap3_prm_write(u32 domain, u32 offset, u32 val)
{
	vmm_writel(val, (void *)(omap3_prm_base + domain + offset));
}

void omap3_cm_setbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(omap3_cm_base + domain + offset)) | mask,
		(void *)(omap3_cm_base + domain + offset));
}

void omap3_cm_clrbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(omap3_cm_base + domain + offset)) & (~mask),
		(void *)(omap3_cm_base + domain + offset));
}

void omap3_prm_setbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(omap3_prm_base + domain + offset)) | mask,
		(void *)(omap3_prm_base + domain + offset));
}

void omap3_prm_clrbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(omap3_prm_base + domain + offset)) & (~mask),
		(void *)(omap3_prm_base + domain + offset));
}

