/**
 * Copyright (c) 2013 Himanshu Chauhan.
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
 * @file cpu_mmu.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Memory management code.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <arch_sections.h>
#include <arch_cpu.h>
#include <libs/stringlib.h>
#include <cpu_mmu.h>
#include <cpu_pgtbl_helper.h>

#define HOST_PGTBL_MAX_TABLE_COUNT		(CONFIG_VAPOOL_SIZE_MB << \
						 (20 - 3 - PGTBL_TABLE_SIZE_SHIFT))
#define HOST_PGTBL_MAX_TABLE_SIZE		(HOST_PGTBL_MAX_TABLE_COUNT * \
						 PGTBL_TABLE_SIZE)

unsigned long __force_order;

struct pgtbl_ctrl host_pgtbl_ctl;

/* initial bootstrap page tables */
extern u64 __pml4[];
extern u64 __pgdp[];
extern u64 __pgdi[];
extern u64 __pgti[];

struct page_table host_pgtbl_array[HOST_PGTBL_MAX_TABLE_COUNT];

static void arch_preinit_pgtable_entries(void)
{
	int i;
	char *pgdp_base, *pgdi_base, *pgti_base;

	pgdp_base = (char *)__pgdp;
	pgdi_base = (char *)__pgdi;
	pgti_base = (char *)__pgti;

	for (i = 0; i < NR_PGDP_PAGES; i++)
		__pml4[i] = ((u64)(pgdp_base + (PAGE_SIZE * i)) & PAGE_MASK) + 3;

	for (i = 0; i < NR_PGDI_PAGES; i++)
		__pgdp[i] = ((u64)(pgdi_base + (PAGE_SIZE * i)) & PAGE_MASK) + 3;

	for (i = 0; i < NR_PGTI_PAGES; i++)
		__pgdi[i] = ((u64)(pgti_base + (PAGE_SIZE * i)) & PAGE_MASK) + 3;
}

/* mmu inline asm routines */
int arch_cpu_aspace_map(virtual_addr_t page_va,
			physical_addr_t page_pa,
			u32 mem_flags)
{
	union page pg;

	/* FIXME: more specific page attributes */
	pg._val = 0x0;
	pg.bits.paddr = (page_pa >> PAGE_SHIFT);
	pg.bits.present = 1;
	pg.bits.rw = 1;

	return mmu_map_page(&host_pgtbl_ctl, host_pgtbl_ctl.base_pgtbl, page_va, &pg);
}

int arch_cpu_aspace_unmap(virtual_addr_t page_va)
{
	return mmu_unmap_page(&host_pgtbl_ctl, host_pgtbl_ctl.base_pgtbl, page_va);
}

int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t *pa)
{
	int rc;
	union page pg;
	u64 fpa;

	rc = mmu_get_page(&host_pgtbl_ctl, host_pgtbl_ctl.base_pgtbl, va, &pg);
	if (rc) {
		return rc;
	}

	fpa = (pg.bits.paddr << PAGE_SHIFT);
	fpa |= va & ~PAGE_MASK;

	*pa = fpa;

	return VMM_OK;
}

int arch_cpu_aspace_pa2va(physical_addr_t pa,
			  virtual_size_t sz,
			  u32 mem_flags,
			  virtual_addr_t *va)
{
	/* x86 has identical mapping */
	*va = pa;

	return VMM_OK;
}

int __init arch_cpu_aspace_primary_init(physical_addr_t *core_resv_pa,
					virtual_addr_t *core_resv_va,
					virtual_size_t *core_resv_sz,
					physical_addr_t *arch_resv_pa,
					virtual_addr_t *arch_resv_va,
					virtual_size_t *arch_resv_sz)
{
	int i, t, rc = VMM_EFAIL;
	virtual_addr_t va, resv_va = *core_resv_va;
	virtual_size_t sz, resv_sz = *core_resv_sz;
	physical_addr_t pa, resv_pa = *core_resv_pa;
	union page *pg;
	union page hyppg;
	struct page_table *pgtbl;

	/*
	 * Boot code didn't populate all the entries in the
	 * page tables. Initialize all of them now so that
	 * later we only have to handle PTE mappings. This
	 * means that the code can map uptil PTEs for all
	 * code and vapool addresses.
	 */
	arch_preinit_pgtable_entries();

	/* Check & setup core reserved space and update the 
	 * core_resv_pa, core_resv_va, and core_resv_sz parameters
	 * to inform host aspace about correct placement of the
	 * core reserved space.
	 */
	pa = arch_code_paddr_start();
	va = arch_code_vaddr_start();
	sz = arch_code_size();
	resv_va = va + sz;
	resv_pa = pa + sz;
	if (resv_va & (PAGE_SIZE - 1)) {
		resv_va += PAGE_SIZE - (resv_va & (PAGE_SIZE - 1));
	}
	if (resv_pa & (PAGE_SIZE - 1)) {
		resv_pa += PAGE_SIZE - (resv_pa & (PAGE_SIZE - 1));
	}
	if (resv_sz & (PAGE_SIZE - 1)) {
		resv_sz += PAGE_SIZE - (resv_sz & (PAGE_SIZE - 1));
	}
	*core_resv_pa = resv_pa;
	*core_resv_va = resv_va;
	*core_resv_sz = resv_sz;

	/* Initialize MMU control and allocate arch reserved space and 
	 * update the *arch_resv_pa, *arch_resv_va, and *arch_resv_sz 
	 * parameters to inform host aspace about the arch reserved space.
	 */
	memset(&host_pgtbl_ctl, 0, sizeof(host_pgtbl_ctl));
	memset(host_pgtbl_array, 0, sizeof(host_pgtbl_array));
	host_pgtbl_ctl.pgtbl_array = &host_pgtbl_array[0];
	host_pgtbl_ctl.pgtbl_max_size = HOST_PGTBL_MAX_TABLE_SIZE;
	host_pgtbl_ctl.pgtbl_max_count = HOST_PGTBL_MAX_TABLE_COUNT;
	*arch_resv_va = (resv_va + resv_sz);
	*arch_resv_pa = (resv_pa + resv_sz);
	*arch_resv_sz = resv_sz;
	host_pgtbl_ctl.pgtbl_base_va = resv_va + resv_sz;
	host_pgtbl_ctl.pgtbl_base_pa = resv_pa + resv_sz;
	resv_sz += PGTBL_TABLE_SIZE * HOST_PGTBL_MAX_TABLE_COUNT;
	*arch_resv_sz = resv_sz - *arch_resv_sz;
	INIT_SPIN_LOCK(&host_pgtbl_ctl.alloc_lock);
	host_pgtbl_ctl.pgtbl_alloc_count = 0x0;
	INIT_LIST_HEAD(&host_pgtbl_ctl.free_pgtbl_list);
	for (i = 0; i < HOST_PGTBL_MAX_TABLE_COUNT; i++) {
		pgtbl = &host_pgtbl_ctl.pgtbl_array[i];
		memset(pgtbl, 0, sizeof(struct page_table));
		pgtbl->tbl_pa = host_pgtbl_ctl.pgtbl_base_pa + i * PGTBL_TABLE_SIZE;
		INIT_SPIN_LOCK(&pgtbl->tbl_lock);
		pgtbl->tbl_va = host_pgtbl_ctl.pgtbl_base_va + i * PGTBL_TABLE_SIZE;
		INIT_LIST_HEAD(&pgtbl->head);
		INIT_LIST_HEAD(&pgtbl->child_list);
		list_add_tail(&pgtbl->head, &host_pgtbl_ctl.free_pgtbl_list);
	}

	/* Handcraft bootstrap pml4 */
	pgtbl = &host_pgtbl_ctl.pgtbl_pml4;
	memset(pgtbl, 0, sizeof(struct page_table));
	pgtbl->level = 0;
	pgtbl->stage = 0;
	pgtbl->parent = NULL;
	pgtbl->map_ia = 0;
	pgtbl->tbl_pa = (virtual_addr_t)__pml4 - 
			arch_code_vaddr_start() + 
			arch_code_paddr_start();
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->tbl_va = (virtual_addr_t)__pml4;
	INIT_LIST_HEAD(&pgtbl->head);
	INIT_LIST_HEAD(&pgtbl->child_list);
	host_pgtbl_ctl.pgtbl_alloc_count++;
	for (t = 0; t < PGTBL_TABLE_ENTCNT; t++) {
		pg = &((union page *)pgtbl->tbl_va)[t];
		if (pg->bits.present) {
			pgtbl->pte_cnt++;
		}
	}

	/* Handcraft bootstrap pgdp */
	pgtbl = &host_pgtbl_ctl.pgtbl_pgdp;
	memset(pgtbl, 0, sizeof(struct page_table));
	pgtbl->level = 1;
	pgtbl->stage = 0;
	pgtbl->parent = &host_pgtbl_ctl.pgtbl_pml4;
	pgtbl->map_ia = arch_code_vaddr_start() & mmu_level_map_mask(0);
	pgtbl->tbl_pa = (virtual_addr_t)__pgdp - 
			arch_code_vaddr_start() + 
			arch_code_paddr_start();
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->tbl_va = (virtual_addr_t)__pgdp;
	INIT_LIST_HEAD(&pgtbl->head);
	INIT_LIST_HEAD(&pgtbl->child_list);
	host_pgtbl_ctl.pgtbl_alloc_count++;
	for (t = 0; t < PGTBL_TABLE_ENTCNT; t++) {
		pg = &((union page *)pgtbl->tbl_va)[t];
		if (pg->bits.present) {
			pgtbl->pte_cnt++;
		}
	}
	list_add_tail(&pgtbl->head, &host_pgtbl_ctl.pgtbl_pml4.child_list);
	host_pgtbl_ctl.pgtbl_pml4.child_cnt++;

	/* Handcraft bootstrap pgdi */
	pgtbl = &host_pgtbl_ctl.pgtbl_pgdi;
	memset(pgtbl, 0, sizeof(struct page_table));
	pgtbl->level = 2;
	pgtbl->stage = 0;
	pgtbl->parent = &host_pgtbl_ctl.pgtbl_pgdp;
	pgtbl->map_ia = arch_code_vaddr_start() & mmu_level_map_mask(1);
	pgtbl->tbl_pa = (virtual_addr_t)__pgdi - 
			arch_code_vaddr_start() + 
			arch_code_paddr_start();
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->tbl_va = (virtual_addr_t)__pgdi;
	INIT_LIST_HEAD(&pgtbl->head);
	INIT_LIST_HEAD(&pgtbl->child_list);
	host_pgtbl_ctl.pgtbl_alloc_count++;
	for (t = 0; t < PGTBL_TABLE_ENTCNT; t++) {
		pg = &((union page *)pgtbl->tbl_va)[t];
		if (pg->bits.present) {
			pgtbl->pte_cnt++;
		}
	}
	list_add_tail(&pgtbl->head, &host_pgtbl_ctl.pgtbl_pgdp.child_list);
	host_pgtbl_ctl.pgtbl_pgdp.child_cnt++;

	/* Handcraft bootstrap pgti */
	pgtbl = &host_pgtbl_ctl.pgtbl_pgti;
	memset(pgtbl, 0, sizeof(struct page_table));
	pgtbl->level = 3;
	pgtbl->stage = 0;
	pgtbl->parent = &host_pgtbl_ctl.pgtbl_pgdi;
	pgtbl->map_ia = arch_code_vaddr_start() & mmu_level_map_mask(2);
	pgtbl->tbl_pa = (virtual_addr_t)__pgti - 
			arch_code_vaddr_start() + 
			arch_code_paddr_start();
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->tbl_va = (virtual_addr_t)__pgti;
	INIT_LIST_HEAD(&pgtbl->head);
	INIT_LIST_HEAD(&pgtbl->child_list);
	host_pgtbl_ctl.pgtbl_alloc_count++;
	for (t = 0; t < PGTBL_TABLE_ENTCNT; t++) {
		pg = &((union page *)pgtbl->tbl_va)[t];
		if (pg->bits.present) {
			pgtbl->pte_cnt++;
		}
	}
	list_add_tail(&pgtbl->head, &host_pgtbl_ctl.pgtbl_pgdi.child_list);
	host_pgtbl_ctl.pgtbl_pgdi.child_cnt++;

	/* Point hypervisor table to bootstrap pml4 */
	host_pgtbl_ctl.base_pgtbl = &host_pgtbl_ctl.pgtbl_pml4;

	/* Map reserved space (core reserved + arch reserved)
	 * We have kept our page table pool in reserved area pages
	 * as cacheable and write-back. We will clean data cache every
	 * time we modify a page table (or translation table) entry.
	 */
	pa = resv_pa;
	va = resv_va;
	sz = resv_sz;
	while (sz) {
		/* FIXME: more specific page attributes */
		hyppg._val = 0x0;
		hyppg.bits.paddr = (pa >> PAGE_SHIFT);
		hyppg.bits.present = 1;
		hyppg.bits.rw = 1;		
		if ((rc = mmu_map_page(&host_pgtbl_ctl, host_pgtbl_ctl.base_pgtbl, va, &hyppg))) {
			goto mmu_init_error;
		}
		sz -= PAGE_SIZE;
		pa += PAGE_SIZE;
		va += PAGE_SIZE;
	}

	/* Clear memory of free translation tables. This cannot be done before
	 * we map reserved space (core reserved + arch reserved).
	 */
	list_for_each_entry(pgtbl, &host_pgtbl_ctl.free_pgtbl_list, head) {
		memset((void *)pgtbl->tbl_va, 0, PGTBL_TABLE_SIZE);
	}

	return VMM_OK;

mmu_init_error:
	return rc;
}

int __cpuinit arch_cpu_aspace_secondary_init(void)
{
	/* FIXME: For now nothing to do here. */
	return VMM_OK;
}

