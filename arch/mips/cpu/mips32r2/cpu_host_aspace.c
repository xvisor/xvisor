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
#include <vmm_host_aspace.h>
#include <vmm_cpu.h>

extern u8 _code_end;
extern u8 _code_start;
u32 nr_ptabs = 0;

u32 nr_pg_szes[9] = {
	TLB_PAGE_SIZE_1K,
	TLB_PAGE_SIZE_4K,
	TLB_PAGE_SIZE_16K,
	TLB_PAGE_SIZE_256K,
	TLB_PAGE_SIZE_1M,
	TLB_PAGE_SIZE_4M,
	TLB_PAGE_SIZE_16M,
	TLB_PAGE_SIZE_64M,
	TLB_PAGE_SIZE_256M
};

int vmm_cpu_aspace_map(virtual_addr_t va,
		       virtual_size_t sz,
		       physical_addr_t pa,
		       u32 mem_flags);
virtual_size_t vmm_cpu_code_base_size(void);

static pgd_t host_pgd[NUM_PGD_ENTRIES];

static int vmm_cpu_boot_pagetable_init(physical_addr_t pa,
				       virtual_addr_t va,
				       virtual_size_t sz)
{
	int i, j, done = 0;
	u32 pgd_offset;
	ptab_t *c_ptab;
	pte_t *spte;
	struct mips32_tlb_entry tlb_entry;
	u32 page_mask = 0, page_size = 0;
	virtual_addr_t cva = va;

	/*
	 * Find the largest page size that can map maximum of
	 * the given virutal address space.
	 */
	for (i = 8; i >= 0; i--) {
		j = ((int)sz - nr_pg_szes[i]);
		if (j > 0) {
			done = 1;
			break;
		}
	}

	if (!done)
		return VMM_EFAIL;

	page_size = nr_pg_szes[i + 1];
	page_mask = ((page_size / 2) - 1);

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
	tlb_entry.entrylo1._s_entrylo.valid = 1; /* Invalid */
	tlb_entry.entrylo1._s_entrylo.dirty = 1; /* writeable */
	tlb_entry.entrylo1._s_entrylo.cacheable = 0; /* Dev map, no cache */
	tlb_entry.entrylo1._s_entrylo.pfn = (pa >> PAGE_SHIFT);

	/* FIXME: This should be a wired entry */
	mips_fill_tlb_entry(&tlb_entry, -1);

	cva += sz;
	spte = (pte_t *)(va + vmm_cpu_code_base_size() + (nr_ptabs * PAGE_SIZE));
	c_ptab = (ptab_t *)(va + vmm_cpu_code_base_size());

	/* Initialize the PGD */
	for (i = 0; i < nr_ptabs; i++) {
		pgd_offset = (cva >> PGD_SHIFT) & PGD_MASK;
		host_pgd[pgd_offset] = (pgd_t)(c_ptab);

		/* Populate the ptabs with pte entries. */
		for (j = 0; j < NUM_PTAB_ENTRIES; j++) {
			c_ptab[j] = (ptab_t)spte;
			spte++;
		}

		c_ptab += NUM_PTAB_ENTRIES;
		cva += (NUM_PTAB_ENTRIES * PAGE_SIZE);
	}


	return VMM_OK;
}

int vmm_cpu_aspace_init(physical_addr_t * resv_pa,
			virtual_addr_t * resv_va,
			virtual_size_t * resv_sz)
{
	u32 c0_sr;
	physical_addr_t pa = vmm_cpu_code_paddr_start();
	virtual_addr_t va = vmm_cpu_code_vaddr_start();
	virtual_size_t sz = vmm_cpu_code_size();

	if (vmm_cpu_boot_pagetable_init(pa, va, sz) != VMM_OK)
		return VMM_EFAIL;


	/*
	 * Now that the page tables are set, we are ready to handle
	 * page faults. So clear the BEV bit.
	 */
	c0_sr = read_c0_status();
	c0_sr &= ~(0x01 << 22);
	write_c0_status(c0_sr);

	return VMM_OK;
}

pte_t *vmm_cpu_va2pte(virtual_addr_t vaddr)
{
	ptab_t *ptab;
	pgd_t pgd_offset = ((vaddr >> PGD_SHIFT) & PGD_MASK);
	ptab = (ptab_t *)host_pgd[pgd_offset];

	return (pte_t *)ptab[((vaddr >> PTAB_SHIFT) & PTAB_MASK)];
}

static void vmm_cpu_create_pte(virtual_addr_t vaddr, physical_addr_t paddr,
			       u32 flags, mips32_tlb_entry_t *tlb_entry)
{
	pte_t *pte = NULL;
	ptab_t *ptab = NULL;
	u32 ptab_id = ((vaddr >> PTAB_SHIFT) & PTAB_MASK);
	u32 pgd_id = ((vaddr >> PGD_SHIFT) & PGD_MASK);

	ptab = (ptab_t *)host_pgd[pgd_id];
	pte = (pte_t *)ptab[ptab_id];

	pte->vaddr = vaddr;
	pte->paddr = paddr;
	pte->flags = flags;
}

int vmm_cpu_aspace_map(virtual_addr_t va,
			virtual_size_t sz,
			physical_addr_t pa,
			u32 mem_flags)
{
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

	vmm_cpu_create_pte(va, pa - page_size, mem_flags, &tlb_entry);

	return VMM_OK;
}

int vmm_cpu_aspace_unmap(virtual_addr_t va,
			 virtual_size_t sz)
{
	return VMM_EFAIL;
}

int vmm_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t * pa)
{
	pte_t *pte = NULL;
	ptab_t *ptab = NULL;
	u32 ptab_id = ((va >> PTAB_SHIFT) & PTAB_MASK);
	u32 pgd_id = ((va >> PGD_SHIFT) & PGD_MASK);

	ptab = (ptab_t *)host_pgd[pgd_id];
	pte = (pte_t *)ptab[ptab_id];

	*pa = pte->paddr;

	return VMM_OK;
}

virtual_addr_t vmm_cpu_code_vaddr_start(void)
{
	return ((virtual_addr_t) 0xC0000000);
}

physical_addr_t vmm_cpu_code_paddr_start(void)
{
	return ((physical_addr_t) 0);
}

virtual_size_t vmm_cpu_code_base_size(void)
{
	return (virtual_size_t)(&_code_end - &_code_start);
}

virtual_size_t vmm_cpu_pg_table_size(void)
{
	/*
	 * Do the calculation of total physical space required
	 * for creating page table to map PG_TABLE_INIT_MAP_SZ
	 * amount of virtual addresses.
	 */
	u32 nr_pte_per_page = PAGE_SIZE / sizeof(pte_t);
	/* Total number of PTEs for mapping PG_TABLE_INIT_MAP_SZ */
	u32 nr_ptes = (PG_TABLE_INIT_MAP_SZ / PAGE_SIZE);
	/* Total pages required to keep above calculated PTEs */
	u32 nr_pte_pages = (nr_ptes * sizeof(pte_t))/PAGE_SIZE;
	/* Number of ptabs to map above number of ptes */
	nr_ptabs = (nr_pte_pages * nr_pte_per_page)/NUM_PTAB_ENTRIES;

	/*
	 * Total memory required (including ptabs and pte pages)
	 * to map the required virtual addresses.
	 */
	u32 need_sz = (nr_ptabs + nr_pte_pages) * PAGE_SIZE;

	return VMM_ROUNDUP2_PAGE_SIZE(need_sz);
}

virtual_size_t vmm_cpu_code_size(void)
{
	virtual_size_t rsz;

	rsz = vmm_cpu_code_base_size() + vmm_cpu_pg_table_size();

	rsz = VMM_ROUNDUP2_PAGE_SIZE(rsz);
	rsz &= PAGE_MASK;

	return rsz;
}
