/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file cpu_host_aspace.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief CPU specific source file for host virtual address space management.
 */

#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_error.h>
#include <cpu_mmu.h>
#include <cpu_asm_macros.h>

int vmm_cpu_aspace_init(physical_addr_t * resv_pa, 
			virtual_addr_t * resv_va,
			virtual_size_t * resv_sz)
{
	int i;

	for (i = 0; i < MAX_HOST_TLB_ENTRIES; i++) {
		host_tlb_entries[i].free = 1;
		host_tlb_entries[i].tlb_index = i;
	}

	return 0;
}

static struct host_tlb_entries_info *free_host_tlb_index(void)
{
	int i;

	for (i = 0; i < MAX_HOST_TLB_ENTRIES; i++) {
		if (host_tlb_entries[i].free) {
			return &host_tlb_entries[i];
		}
	}

	return NULL;
}

int vmm_cpu_aspace_map(virtual_addr_t va, 
			virtual_size_t sz, 
			physical_addr_t pa,
			u32 mem_flags)
{
	struct host_tlb_entries_info *tlb_info = free_host_tlb_index();
	struct mips32_tlb_entry tlb_entry;
	u32 page_mask = 0, page_size = 0;

	switch (sz) {
	case TLB_PAGE_SIZE_1K:
	case TLB_PAGE_SIZE_4K:
	case TLB_PAGE_SIZE_16K:
	case TLB_PAGE_SIZE_256K:
	case TLB_PAGE_SIZE_1M:
	case TLB_PAGE_SIZE_4M:
	case TLB_PAGE_SIZE_16M:
	case TLB_PAGE_SIZE_64M:
	case TLB_PAGE_SIZE_256M:
		page_size = sz;
		page_mask = ((sz / 2) - 1);
		break;
	default:
		vmm_panic("Guest physical memory region should be same as page sizes available for MIPS32.\n");
	}


	if (tlb_info) {
		/* Create TLB Hi Entry */
		tlb_entry.entryhi._s_entryhi.asid = (0x01UL << 6);
		tlb_entry.entryhi._s_entryhi.reserved = 0;
		tlb_entry.entryhi._s_entryhi.vpn2 = (va >> VPN2_SHIFT);
		tlb_entry.entryhi._s_entryhi.vpn2x = 0;
		tlb_entry.page_mask = page_mask;

		/* TLB Low entry. Mapping two physical addresses */
		/* FIXME: Take the flag settings from mem_flags. */
		tlb_entry.entrylo0._s_entrylo.global = 0; /* not global */
		tlb_entry.entrylo0._s_entrylo.valid = 1; /* valid */
		tlb_entry.entrylo0._s_entrylo.dirty = 1; /* writeable */
		tlb_entry.entrylo0._s_entrylo.cacheable = 0; /* Dev map, no cache */
		tlb_entry.entrylo0._s_entrylo.pfn = (pa >> PAGE_SHIFT);

		/* We'll map to consecutive physical addresses */
		/* Needed? */
		pa += page_size;
		tlb_entry.entrylo1._s_entrylo.global = 0; /* not global */
		tlb_entry.entrylo1._s_entrylo.valid = 1; /* valid */
		tlb_entry.entrylo1._s_entrylo.dirty = 1; /* writeable */
		tlb_entry.entrylo1._s_entrylo.cacheable = 0; /* Dev map, no cache */
		tlb_entry.entrylo1._s_entrylo.pfn = (pa >> PAGE_SHIFT);

		mips_fill_tlb_entry(&tlb_entry, tlb_info->tlb_index);
		tlb_info->vaddr = va;
		tlb_info->paddr = pa;
		tlb_info->free = 0;

		return VMM_OK;
	}

	return VMM_EFAIL;
}

int vmm_cpu_aspace_unmap(virtual_addr_t va, 
			 virtual_size_t sz)
{
	int i;

	for (i = 0; i < MAX_HOST_TLB_ENTRIES; i++) {
		if (host_tlb_entries[i].vaddr == va) {
			/* FIXME: Invalidate the current tlb entry */
			host_tlb_entries[i].free = 1;
		}
	}

	return VMM_EFAIL;
}

int vmm_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t * pa)
{
	int rc = VMM_EFAIL;
	int i;

	for (i = 0; i < MAX_HOST_TLB_ENTRIES; i++) {
		if ((host_tlb_entries[i].vaddr == va) &&
		    (!host_tlb_entries[i].free)) {
			*pa = host_tlb_entries[i].paddr;
			rc = VMM_OK;
		}
	}

	return rc;
}
