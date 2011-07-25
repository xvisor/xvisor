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

#include <vmm_string.h>
#include <vmm_error.h>
#include <cpu_mmu.h>
#include <cpu_asm_macros.h>

int vmm_cpu_aspace_init(void)
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

int vmm_cpu_iomap(virtual_addr_t va, virtual_size_t sz,
		  physical_addr_t pa)
{
	struct host_tlb_entries_info *tlb_info = free_host_tlb_index();
	struct mips32_tlb_entry tlb_entry;

	if (tlb_info) {
		/* Create TLB Hi Entry */
		tlb_entry.entryhi._s_entryhi.asid = (0x01UL << 7);
		tlb_entry.entryhi._s_entryhi.reserved = 0;
		tlb_entry.entryhi._s_entryhi.vpn2 = (va >> VPN2_SHIFT);
		tlb_entry.entryhi._s_entryhi.vpn2x = 0;
		tlb_entry.page_mask = PAGE_MASK;

		/* TLB Low entry. Mapping two physical addresses */
		tlb_entry.entrylo0._s_entrylo.global = 0; /* not global */
		tlb_entry.entrylo0._s_entrylo.valid = 1; /* valid */
		tlb_entry.entrylo0._s_entrylo.dirty = 1; /* writeable */
		tlb_entry.entrylo0._s_entrylo.cacheable = 0; /* Dev map, no cache */
		tlb_entry.entrylo0._s_entrylo.pfn = (pa >> PAGE_SHIFT);

		/* We'll map to consecutive physical addresses */
		/* Needed? */
		pa += PAGE_SIZE;
		tlb_entry.entrylo1._s_entrylo.global = 0; /* not global */
		tlb_entry.entrylo1._s_entrylo.valid = 1; /* valid */
		tlb_entry.entrylo1._s_entrylo.dirty = 1; /* writeable */
		tlb_entry.entrylo1._s_entrylo.cacheable = 0; /* Dev map, no cache */
		tlb_entry.entrylo1._s_entrylo.pfn = (pa >> PAGE_SHIFT);

		fill_tlb_entry(&tlb_entry, tlb_info->tlb_index);
		tlb_info->vaddr = va;
		tlb_info->paddr = pa;
		tlb_info->free = 0;

		return VMM_OK;
	}

	return VMM_EFAIL;
}

int vmm_cpu_iounmap(virtual_addr_t va, virtual_size_t sz)
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
