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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief CPU specific source file for host virtual address space management.
 */

#include <arch_cpu.h>
#include <arch_sections.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <cpu_mmu.h>
#include <cpu_asm_macros.h>

extern u8 _code_end;
extern u8 _code_start;

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

int arch_cpu_aspace_map(virtual_addr_t va,
		       virtual_size_t sz,
		       physical_addr_t pa,
		       u32 mem_flags);
virtual_size_t cpu_code_base_size(void);

static pgd_t host_pgd[NUM_PGD_ENTRIES];

static virtual_size_t calculate_page_table_size(virtual_size_t sz,
						u32 *nr_ptabs)
{
	/*
	 * Do the calculation of total physical space required
	 * for creating page table to map PG_TABLE_INIT_MAP_SZ
	 * amount of virtual addresses.
	 */
	u32 nr_pte_per_page = PAGE_SIZE / sizeof(pte_t);
	/* Total number of PTEs for mapping the given size. */
	u32 nr_ptes = (sz / PAGE_SIZE);
	/* Total pages required to keep above calculated PTEs */
	u32 nr_pte_pages = (nr_ptes * sizeof(pte_t))/PAGE_SIZE;
	/* Number of ptabs to map above number of ptes */
	*nr_ptabs = (nr_pte_pages * nr_pte_per_page)/NUM_PTAB_ENTRIES;

	/*
	 * Total memory required (including ptabs and pte pages)
	 * to map the required virtual addresses.
	 */
	u32 need_sz = (*nr_ptabs + nr_pte_pages) * PAGE_SIZE;

	return VMM_ROUNDUP2_PAGE_SIZE(need_sz);
}

static int cpu_boot_pagetable_init(physical_addr_t *pa,
				       virtual_addr_t *va,
				       virtual_size_t *sz)
{
	int i, j;
	u32 pgd_offset;
	ptab_t *c_ptab;
	pte_t *spte;
	virtual_addr_t cva, eva = 0;
	u32 nr_ptabs = 0, pg_tab_sz = 0, tsize2map;

	tsize2map = CONFIG_VAPOOL_SIZE_MB << 20;

	pg_tab_sz = calculate_page_table_size(tsize2map, &nr_ptabs);

	/*+----------------------------------------+ (CONFIG_VAPOOL
	  |                                        |     	  +
	  |                                        |  VMM CODE DATA
	  |                VIRTUAL                 |	  +
	  |               ADDRESSES                |  PAGE TABLE SIZE)
	  |                   TO                   |
	  |                   BE                   |
	  |                 MAPPED                 |
	  |                                        |
	  |                                        |
	  |                                        |
	  +----------------------------------------+
	  |                                        |
	  |      RESERVED AREA FOR ALLOCATION BITM |
	  |                                        |
	  +----------------------------------------+
	  |             PAGE TABLES                |
	  +----------------------------------------+
	  |                                        |
	  |            VMM CODE + DATA             |
	  |                                        |
	  |                                        |
	  +----------------------------------------+ 0x00000000*/
	/*
	 * Map virtual addresses from the end of VMM. Why? We don't
	 * expect text section to fault.
	 */
	cva = arch_code_vaddr_start() + arch_code_size();

	spte = (pte_t *)(CPU_TEXT_START + arch_code_size() + (*sz)
			 + (nr_ptabs * PAGE_SIZE));
	c_ptab = (ptab_t *)(CPU_TEXT_START + arch_code_size() + (*sz));

	/* Initialize the PGD */
	for (i = 0; i <= nr_ptabs; i++) {
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

	cva = arch_code_vaddr_start() + arch_code_size();
	eva += cva + ((CONFIG_VAPOOL_SIZE_MB << 20) - 
				(arch_code_size() + (*sz) + pg_tab_sz));
	*pa += arch_code_size();

	/* Create the page table entries for all the virtual addresses. */
	for (; cva < eva;) {
		if (arch_cpu_aspace_map(cva, VMM_PAGE_SIZE, *pa, 0) != VMM_OK)
			return VMM_EFAIL;

		cva += VMM_PAGE_SIZE;
		*pa += VMM_PAGE_SIZE;
	}

	/*
	 * Set current ASID to VMM's ASID. We need to handle
	 * our own page faults.
	 */
	set_current_asid(0x1UL << 6);
	*sz += pg_tab_sz;

	return VMM_OK;
}

int __init arch_cpu_aspace_primary_init(physical_addr_t * core_resv_pa, 
					virtual_addr_t * core_resv_va,
					virtual_size_t * core_resv_sz,
					physical_addr_t * arch_resv_pa,
					virtual_addr_t * arch_resv_va,
					virtual_size_t * arch_resv_sz)
{
	u32 c0_sr;

	if (cpu_boot_pagetable_init(core_resv_pa, core_resv_va, core_resv_sz))
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

int __cpuinit arch_cpu_aspace_secondary_init(void)
{
	/* FIXME: For now nothing to do here. */
	return VMM_OK;
}

pte_t *cpu_va2pte(virtual_addr_t vaddr)
{
	ptab_t *ptab;
	pte_t *pte;
	pgd_t pgd_offset = ((vaddr >> PGD_SHIFT) & PGD_MASK);
	u32 ptab_offset = ((vaddr >> PTAB_SHIFT) & PTAB_MASK);
	ptab = (ptab_t *)host_pgd[pgd_offset];

	BUG_ON(ptab == NULL);

	pte = (pte_t *)ptab[ptab_offset];

	return pte;
}

static void cpu_create_pte(virtual_addr_t vaddr, physical_addr_t paddr,
			       u32 flags)
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

int arch_cpu_aspace_map(virtual_addr_t page_va,
			physical_addr_t page_pa,
			u32 mem_flags)
{
	virtual_size_t sz = VMM_PAGE_SIZE;

	switch (sz) {
	case TLB_PAGE_SIZE_4K:
		cpu_create_pte(page_va, page_pa, mem_flags);
		break;

	case TLB_PAGE_SIZE_1K:
	case TLB_PAGE_SIZE_16K:
	case TLB_PAGE_SIZE_256K:
	case TLB_PAGE_SIZE_1M:
	case TLB_PAGE_SIZE_4M:
	case TLB_PAGE_SIZE_16M:
	case TLB_PAGE_SIZE_64M:
	case TLB_PAGE_SIZE_256M:
	default:
		vmm_panic("%d page size not supported.\n", sz);
	}

	return VMM_OK;
}

int arch_cpu_aspace_unmap(virtual_addr_t page_va)
{
	return VMM_OK;
}

int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t * pa)
{
	pte_t *pte = NULL;
	ptab_t *ptab = NULL;
	u32 ptab_id = ((va >> PTAB_SHIFT) & PTAB_MASK);
	u32 pgd_id = ((va >> PGD_SHIFT) & PGD_MASK);

	if (va < (arch_code_vaddr_start() + arch_code_size()))
		*pa = (va - arch_code_vaddr_start());
	else {
		ptab = (ptab_t *)host_pgd[pgd_id];
		pte = (pte_t *)ptab[ptab_id];

		*pa = pte->paddr;
	}

	return VMM_OK;
}

virtual_addr_t arch_code_vaddr_start(void)
{
	return ((virtual_addr_t) 0xC0000000);
}

physical_addr_t arch_code_paddr_start(void)
{
	return ((physical_addr_t) 0);
}

virtual_size_t cpu_code_base_size(void)
{
	return (virtual_size_t)(&_code_end - &_code_start);
}

virtual_size_t arch_code_size(void)
{
	return VMM_ROUNDUP2_PAGE_SIZE(cpu_code_base_size());
}
