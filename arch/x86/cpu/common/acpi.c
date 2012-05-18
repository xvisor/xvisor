/**
 * Copyright (c) 2012 Himanshu Chauhan.
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
 * @file acpi.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief ACPI parser.
 */

#include <vmm_types.h>
#include <cpu_apic.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <cpu_mmu.h>
#include <arch_cpu.h>
#include <cpu_private.h>
#include <acpi.h>

u32 nr_sys_hdr;

struct acpi_search_area acpi_areas[] = {
	{ "Extended BIOS Data Area (EBDA)", 0x0009FC00, 0x0009FFFF },
	{ "BIOS Read-Only Memory", 0xE0000, 0xFFFFF },
	{ NULL, 0, 0 },
};

static u64 locate_rsdp_in_area(virtual_addr_t vaddr, u32 size)
{
	virtual_addr_t addr;

	for (addr = vaddr; addr < (vaddr + size); addr += 16) {
		if (!vmm_memcmp((const void *)addr, RSDP_SIGNATURE, 8)) {
			return addr;
		}
	}

	return 0;
}

static virtual_addr_t find_root_system_descriptor(void)
{
	struct acpi_search_area *carea = &acpi_areas[0];
	virtual_addr_t area_map;
	virtual_addr_t rsdp_base = 0;

	while (carea->area_name) {
		area_map = vmm_host_iomap(carea->phys_start,
					  (carea->phys_end
					   - carea->phys_start));
		BUG_ON((void *)area_map == NULL,
		       "Failed to map the %s for RSDP search.\n",
		       carea->area_name);

		if ((rsdp_base
		     = locate_rsdp_in_area(area_map,
					   (carea->phys_end
					    - carea->phys_start))) != 0) {
			break;
		}

		rsdp_base = 0;
		carea++;
		vmm_host_iounmap(area_map,
				 (carea->phys_end - carea->phys_start));
	}

	return rsdp_base;
}

#if 0
void check_all_desc(u32 *base)
{
	int i = 0;
	struct sdt_header *hdr;

	for (i = 0; i < nr_sys_hdr; i++) {
		hdr = (struct sdt_header *)vmm_host_iomap(*base, PAGE_SIZE);
		base++;
		vmm_host_iounmap((virtual_addr_t)hdr, PAGE_SIZE);
	}
}
#endif

int acpi_init(void)
{
	struct rsdp *root_desc = (struct rsdp *)find_root_system_descriptor();
	struct sdt_header *rsdt = NULL;

	BUG_ON((root_desc == NULL), "Couldn't find any root system descriptor table."
	       "Can't configure system without ACPI.\n");

	BUG_ON((root_desc->rsdt_addr == 0), "No root descriptor found in RSD Pointer.\n");

	rsdt = (struct sdt_header *)vmm_host_iomap(root_desc->rsdt_addr, sizeof(*root_desc));
	BUG_ON((rsdt == NULL), "Failed to map root descriptor base.\n");
	BUG_ON((vmm_memcmp(rsdt->signature, RSDT_SIGNATURE, 4)), "Invalid RSDT signature found.\n");

	nr_sys_hdr = (rsdt->len - sizeof(struct sdt_header))/4;

	return 0;
}
