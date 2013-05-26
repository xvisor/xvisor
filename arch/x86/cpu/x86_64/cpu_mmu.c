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

/* Note: we use 1/8th or 12.5% of VAPOOL memory as page table pool. 
 * For example if VAPOOL is 8 MB then page table pool will be 1 MB
 * or 1 MB / 4 KB = 256 page tables
 */
#define PGTBL_FIRST_LEVEL		0
#define PGTBL_LAST_LEVEL		3
#define PGTBL_TABLE_SIZE_SHIFT		12
#define PGTBL_TABLE_SIZE		4096
#define PGTBL_TABLE_ENTCNT		512
#define PGTBL_MAX_TABLE_COUNT		(CONFIG_VAPOOL_SIZE_MB << \
					(20 - 3 - PGTBL_TABLE_SIZE_SHIFT))
#define PGTBL_MAX_TABLE_SIZE		(PGTBL_MAX_TABLE_COUNT * \
						PGTBL_TABLE_SIZE)

struct mmu_ctrl {
	struct page_table *hyp_tbl;
	virtual_addr_t pgtbl_base_va;
	physical_addr_t pgtbl_base_pa;
	struct page_table pgtbl_array[PGTBL_MAX_TABLE_COUNT];
	struct page_table ipgtbl_pml4;
	struct page_table ipgtbl_pgdp;
	struct page_table ipgtbl_pgdi;
	struct page_table ipgtbl_pgti;
	vmm_spinlock_t alloc_lock;
	u32 pgtbl_alloc_count;
	struct dlist free_pgtbl_list;
};

static struct mmu_ctrl mctl;

/* initial bootstrap page tables */
extern u64 __pml4[];
extern u64 __pgdp[];
extern u64 __pgdi[];
extern u64 __pgti[];

/* mmu inline asm routines */
static inline void invalidate_vaddr_tlb(virtual_addr_t vaddr)
{
	__asm__ __volatile__("invlpg (%0)\n\t"
			::"r"(vaddr):"memory");
}

static struct page_table *mmu_pgtbl_find(physical_addr_t tbl_pa)
{
	int index;

	tbl_pa &= ~(PGTBL_TABLE_SIZE - 1);

	if (tbl_pa == mctl.ipgtbl_pml4.tbl_pa) {
		return &mctl.ipgtbl_pml4;
	} else if (tbl_pa == mctl.ipgtbl_pgdp.tbl_pa) {
		return &mctl.ipgtbl_pgdp;
	} else if (tbl_pa == mctl.ipgtbl_pgdi.tbl_pa) {
		return &mctl.ipgtbl_pgdi;
	} else if (tbl_pa == mctl.ipgtbl_pgti.tbl_pa) {
		return &mctl.ipgtbl_pgti;
	}

	if ((mctl.pgtbl_base_pa <= tbl_pa) &&
	    (tbl_pa <= (mctl.pgtbl_base_pa + PGTBL_MAX_TABLE_SIZE))) {
		tbl_pa = tbl_pa - mctl.pgtbl_base_pa;
		index = tbl_pa >> PGTBL_TABLE_SIZE_SHIFT;
		if (index < PGTBL_MAX_TABLE_COUNT) {
			return &mctl.pgtbl_array[index];
		}
	}

	return NULL;
}

static inline bool mmu_pgtbl_isattached(struct page_table *child)
{
	return ((child != NULL) && (child->parent != NULL));
}

static inline physical_addr_t mmu_level_map_mask(int level)
{
	switch (level) {
	case 0:
		return PML4_MAP_MASK;
	case 1:
		return PGDP_MAP_MASK;
	case 2:
		return PGDI_MAP_MASK;
	default:
		break;
	};
	return PGTI_MAP_MASK;
}

static inline int mmu_level_index(physical_addr_t ia, int level)
{
	switch (level) {
	case 0:
		return (ia >> PML4_SHIFT) & ~PGTREE_MASK;
	case 1:
		return (ia >> PGDP_SHIFT) & ~PGTREE_MASK;
	case 2:
		return (ia >> PGDI_SHIFT) & ~PGTREE_MASK;
	default:
		break;
	};
	return (ia >> PGTI_SHIFT) & ~PGTREE_MASK;
}

static int mmu_pgtbl_attach(struct page_table *parent,
			    physical_addr_t map_ia, 
			    struct page_table *child)
{
	int index;
	union page *pg;
	irq_flags_t flags;

	if (!parent || !child) {
		return VMM_EFAIL;
	}
	if (mmu_pgtbl_isattached(child)) {
		return VMM_EFAIL;
	}
	if ((parent->level == PGTBL_LAST_LEVEL) || 
	    (child->stage != parent->stage)) {
		return VMM_EFAIL;
	}

	index = mmu_level_index(map_ia, parent->level);
	pg = &((union page *)parent->tbl_va)[index];

	vmm_spin_lock_irqsave(&parent->tbl_lock, flags);

	if (pg->bits.present) {
		vmm_spin_unlock_irqrestore(&parent->tbl_lock, flags);
		return VMM_EFAIL;
	}

	pg->bits.paddr = (child->tbl_pa & PAGE_MASK) >> PAGE_SHIFT;
	pg->bits.present = 1;
	pg->bits.rw = 1;

	/* FIXME: flush cache */

	child->parent = parent;
	child->level = parent->level + 1;
	child->map_ia = map_ia & mmu_level_map_mask(parent->level);
	parent->pte_cnt++;
	parent->child_cnt++;
	list_add(&child->head, &parent->child_list);

	vmm_spin_unlock_irqrestore(&parent->tbl_lock, flags);

	return VMM_OK;
}

static int mmu_pgtbl_deattach(struct page_table *child)
{
	int index;
	union page *pg;
	irq_flags_t flags;
	struct page_table *parent;

	if (!child || !mmu_pgtbl_isattached(child)) {
		return VMM_EFAIL;
	}

	parent = child->parent;
	index = mmu_level_index(child->map_ia, parent->level);
	pg = &((union page *)parent->tbl_va)[index];

	vmm_spin_lock_irqsave(&parent->tbl_lock, flags);

	if (!pg->bits.present) {
		vmm_spin_unlock_irqrestore(&parent->tbl_lock, flags);
		return VMM_EFAIL;
	}

	pg->_val = 0x0;

	/* FIXME: flush cache */

	child->parent = NULL;
	parent->pte_cnt--;
	parent->child_cnt--;
	list_del(&child->head);

	vmm_spin_unlock_irqrestore(&parent->tbl_lock, flags);

	return VMM_OK;
}

struct page_table *mmu_pgtbl_alloc(int stage)
{
	irq_flags_t flags;
	struct dlist *l;
	struct page_table *pgtbl;

	vmm_spin_lock_irqsave(&mctl.alloc_lock, flags);

	if (list_empty(&mctl.free_pgtbl_list)) {
		vmm_spin_unlock_irqrestore(&mctl.alloc_lock, flags);
		return NULL;
	}

	l = list_pop(&mctl.free_pgtbl_list);
	pgtbl = list_entry(l, struct page_table, head);
	mctl.pgtbl_alloc_count++;

	vmm_spin_unlock_irqrestore(&mctl.alloc_lock, flags);

	pgtbl->parent = NULL;
	pgtbl->stage = stage;
	pgtbl->level = PGTBL_FIRST_LEVEL;
	pgtbl->map_ia = 0;
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->pte_cnt = 0;
	pgtbl->child_cnt = 0;
	INIT_LIST_HEAD(&pgtbl->child_list);

	return pgtbl;
}

int mmu_pgtbl_free(struct page_table *pgtbl)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct dlist *l;
	struct page_table *child;

	if (!pgtbl) {
		return VMM_EFAIL;
	}

	if (mmu_pgtbl_isattached(pgtbl)) {
		if ((rc = mmu_pgtbl_deattach(pgtbl))) {
			return rc;
		}
	}

	while (!list_empty(&pgtbl->child_list)) {
		l = list_first(&pgtbl->child_list);
		child = list_entry(l, struct page_table, head);
		if ((rc = mmu_pgtbl_deattach(child))) {
			return rc;
		}
		if ((rc = mmu_pgtbl_free(child))) {
			return rc;
		}
	}

	vmm_spin_lock_irqsave(&pgtbl->tbl_lock, flags);
	pgtbl->pte_cnt = 0;
	memset((void *)pgtbl->tbl_va, 0, PGTBL_TABLE_SIZE);
	vmm_spin_unlock_irqrestore(&pgtbl->tbl_lock, flags);

	pgtbl->level = PGTBL_FIRST_LEVEL;
	pgtbl->map_ia = 0;

	vmm_spin_lock_irqsave(&mctl.alloc_lock, flags);
	list_add_tail(&pgtbl->head, &mctl.free_pgtbl_list);
	mctl.pgtbl_alloc_count--;
	vmm_spin_unlock_irqrestore(&mctl.alloc_lock, flags);

	return VMM_OK;
}

struct page_table *mmu_pgtbl_get_child(struct page_table *parent,
					physical_addr_t map_ia,
					bool create)
{
	int rc, index;
	union page *pg, pgt;
	irq_flags_t flags;
	physical_addr_t tbl_pa;
	struct page_table *child;

	if (!parent) {
		return NULL;
	}

	index = mmu_level_index(map_ia, parent->level);
	pg = &((union page *)parent->tbl_va)[index];

	vmm_spin_lock_irqsave(&parent->tbl_lock, flags);
	pgt._val = pg->_val;
	vmm_spin_unlock_irqrestore(&parent->tbl_lock, flags);

	if (pgt.bits.present) {
		tbl_pa = pgt._val & PAGE_MASK;
		child = mmu_pgtbl_find(tbl_pa);
		if (child->parent == parent) {
			return child;
		}
		return NULL;
	}

	if (!create) {
		return NULL;
	}

	child = mmu_pgtbl_alloc(parent->stage);
	if (!child) {
		return NULL;
	}

	if ((rc = mmu_pgtbl_attach(parent, map_ia, child))) {
		mmu_pgtbl_free(child);
	}

	return child;
}

int mmu_get_page(struct page_table *pgtbl, physical_addr_t ia, union page *pg)
{
	int index;
	irq_flags_t flags;
	union page *pgt;
	struct page_table *child;

	if (!pgtbl || !pg) {
		return VMM_EFAIL;
	}

	index = mmu_level_index(ia, pgtbl->level);
	pgt = &((union page *)pgtbl->tbl_va)[index];

	vmm_spin_lock_irqsave(&pgtbl->tbl_lock, flags);

	if (!pgt->bits.present) {
		vmm_spin_unlock_irqrestore(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	if (pgtbl->level < PGTBL_LAST_LEVEL) {
		vmm_spin_unlock_irqrestore(&pgtbl->tbl_lock, flags);
		child = mmu_pgtbl_get_child(pgtbl, ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		return mmu_get_page(child, ia, pg);
	}

	pg->_val = pgt->_val;

	vmm_spin_unlock_irqrestore(&pgtbl->tbl_lock, flags);

	return VMM_OK;
}

int mmu_unmap_page(struct page_table *pgtbl, physical_addr_t ia)
{
	int index, rc;
	bool free_pgtbl;
	union page *pgt;
	irq_flags_t flags;
	struct page_table *child;

	if (!pgtbl) {
		return VMM_EFAIL;
	}

	if (pgtbl->level < PGTBL_LAST_LEVEL) {
		child = mmu_pgtbl_get_child(pgtbl, ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		rc = mmu_unmap_page(child, ia);
		if ((pgtbl->pte_cnt == 0) && 
		    (pgtbl->level > PGTBL_FIRST_LEVEL)) {
			mmu_pgtbl_free(pgtbl);
		}
		return rc;
	}

	index = mmu_level_index(ia, pgtbl->level);
	pgt = &((union page *)pgtbl->tbl_va)[index];

	vmm_spin_lock_irqsave(&pgtbl->tbl_lock, flags);

	if (!pgt->bits.present) {
		vmm_spin_unlock_irqrestore(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	pgt->_val = 0x0;

	invalidate_vaddr_tlb(ia);

	pgtbl->pte_cnt--;
	free_pgtbl = FALSE;
	if ((pgtbl->pte_cnt == 0) && (pgtbl->level > PGTBL_FIRST_LEVEL)) {
		free_pgtbl = TRUE;
	}

	vmm_spin_unlock_irqrestore(&pgtbl->tbl_lock, flags);

	if (free_pgtbl) {
		mmu_pgtbl_free(pgtbl);
	}

	return VMM_OK;
}

int mmu_map_page(struct page_table *pgtbl, physical_addr_t ia, union page *pg)
{
	int index;
	union page *pgt;
	irq_flags_t flags;
	struct page_table *child;

	if (!pgtbl || !pg) {
		return VMM_EFAIL;
	}

	if (pgtbl->level < PGTBL_LAST_LEVEL) {
		child = mmu_pgtbl_get_child(pgtbl, ia, TRUE);
		if (!child) {
			return VMM_EFAIL;
		}
		return mmu_map_page(child, ia, pg);
	}

	index = mmu_level_index(ia, pgtbl->level);
	pgt = &((union page *)pgtbl->tbl_va)[index];

	vmm_spin_lock_irqsave(&pgtbl->tbl_lock, flags);

	if (pgt->bits.present) {
		vmm_spin_unlock_irqrestore(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	pgt->_val = 0x0;
	pgt->_val = pg->_val;

	/* FIXME: flush cache */

	pgtbl->pte_cnt++;

	vmm_spin_unlock_irqrestore(&pgtbl->tbl_lock, flags);

	return VMM_OK;
}

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

	return mmu_map_page(mctl.hyp_tbl, page_va, &pg);
}

int arch_cpu_aspace_unmap(virtual_addr_t page_va)
{
	return mmu_unmap_page(mctl.hyp_tbl, page_va);
}

int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t *pa)
{
	int rc;
	union page pg;
	u64 fpa;

	rc = mmu_get_page(mctl.hyp_tbl, va, &pg);
	if (rc) {
		return rc;
	}

	fpa = (pg.bits.paddr << PAGE_SHIFT);
	fpa |= va & ~PAGE_MASK;

	*pa = fpa;

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
	struct dlist *l;
	union page *pg;
	union page hyppg;
	struct page_table *pgtbl;

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
	memset(&mctl, 0, sizeof(mctl));
	*arch_resv_va = (resv_va + resv_sz);
	*arch_resv_pa = (resv_pa + resv_sz);
	*arch_resv_sz = resv_sz;
	mctl.pgtbl_base_va = resv_va + resv_sz;
	mctl.pgtbl_base_pa = resv_pa + resv_sz;
	resv_sz += PGTBL_TABLE_SIZE * PGTBL_MAX_TABLE_COUNT;
	*arch_resv_sz = resv_sz - *arch_resv_sz;
	INIT_SPIN_LOCK(&mctl.alloc_lock);
	mctl.pgtbl_alloc_count = 0x0;
	INIT_LIST_HEAD(&mctl.free_pgtbl_list);
	for (i = 0; i < PGTBL_MAX_TABLE_COUNT; i++) {
		pgtbl = &mctl.pgtbl_array[i];
		memset(pgtbl, 0, sizeof(struct page_table));
		pgtbl->tbl_pa = mctl.pgtbl_base_pa + i * PGTBL_TABLE_SIZE;
		INIT_SPIN_LOCK(&pgtbl->tbl_lock);
		pgtbl->tbl_va = mctl.pgtbl_base_va + i * PGTBL_TABLE_SIZE;
		INIT_LIST_HEAD(&pgtbl->head);
		INIT_LIST_HEAD(&pgtbl->child_list);
		list_add_tail(&pgtbl->head, &mctl.free_pgtbl_list);
	}

	/* Handcraft bootstrap pml4 */
	pgtbl = &mctl.ipgtbl_pml4;
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
	mctl.pgtbl_alloc_count++;
	for (t = 0; t < PGTBL_TABLE_ENTCNT; t++) {
		pg = &((union page *)pgtbl->tbl_va)[t];
		if (pg->bits.present) {
			pgtbl->pte_cnt++;
		}
	}

	/* Handcraft bootstrap pgdp */
	pgtbl = &mctl.ipgtbl_pgdp;
	memset(pgtbl, 0, sizeof(struct page_table));
	pgtbl->level = 1;
	pgtbl->stage = 0;
	pgtbl->parent = &mctl.ipgtbl_pml4;
	pgtbl->map_ia = arch_code_vaddr_start() & mmu_level_map_mask(0);
	pgtbl->tbl_pa = (virtual_addr_t)__pgdp - 
			arch_code_vaddr_start() + 
			arch_code_paddr_start();
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->tbl_va = (virtual_addr_t)__pgdp;
	INIT_LIST_HEAD(&pgtbl->head);
	INIT_LIST_HEAD(&pgtbl->child_list);
	mctl.pgtbl_alloc_count++;
	for (t = 0; t < PGTBL_TABLE_ENTCNT; t++) {
		pg = &((union page *)pgtbl->tbl_va)[t];
		if (pg->bits.present) {
			pgtbl->pte_cnt++;
		}
	}
	list_add_tail(&pgtbl->head, &mctl.ipgtbl_pml4.child_list);
	mctl.ipgtbl_pml4.child_cnt++;

	/* Handcraft bootstrap pgdi */
	pgtbl = &mctl.ipgtbl_pgdi;
	memset(pgtbl, 0, sizeof(struct page_table));
	pgtbl->level = 2;
	pgtbl->stage = 0;
	pgtbl->parent = &mctl.ipgtbl_pgdp;
	pgtbl->map_ia = arch_code_vaddr_start() & mmu_level_map_mask(1);
	pgtbl->tbl_pa = (virtual_addr_t)__pgdi - 
			arch_code_vaddr_start() + 
			arch_code_paddr_start();
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->tbl_va = (virtual_addr_t)__pgdi;
	INIT_LIST_HEAD(&pgtbl->head);
	INIT_LIST_HEAD(&pgtbl->child_list);
	mctl.pgtbl_alloc_count++;
	for (t = 0; t < PGTBL_TABLE_ENTCNT; t++) {
		pg = &((union page *)pgtbl->tbl_va)[t];
		if (pg->bits.present) {
			pgtbl->pte_cnt++;
		}
	}
	list_add_tail(&pgtbl->head, &mctl.ipgtbl_pgdp.child_list);
	mctl.ipgtbl_pgdp.child_cnt++;

	/* Handcraft bootstrap pgti */
	pgtbl = &mctl.ipgtbl_pgti;
	memset(pgtbl, 0, sizeof(struct page_table));
	pgtbl->level = 3;
	pgtbl->stage = 0;
	pgtbl->parent = &mctl.ipgtbl_pgdi;
	pgtbl->map_ia = arch_code_vaddr_start() & mmu_level_map_mask(2);
	pgtbl->tbl_pa = (virtual_addr_t)__pgti - 
			arch_code_vaddr_start() + 
			arch_code_paddr_start();
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->tbl_va = (virtual_addr_t)__pgti;
	INIT_LIST_HEAD(&pgtbl->head);
	INIT_LIST_HEAD(&pgtbl->child_list);
	mctl.pgtbl_alloc_count++;
	for (t = 0; t < PGTBL_TABLE_ENTCNT; t++) {
		pg = &((union page *)pgtbl->tbl_va)[t];
		if (pg->bits.present) {
			pgtbl->pte_cnt++;
		}
	}
	list_add_tail(&pgtbl->head, &mctl.ipgtbl_pgdi.child_list);
	mctl.ipgtbl_pgdi.child_cnt++;

	/* Point hypervisor table to bootstrap pml4 */
	mctl.hyp_tbl = &mctl.ipgtbl_pml4;

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
		if ((rc = mmu_map_page(mctl.hyp_tbl, va, &hyppg))) {
			goto mmu_init_error;
		}
		sz -= PAGE_SIZE;
		pa += PAGE_SIZE;
		va += PAGE_SIZE;
	}

	/* Clear memory of free translation tables. This cannot be done before
	 * we map reserved space (core reserved + arch reserved).
	 */
	list_for_each(l, &mctl.free_pgtbl_list) {
		pgtbl = list_entry(l, struct page_table, head);
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

