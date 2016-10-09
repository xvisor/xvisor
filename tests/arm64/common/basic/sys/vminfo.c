/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vminfo.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Guest/VM Info driver source
 */

#include <arm_io.h>
#include <arm_math.h>
#include <sys/vminfo.h>

#define VMINFO_MAGIC_OFFSET		0x00
#define VMINFO_VENDOR_OFFSET		0x04
#define VMINFO_VERSION_OFFSET		0x08
#define VMINFO_VCPU_COUNT_OFFSET	0x0c
#define VMINFO_BOOT_DELAY_OFFSET	0x10
#define VMINFO_RAM0_OFFSET		0x40

#define VMINFO_RAMx_BASE_MS_OFFSET	0x00
#define VMINFO_RAMx_BASE_LS_OFFSET	0x04
#define VMINFO_RAMx_SIZE_MS_OFFSET	0x08
#define VMINFO_RAMx_SIZE_LS_OFFSET	0x0c

#define VMINFO_RAM_BASE_MS_OFFSET(bank)	\
	(VMINFO_RAM0_OFFSET + (bank) * 0x10 + VMINFO_RAMx_BASE_MS_OFFSET)
#define VMINFO_RAM_BASE_LS_OFFSET(bank)	\
	(VMINFO_RAM0_OFFSET + (bank) * 0x10 + VMINFO_RAMx_BASE_LS_OFFSET)
#define VMINFO_RAM_SIZE_MS_OFFSET(bank)	\
	(VMINFO_RAM0_OFFSET + (bank) * 0x10 + VMINFO_RAMx_SIZE_MS_OFFSET)
#define VMINFO_RAM_SIZE_LS_OFFSET(bank)	\
	(VMINFO_RAM0_OFFSET + (bank) * 0x10 + VMINFO_RAMx_SIZE_LS_OFFSET)

u32 vminfo_magic(virtual_addr_t base)
{
	return arm_readl((void *)(base + VMINFO_MAGIC_OFFSET));
}

u32 vminfo_vendor(virtual_addr_t base)
{
	return arm_readl((void *)(base + VMINFO_VENDOR_OFFSET));
}

u32 vminfo_version(virtual_addr_t base)
{
	return arm_readl((void *)(base + VMINFO_VERSION_OFFSET));
}

u32 vminfo_vcpu_count(virtual_addr_t base)
{
	return arm_readl((void *)(base + VMINFO_VCPU_COUNT_OFFSET));
}

u32 vminfo_boot_delay(virtual_addr_t base)
{
	return arm_readl((void *)(base + VMINFO_BOOT_DELAY_OFFSET));
}

physical_addr_t vminfo_ram_base(virtual_addr_t base, u32 bank)
{
	u32 ms, ls;

	if (4 <= bank)
		return 0;

	ms = arm_readl((void *)(base + VMINFO_RAM_BASE_MS_OFFSET(bank)));
	ls = arm_readl((void *)(base + VMINFO_RAM_BASE_LS_OFFSET(bank)));

	return (physical_addr_t)(((u64)ms << 32) | ((u64)ls));
}

physical_size_t vminfo_ram_size(virtual_addr_t base, u32 bank)
{
	u32 ms, ls;

	if (4 <= bank)
		return 0;

	ms = arm_readl((void *)(base + VMINFO_RAM_SIZE_MS_OFFSET(bank)));
	ls = arm_readl((void *)(base + VMINFO_RAM_SIZE_LS_OFFSET(bank)));

	return (physical_size_t)(((u64)ms << 32) | ((u64)ls));
}
