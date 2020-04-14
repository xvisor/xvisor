/**
 * Copyright (c) 2020 Anup Patel.
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
 * @file generic_mmu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of Generic MMU
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <libs/stringlib.h>
#include <libs/radix-tree.h>
#include <arch_sections.h>
#include <arch_barrier.h>

#include <generic_mmu.h>

/*
 * NOTE: we use 1/64th or 1.5625% of VAPOOL memory as translation table pool.
 * For example if VAPOOL is 8 MB and page table size is 4KB then page table
 * pool will be 128 KB or 32 (= 128 KB / 4 KB) page tables.
 */
#define PGTBL_POOL_COUNT 	(CONFIG_VAPOOL_SIZE_MB << \
					(20 - 6 - ARCH_MMU_PGTBL_SIZE_SHIFT))
#define PGTBL_POOL_SIZE		(PGTBL_POOL_COUNT * ARCH_MMU_PGTBL_SIZE)

#define INITIAL_PGTBL_COUNT	ARCH_INITIAL_PGTBL_COUNT
#define INITIAL_PGTBL_SIZE	(INITIAL_PGTBL_COUNT * ARCH_MMU_PGTBL_SIZE)

#define PGTBL_POOL_TOTAL_COUNT	(INITIAL_PGTBL_COUNT + PGTBL_POOL_COUNT)
#define PGTBL_POOL_TOTAL_SIZE	(INITIAL_PGTBL_SIZE + PGTBL_POOL_SIZE)

struct mmu_ctrl {
	struct mmu_pgtbl *hyp_pgtbl;
	virtual_addr_t pgtbl_base_va;
	physical_addr_t pgtbl_base_pa;
	virtual_addr_t ipgtbl_base_va;
	physical_addr_t ipgtbl_base_pa;
	vmm_rwlock_t pgtbl_pool_lock;
	struct mmu_pgtbl pgtbl_pool_array[PGTBL_POOL_COUNT];
	struct mmu_pgtbl ipgtbl_pool_array[INITIAL_PGTBL_COUNT];
	u64 pgtbl_pool_alloc_count;
	struct dlist pgtbl_pool_free_list;
	vmm_rwlock_t pgtbl_nonpool_lock;
	struct dlist pgtbl_nonpool_list;
	struct radix_tree_root pgtbl_nonpool_tree;
};

static struct mmu_ctrl mmuctrl;

u8 __attribute__ ((aligned(1UL << ARCH_MMU_PGTBL_ALIGN_ORDER)))
					def_pgtbl[INITIAL_PGTBL_SIZE] = { 0 };
int def_pgtbl_tree[INITIAL_PGTBL_COUNT];

static struct mmu_pgtbl *mmu_pgtbl_pool_alloc(int stage)
{
	irq_flags_t flags;
	struct dlist *l;
	struct mmu_pgtbl *pgtbl;

	vmm_write_lock_irqsave_lite(&mmuctrl.pgtbl_pool_lock, flags);

	if (list_empty(&mmuctrl.pgtbl_pool_free_list)) {
		vmm_write_unlock_irqrestore_lite(&mmuctrl.pgtbl_pool_lock, flags);
		return NULL;
	}

	l = list_pop(&mmuctrl.pgtbl_pool_free_list);
	pgtbl = list_entry(l, struct mmu_pgtbl, head);
	mmuctrl.pgtbl_pool_alloc_count++;

	vmm_write_unlock_irqrestore_lite(&mmuctrl.pgtbl_pool_lock, flags);

	return pgtbl;
}

static struct mmu_pgtbl *mmu_pgtbl_pool_find(int stage,
					     physical_addr_t tbl_pa)
{
	int index;

	tbl_pa &= ~(ARCH_MMU_PGTBL_SIZE - 1);

	if ((mmuctrl.ipgtbl_base_pa <= tbl_pa) &&
	    (tbl_pa <= (mmuctrl.ipgtbl_base_pa + INITIAL_PGTBL_SIZE))) {
		tbl_pa = tbl_pa - mmuctrl.ipgtbl_base_pa;
		index = tbl_pa >> ARCH_MMU_PGTBL_SIZE_SHIFT;
		if (index < INITIAL_PGTBL_COUNT) {
			return &mmuctrl.ipgtbl_pool_array[index];
		}
	}

	if ((mmuctrl.pgtbl_base_pa <= tbl_pa) &&
	    (tbl_pa <= (mmuctrl.pgtbl_base_pa + PGTBL_POOL_SIZE))) {
		tbl_pa = tbl_pa - mmuctrl.pgtbl_base_pa;
		index = tbl_pa >> ARCH_MMU_PGTBL_SIZE_SHIFT;
		if (index < PGTBL_POOL_COUNT) {
			return &mmuctrl.pgtbl_pool_array[index];
		}
	}

	return NULL;
}

static u64 mmu_pgtbl_pool_count(int stage, int level)
{
	int i;
	u64 count = 0;
	irq_flags_t flags;

	vmm_read_lock_irqsave_lite(&mmuctrl.pgtbl_pool_lock, flags);

	for (i = 0; i < INITIAL_PGTBL_COUNT; i++) {
		if (mmuctrl.ipgtbl_pool_array[i].stage == stage &&
		    mmuctrl.ipgtbl_pool_array[i].level == level)
			count++;
	}

	for (i = 0; i < PGTBL_POOL_COUNT; i++) {
		if (mmuctrl.pgtbl_pool_array[i].stage == stage &&
		    mmuctrl.pgtbl_pool_array[i].level == level)
			count++;
	}

	vmm_read_unlock_irqrestore_lite(&mmuctrl.pgtbl_pool_lock, flags);

	return count;
}

static u64 mmu_pgtbl_pool_alloc_count(void)
{
	u64 count;
	irq_flags_t flags;

	vmm_read_lock_irqsave_lite(&mmuctrl.pgtbl_pool_lock, flags);
	count = mmuctrl.pgtbl_pool_alloc_count;
	vmm_read_unlock_irqrestore_lite(&mmuctrl.pgtbl_pool_lock, flags);

	return count;
}

static void mmu_pgtbl_pool_free(struct mmu_pgtbl *pgtbl)
{
	irq_flags_t flags;

	vmm_write_lock_irqsave_lite(&mmuctrl.pgtbl_pool_lock, flags);
	list_add_tail(&pgtbl->head, &mmuctrl.pgtbl_pool_free_list);
	mmuctrl.pgtbl_pool_alloc_count--;
	vmm_write_unlock_irqrestore_lite(&mmuctrl.pgtbl_pool_lock, flags);
}

struct mmu_pgtbl_nonpool {
	struct dlist head;
	struct mmu_pgtbl pgtbl;
};

static struct mmu_pgtbl *mmu_pgtbl_nonpool_alloc(int stage)
{
	irq_flags_t flags;
	struct mmu_pgtbl *pgtbl;
	struct mmu_pgtbl_nonpool *npgtbl;

	npgtbl = vmm_zalloc(sizeof(*npgtbl));
	if (!npgtbl)
		return NULL;
	INIT_LIST_HEAD(&npgtbl->head);
	pgtbl = &npgtbl->pgtbl;

	pgtbl->tbl_va = vmm_host_alloc_aligned_pages(
					VMM_SIZE_TO_PAGE(ARCH_MMU_PGTBL_SIZE),
					ARCH_MMU_PGTBL_ALIGN_ORDER,
					VMM_MEMORY_FLAGS_NORMAL);
	if (vmm_host_va2pa(pgtbl->tbl_va, &pgtbl->tbl_pa)) {
		vmm_host_free_pages(pgtbl->tbl_va,
				    VMM_SIZE_TO_PAGE(ARCH_MMU_PGTBL_SIZE));
		vmm_free(npgtbl);
		return NULL;
	}

	vmm_write_lock_irqsave_lite(&mmuctrl.pgtbl_nonpool_lock, flags);

	if (radix_tree_insert(&mmuctrl.pgtbl_nonpool_tree,
			      pgtbl->tbl_pa >> ARCH_MMU_PGTBL_ALIGN_ORDER,
			      npgtbl)) {
		vmm_write_unlock_irqrestore_lite(&mmuctrl.pgtbl_nonpool_lock,
						 flags);
		vmm_host_free_pages(pgtbl->tbl_va,
				    VMM_SIZE_TO_PAGE(ARCH_MMU_PGTBL_SIZE));
		vmm_free(pgtbl);
		return NULL;
	}

	list_add_tail(&npgtbl->head, &mmuctrl.pgtbl_nonpool_list);

	vmm_write_unlock_irqrestore_lite(&mmuctrl.pgtbl_nonpool_lock, flags);

	return pgtbl;
}

static struct mmu_pgtbl *mmu_pgtbl_nonpool_find(int stage,
						physical_addr_t tbl_pa)
{
	irq_flags_t flags;
	struct mmu_pgtbl *pgtbl = NULL;
	struct mmu_pgtbl_nonpool *npgtbl;

	vmm_read_lock_irqsave_lite(&mmuctrl.pgtbl_nonpool_lock, flags);

	npgtbl = radix_tree_lookup(&mmuctrl.pgtbl_nonpool_tree,
				   tbl_pa >> ARCH_MMU_PGTBL_ALIGN_ORDER);
	if (npgtbl)
		pgtbl = &npgtbl->pgtbl;

	vmm_read_unlock_irqrestore_lite(&mmuctrl.pgtbl_nonpool_lock, flags);

	return pgtbl;
}

static u64 mmu_pgtbl_nonpool_count(int stage, int level)
{
	u64 count = 0;
	irq_flags_t flags;
	struct mmu_pgtbl_nonpool *npgtbl;

	vmm_read_lock_irqsave_lite(&mmuctrl.pgtbl_nonpool_lock, flags);

	list_for_each_entry(npgtbl, &mmuctrl.pgtbl_nonpool_list, head) {
		if (npgtbl->pgtbl.stage == stage &&
		    npgtbl->pgtbl.level == level)
			count++;
	}

	vmm_read_unlock_irqrestore_lite(&mmuctrl.pgtbl_nonpool_lock, flags);

	return count;
}

static void mmu_pgtbl_nonpool_free(struct mmu_pgtbl *pgtbl)
{
	irq_flags_t flags;
	struct mmu_pgtbl_nonpool *npgtbl =
			container_of(pgtbl, struct mmu_pgtbl_nonpool, pgtbl);

	vmm_write_lock_irqsave_lite(&mmuctrl.pgtbl_nonpool_lock, flags);

	list_del_init(&npgtbl->head);

	radix_tree_delete(&mmuctrl.pgtbl_nonpool_tree,
			  pgtbl->tbl_pa >> ARCH_MMU_PGTBL_ALIGN_ORDER);

	vmm_write_unlock_irqrestore_lite(&mmuctrl.pgtbl_nonpool_lock, flags);

	vmm_host_free_pages(pgtbl->tbl_va,
			    VMM_SIZE_TO_PAGE(ARCH_MMU_PGTBL_SIZE));

	vmm_free(npgtbl);
}

u64 mmu_pgtbl_count(int stage, int level)
{
	if (stage == MMU_STAGE1)
		return mmu_pgtbl_pool_count(stage, level);
	return mmu_pgtbl_nonpool_count(stage, level);
}

struct mmu_pgtbl *mmu_pgtbl_find(int stage, physical_addr_t tbl_pa)
{
	if (stage == MMU_STAGE1)
		return mmu_pgtbl_pool_find(stage, tbl_pa);
	return mmu_pgtbl_nonpool_find(stage, tbl_pa);
}

static inline bool mmu_pgtbl_isattached(struct mmu_pgtbl *child)
{
	return ((child != NULL) && (child->parent != NULL));
}

static int mmu_pgtbl_attach(struct mmu_pgtbl *parent,
				physical_addr_t map_ia,
				struct mmu_pgtbl *child)
{
	int index;
	arch_pte_t *pte;
	irq_flags_t flags;

	if (!parent || !child) {
		return VMM_EFAIL;
	}
	if (mmu_pgtbl_isattached(child)) {
		return VMM_EFAIL;
	}
	if ((parent->level == 0) ||
	    (child->stage != parent->stage)) {
		return VMM_EFAIL;
	}

	index = arch_mmu_level_index(map_ia, parent->stage, parent->level);
	pte = (arch_pte_t *)parent->tbl_va;

	vmm_spin_lock_irqsave_lite(&parent->tbl_lock, flags);

	if (arch_mmu_pte_is_valid(&pte[index], parent->stage, parent->level)) {
		vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);
		return VMM_EFAIL;
	}

	arch_mmu_pte_set_table(&pte[index], parent->stage, parent->level,
				child->tbl_pa);
	arch_mmu_pte_sync(&pte[index], parent->stage, parent->level);

	child->parent = parent;
	child->level = parent->level - 1;
	child->map_ia = map_ia & arch_mmu_level_map_mask(parent->stage,
							 parent->level);
	parent->pte_cnt++;
	parent->child_cnt++;
	list_add(&child->head, &parent->child_list);

	vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);

	return VMM_OK;
}

static int mmu_pgtbl_deattach(struct mmu_pgtbl *child)
{
	int index;
	arch_pte_t *pte;
	irq_flags_t flags;
	struct mmu_pgtbl *parent;

	if (!child || !mmu_pgtbl_isattached(child)) {
		return VMM_EFAIL;
	}

	parent = child->parent;
	index = arch_mmu_level_index(child->map_ia, parent->stage, parent->level);
	pte = (arch_pte_t *)parent->tbl_va;

	vmm_spin_lock_irqsave_lite(&parent->tbl_lock, flags);

	if (!arch_mmu_pte_is_valid(&pte[index], parent->stage, parent->level)) {
		vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);
		return VMM_EFAIL;
	}

	arch_mmu_pte_clear(&pte[index], parent->stage, parent->level);
	arch_mmu_pte_sync(&pte[index], parent->stage, parent->level);

	child->parent = NULL;
	parent->pte_cnt--;
	parent->child_cnt--;
	list_del(&child->head);

	vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);

	return VMM_OK;
}

struct mmu_pgtbl *mmu_pgtbl_alloc(int stage)
{
	struct mmu_pgtbl *pgtbl;

	if (stage == MMU_STAGE1)
		pgtbl = mmu_pgtbl_pool_alloc(stage);
	else
		pgtbl = mmu_pgtbl_nonpool_alloc(stage);
	if (!pgtbl)
		return NULL;

	pgtbl->parent = NULL;
	pgtbl->stage = stage;
	pgtbl->level = arch_mmu_start_level(stage);
	pgtbl->map_ia = 0;
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->pte_cnt = 0;
	pgtbl->child_cnt = 0;
	INIT_LIST_HEAD(&pgtbl->child_list);
	arch_mmu_pgtbl_clear(pgtbl->tbl_va);

	return pgtbl;
}

int mmu_pgtbl_free(struct mmu_pgtbl *pgtbl)
{
	int stage, rc = VMM_OK;
	irq_flags_t flags;
	struct dlist *l;
	struct mmu_pgtbl *child;

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
		child = list_entry(l, struct mmu_pgtbl, head);
		if ((rc = mmu_pgtbl_deattach(child))) {
			return rc;
		}
		if ((rc = mmu_pgtbl_free(child))) {
			return rc;
		}
	}

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);
	pgtbl->pte_cnt = 0;
	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);

	stage = pgtbl->stage;
	pgtbl->stage = 0;
	pgtbl->level = 0;
	pgtbl->map_ia = 0;

	if (stage == MMU_STAGE1)
		mmu_pgtbl_pool_free(pgtbl);
	else
		mmu_pgtbl_nonpool_free(pgtbl);

	return VMM_OK;
}

struct mmu_pgtbl *mmu_pgtbl_get_child(struct mmu_pgtbl *parent,
					  physical_addr_t map_ia,
					  bool create)
{
	int rc, index;
	arch_pte_t *pte, pte_val;
	irq_flags_t flags;
	physical_addr_t tbl_pa;
	struct mmu_pgtbl *child;

	if (!parent) {
		return NULL;
	}

	index = arch_mmu_level_index(map_ia, parent->stage, parent->level);
	pte = (arch_pte_t *)parent->tbl_va;

	vmm_spin_lock_irqsave_lite(&parent->tbl_lock, flags);
	pte_val = pte[index];
	vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);

	if (arch_mmu_pte_is_valid(&pte_val, parent->stage, parent->level)) {
		if ((parent->level > 0) &&
		    arch_mmu_pte_is_table(&pte_val, parent->stage,
					  parent->level)) {
			tbl_pa = arch_mmu_pte_table_addr(&pte_val,
						parent->stage, parent->level);
			child = mmu_pgtbl_find(parent->stage, tbl_pa);
			if (child->parent == parent) {
				return child;
			}
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

int mmu_get_page(struct mmu_pgtbl *pgtbl,
		     physical_addr_t ia, struct mmu_page *pg)
{
	int index;
	arch_pte_t *pte;
	irq_flags_t flags;
	struct mmu_pgtbl *child;

	if (!pgtbl || !pg) {
		return VMM_EFAIL;
	}

	index = arch_mmu_level_index(ia, pgtbl->stage, pgtbl->level);
	pte = (arch_pte_t *)pgtbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);

	if (!arch_mmu_pte_is_valid(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}
	if ((pgtbl->level == 0) &&
	    arch_mmu_pte_is_table(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	if ((pgtbl->level > 0) &&
	    arch_mmu_pte_is_table(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		child = mmu_pgtbl_get_child(pgtbl, ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		return mmu_get_page(child, ia, pg);
	}

	memset(pg, 0, sizeof(struct mmu_page));

	pg->ia = ia & arch_mmu_level_map_mask(pgtbl->stage, pgtbl->level);
	pg->oa = arch_mmu_pte_addr(&pte[index], pgtbl->stage, pgtbl->level);
	pg->sz = arch_mmu_level_block_size(pgtbl->stage, pgtbl->level);
	arch_mmu_pte_flags(&pte[index], pgtbl->stage, pgtbl->level, &pg->flags);

	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);

	return VMM_OK;
}

int mmu_unmap_page(struct mmu_pgtbl *pgtbl, struct mmu_page *pg)
{
	int start_level;
	int index, rc;
	bool free_pgtbl;
	arch_pte_t *pte;
	irq_flags_t flags;
	struct mmu_pgtbl *child;
	physical_size_t blksz;

	if (!pgtbl || !pg) {
		return VMM_EFAIL;
	}
	if (!arch_mmu_valid_block_size(pg->sz)) {
		return VMM_EFAIL;
	}

	blksz = arch_mmu_level_block_size(pgtbl->stage, pgtbl->level);
	if (pg->sz > blksz ) {
		return VMM_EFAIL;
	}

	start_level = arch_mmu_start_level(pgtbl->stage);

	if (pg->sz < blksz) {
		child = mmu_pgtbl_get_child(pgtbl, pg->ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		rc = mmu_unmap_page(child, pg);
		if ((pgtbl->pte_cnt == 0) &&
		    (pgtbl->level < start_level)) {
			mmu_pgtbl_free(pgtbl);
		}
		return rc;
	}

	index = arch_mmu_level_index(pg->ia, pgtbl->stage, pgtbl->level);
	pte = (arch_pte_t *)pgtbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);

	if (!arch_mmu_pte_is_valid(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}
	if ((pgtbl->level == 0) &&
	    arch_mmu_pte_is_table(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	arch_mmu_pte_clear(&pte[index], pgtbl->stage, pgtbl->level);
	arch_mmu_pte_sync(&pte[index], pgtbl->stage, pgtbl->level);

	if (pgtbl->stage == MMU_STAGE1) {
		arch_mmu_stage1_tlbflush((virtual_addr_t)pg->ia,
					 (virtual_size_t)blksz);
	} else {
		arch_mmu_stage2_tlbflush(pg->ia, blksz);
	}

	pgtbl->pte_cnt--;
	free_pgtbl = FALSE;
	if ((pgtbl->pte_cnt == 0) && (pgtbl->level < start_level)) {
		free_pgtbl = TRUE;
	}

	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);

	if (free_pgtbl) {
		mmu_pgtbl_free(pgtbl);
	}

	return VMM_OK;
}

int mmu_map_page(struct mmu_pgtbl *pgtbl, struct mmu_page *pg)
{
	int index;
	arch_pte_t *pte;
	irq_flags_t flags;
	struct mmu_pgtbl *child;
	physical_size_t blksz;

	if (!pgtbl || !pg) {
		return VMM_EFAIL;
	}
	if (!arch_mmu_valid_block_size(pg->sz)) {
		return VMM_EINVALID;
	}

	blksz = arch_mmu_level_block_size(pgtbl->stage, pgtbl->level);
	if (pg->sz > blksz ) {
		return VMM_EFAIL;
	}

	if (pg->sz < blksz) {
		child = mmu_pgtbl_get_child(pgtbl, pg->ia, TRUE);
		if (!child) {
			return VMM_EFAIL;
		}
		return mmu_map_page(child, pg);
	}

	index = arch_mmu_level_index(pg->ia, pgtbl->stage, pgtbl->level);
	pte = (arch_pte_t *)pgtbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);

	if (arch_mmu_pte_is_valid(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	arch_mmu_pte_set(&pte[index], pgtbl->stage, pgtbl->level,
			 pg->oa, &pg->flags);
	arch_mmu_pte_sync(&pte[index], pgtbl->stage, pgtbl->level);

	if (pgtbl->stage == MMU_STAGE1) {
		arch_mmu_stage1_tlbflush((virtual_addr_t)pg->ia,
					 (virtual_size_t)blksz);
	} else {
		arch_mmu_stage2_tlbflush(pg->ia, blksz);
	}

	pgtbl->pte_cnt++;

	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);

	return VMM_OK;
}

int mmu_find_pte(struct mmu_pgtbl *pgtbl, physical_addr_t ia,
		     arch_pte_t **ptep, struct mmu_pgtbl **pgtblp)
{
	int index;
	arch_pte_t *pte;
	irq_flags_t flags;
	struct mmu_pgtbl *child;

	if (!pgtbl || !ptep || !pgtblp) {
		return VMM_EFAIL;
	}

	index = arch_mmu_level_index(ia, pgtbl->stage, pgtbl->level);
	pte = (arch_pte_t *)pgtbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);

	if (!arch_mmu_pte_is_valid(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	if ((pgtbl->level == 0) &&
	    arch_mmu_pte_is_table(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	if ((pgtbl->level > 0) &&
	    arch_mmu_pte_is_table(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		child = mmu_pgtbl_get_child(pgtbl, ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		return mmu_find_pte(child, ia, ptep, pgtblp);
	}

	*ptep = &pte[index];
	*pgtblp = pgtbl;

	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);

	return VMM_OK;
}

int mmu_get_hypervisor_page(virtual_addr_t va, struct mmu_page *pg)
{
	return mmu_get_page(mmuctrl.hyp_pgtbl, va, pg);
}

int mmu_unmap_hypervisor_page(struct mmu_page *pg)
{
	return mmu_unmap_page(mmuctrl.hyp_pgtbl, pg);
}

int mmu_map_hypervisor_page(struct mmu_page *pg)
{
	return mmu_map_page(mmuctrl.hyp_pgtbl, pg);
}

struct mmu_pgtbl *mmu_hypervisor_pgtbl(void)
{
	return mmuctrl.hyp_pgtbl;
}

void arch_cpu_aspace_print_info(struct vmm_chardev *cdev)
{
	u64 count, total;
	int stage, level;

	vmm_cprintf(cdev, "Pool Page Tables\n");
	count = mmu_pgtbl_pool_alloc_count();
	vmm_cprintf(cdev, "    Used  : %"PRIu64"\n", count);
	vmm_cprintf(cdev, "    Free  : %"PRIu64"\n",
		    ((u64)PGTBL_POOL_TOTAL_COUNT - count));
	vmm_cprintf(cdev, "    Total : %"PRIu64"\n",
		    (u64)PGTBL_POOL_TOTAL_COUNT);
	vmm_cprintf(cdev, "    Size  : %"PRIu64" KB\n",
		    (u64)(PGTBL_POOL_TOTAL_SIZE / 1024));
	vmm_cprintf(cdev, "\n");

	for (stage = MMU_STAGE1; stage < MMU_STAGE_MAX; stage++) {
		vmm_cprintf(cdev, "Stage%d Page Tables\n", stage);
		total = 0;
		for (level = arch_mmu_start_level(stage);
		     -1 < level; level--) {
			count = mmu_pgtbl_count(stage, level);
			vmm_cprintf(cdev, "    Level%d : %"PRIu64"\n",
				    level, count);
			total += count;
		}
		vmm_cprintf(cdev, "    Total  : %"PRIu64"\n", total);
		vmm_cprintf(cdev, "\n");
	}
}

u32 arch_cpu_aspace_hugepage_log2size(void)
{
	return arch_mmu_level_block_shift(MMU_STAGE1, 1);
}

int arch_cpu_aspace_map(virtual_addr_t page_va,
			virtual_size_t page_sz,
			physical_addr_t page_pa,
			u32 mem_flags)
{
	struct mmu_page p;

	if (page_sz != arch_mmu_level_block_size(MMU_STAGE1, 0) &&
	    page_sz != arch_mmu_level_block_size(MMU_STAGE1, 1))
		return VMM_EINVALID;

	memset(&p, 0, sizeof(p));
	p.ia = page_va;
	p.oa = page_pa;
	p.sz = page_sz;
	arch_mmu_stage1_pgflags_set(&p.flags, mem_flags);

	return mmu_map_hypervisor_page(&p);
}

int arch_cpu_aspace_unmap(virtual_addr_t page_va)
{
	int rc;
	struct mmu_page p;

	rc = mmu_get_hypervisor_page(page_va, &p);
	if (rc) {
		return rc;
	}

	return mmu_unmap_hypervisor_page(&p);
}

int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t *pa)
{
	int rc = VMM_OK;
	struct mmu_page p;

	if ((rc = mmu_get_hypervisor_page(va, &p))) {
		return rc;
	}

	*pa = p.oa + (va & (p.sz - 1));

	return VMM_OK;
}

virtual_addr_t __init arch_cpu_aspace_vapool_start(void)
{
	return arch_code_vaddr_start();
}

virtual_size_t __init arch_cpu_aspace_vapool_estimate_size(
						physical_size_t total_ram)
{
	return CONFIG_VAPOOL_SIZE_MB << 20;
}

int __init arch_cpu_aspace_primary_init(physical_addr_t *core_resv_pa,
					virtual_addr_t *core_resv_va,
					virtual_size_t *core_resv_sz,
					physical_addr_t *arch_resv_pa,
					virtual_addr_t *arch_resv_va,
					virtual_size_t *arch_resv_sz)
{
	int i, t, rc = VMM_EFAIL;
	virtual_addr_t va, resv_va;
	virtual_size_t sz, resv_sz;
	physical_addr_t pa, resv_pa;
	arch_pte_t *pte;
	struct mmu_page hyppg;
	struct mmu_pgtbl *pgtbl, *parent;
	virtual_addr_t l0_shift = arch_mmu_level_block_shift(MMU_STAGE1, 0);
	virtual_addr_t l0_size = arch_mmu_level_block_size(MMU_STAGE1, 0);

	/* Check constraints of generic MMU */
	if (ARCH_MMU_PGTBL_SIZE != l0_size ||
	    ARCH_MMU_PGTBL_ALIGN_ORDER > l0_shift)
		return VMM_EINVALID;

	/* Initial values of resv_va, resv_pa, and resv_sz */
	pa = arch_code_paddr_start();
	va = arch_code_vaddr_start();
	sz = arch_code_size();
	resv_va = va + sz;
	resv_pa = pa + sz;
	resv_sz = 0;
	if (resv_va & (l0_size - 1)) {
		resv_va += l0_size - (resv_va & (l0_size - 1));
	}
	if (resv_pa & (l0_size - 1)) {
		resv_pa += l0_size - (resv_pa & (l0_size - 1));
	}

	/* Initialize MMU control and allocate arch reserved space and
	 * update the *arch_resv_pa, *arch_resv_va, and *arch_resv_sz
	 * parameters to inform host aspace about the arch reserved space.
	 */
	memset(&mmuctrl, 0, sizeof(mmuctrl));

	*arch_resv_va = (resv_va + resv_sz);
	*arch_resv_pa = (resv_pa + resv_sz);
	*arch_resv_sz = resv_sz;
	mmuctrl.pgtbl_base_va = resv_va + resv_sz;
	mmuctrl.pgtbl_base_pa = resv_pa + resv_sz;
	resv_sz += ARCH_MMU_PGTBL_SIZE * PGTBL_POOL_COUNT;
	if (resv_sz & (l0_size - 1)) {
		resv_sz += l0_size - (resv_sz & (l0_size - 1));
	}
	*arch_resv_sz = resv_sz - *arch_resv_sz;
	mmuctrl.ipgtbl_base_va = (virtual_addr_t)&def_pgtbl;
	mmuctrl.ipgtbl_base_pa = mmuctrl.ipgtbl_base_va -
				arch_code_vaddr_start() +
				arch_code_paddr_start();
	INIT_RW_LOCK(&mmuctrl.pgtbl_pool_lock);
	mmuctrl.pgtbl_pool_alloc_count = 0x0;
	INIT_LIST_HEAD(&mmuctrl.pgtbl_pool_free_list);
	INIT_RW_LOCK(&mmuctrl.pgtbl_nonpool_lock);
	INIT_LIST_HEAD(&mmuctrl.pgtbl_nonpool_list);
	INIT_RADIX_TREE(&mmuctrl.pgtbl_nonpool_tree, 0);
	for (i = 1; i < INITIAL_PGTBL_COUNT; i++) {
		if (def_pgtbl_tree[i] != -1) {
			continue;
		}
		pgtbl = &mmuctrl.ipgtbl_pool_array[i];
		memset(pgtbl, 0, sizeof(struct mmu_pgtbl));
		pgtbl->tbl_pa = mmuctrl.ipgtbl_base_pa + i * ARCH_MMU_PGTBL_SIZE;
		INIT_SPIN_LOCK(&pgtbl->tbl_lock);
		pgtbl->tbl_va = mmuctrl.ipgtbl_base_va + i * ARCH_MMU_PGTBL_SIZE;
		INIT_LIST_HEAD(&pgtbl->head);
		INIT_LIST_HEAD(&pgtbl->child_list);
		list_add_tail(&pgtbl->head, &mmuctrl.pgtbl_pool_free_list);
	}
	for (i = 0; i < PGTBL_POOL_COUNT; i++) {
		pgtbl = &mmuctrl.pgtbl_pool_array[i];
		memset(pgtbl, 0, sizeof(struct mmu_pgtbl));
		pgtbl->tbl_pa = mmuctrl.pgtbl_base_pa + i * ARCH_MMU_PGTBL_SIZE;
		INIT_SPIN_LOCK(&pgtbl->tbl_lock);
		pgtbl->tbl_va = mmuctrl.pgtbl_base_va + i * ARCH_MMU_PGTBL_SIZE;
		INIT_LIST_HEAD(&pgtbl->head);
		INIT_LIST_HEAD(&pgtbl->child_list);
		list_add_tail(&pgtbl->head, &mmuctrl.pgtbl_pool_free_list);
	}

	/* Handcraft hypervisor translation table */
	mmuctrl.hyp_pgtbl = &mmuctrl.ipgtbl_pool_array[0];
	memset(mmuctrl.hyp_pgtbl, 0, sizeof(struct mmu_pgtbl));
	INIT_LIST_HEAD(&mmuctrl.hyp_pgtbl->head);
	mmuctrl.hyp_pgtbl->parent = NULL;
	mmuctrl.hyp_pgtbl->stage = MMU_STAGE1;
	mmuctrl.hyp_pgtbl->level = arch_mmu_start_level(MMU_STAGE1);
	mmuctrl.hyp_pgtbl->map_ia = 0x0;
	mmuctrl.hyp_pgtbl->tbl_pa =  mmuctrl.ipgtbl_base_pa;
	INIT_SPIN_LOCK(&mmuctrl.hyp_pgtbl->tbl_lock);
	mmuctrl.hyp_pgtbl->tbl_va =  mmuctrl.ipgtbl_base_va;
	mmuctrl.hyp_pgtbl->pte_cnt = 0x0;
	mmuctrl.hyp_pgtbl->child_cnt = 0x0;
	INIT_LIST_HEAD(&mmuctrl.hyp_pgtbl->child_list);
	/* Scan table */
	for (t = 0; t < ARCH_MMU_PGTBL_ENTCNT; t++) {
		pte = &((arch_pte_t *)mmuctrl.hyp_pgtbl->tbl_va)[t];
		if (arch_mmu_pte_is_valid(pte, mmuctrl.hyp_pgtbl->stage,
					  mmuctrl.hyp_pgtbl->level)) {
			mmuctrl.hyp_pgtbl->pte_cnt++;
		}
	}
	/* Update MMU control */
	mmuctrl.pgtbl_pool_alloc_count++;
	for (i = 1; i < INITIAL_PGTBL_COUNT; i++) {
		if (def_pgtbl_tree[i] == -1) {
			break;
		}
		pgtbl = &mmuctrl.ipgtbl_pool_array[i];
		parent = &mmuctrl.ipgtbl_pool_array[def_pgtbl_tree[i]];
		memset(pgtbl, 0, sizeof(struct mmu_pgtbl));
		/* Handcraft child tree */
		pgtbl->parent = parent;
		pgtbl->stage = parent->stage;
		pgtbl->level = parent->level - 1;
		pgtbl->tbl_pa = mmuctrl.ipgtbl_base_pa + i * ARCH_MMU_PGTBL_SIZE;
		INIT_SPIN_LOCK(&pgtbl->tbl_lock);
		pgtbl->tbl_va = mmuctrl.ipgtbl_base_va + i * ARCH_MMU_PGTBL_SIZE;
		for (t = 0; t < ARCH_MMU_PGTBL_ENTCNT; t++) {
			pte = &(((arch_pte_t *)parent->tbl_va)[t]);
			if (!arch_mmu_pte_is_valid(pte, parent->stage,
						   parent->level)) {
				continue;
			}
			pa = arch_mmu_pte_table_addr(pte, parent->stage,
						     parent->level);
			if (pa == pgtbl->tbl_pa) {
				pgtbl->map_ia = parent->map_ia;
				pgtbl->map_ia += ((arch_pte_t)t) <<
				arch_mmu_level_index_shift(parent->stage,
							   parent->level);
				break;
			}
		}
		INIT_LIST_HEAD(&pgtbl->head);
		INIT_LIST_HEAD(&pgtbl->child_list);
		/* Scan table enteries */
		for (t = 0; t < ARCH_MMU_PGTBL_ENTCNT; t++) {
			pte = &(((arch_pte_t *)pgtbl->tbl_va)[t]);
			if (arch_mmu_pte_is_valid(pte, pgtbl->stage,
						  pgtbl->level)) {
				pgtbl->pte_cnt++;
			}
		}
		/* Update parent */
		parent->child_cnt++;
		list_add_tail(&pgtbl->head, &parent->child_list);
		/* Update MMU control */
		mmuctrl.pgtbl_pool_alloc_count++;
	}

	/* Check & setup core reserved space and update the
	 * core_resv_pa, core_resv_va, and core_resv_sz parameters
	 * to inform host aspace about correct placement of the
	 * core reserved space.
	 */
	*core_resv_pa = resv_pa + resv_sz;
	*core_resv_va = resv_va + resv_sz;
	if (*core_resv_sz & (l0_size - 1)) {
		*core_resv_sz += l0_size - (*core_resv_sz & (l0_size - 1));
	}
	resv_sz += *core_resv_sz;

	/* Map reserved space (core reserved + arch reserved)
	 * We have kept our page table pool in reserved area pages
	 * as cacheable and write-back. We will clean data cache every
	 * time we modify a page table (or translation table) entry.
	 */
	pa = resv_pa;
	va = resv_va;
	sz = resv_sz;
	while (sz) {
		memset(&hyppg, 0, sizeof(hyppg));
		hyppg.oa = pa;
		hyppg.ia = va;
		hyppg.sz = l0_size;
		arch_mmu_stage1_pgflags_set(&hyppg.flags,
					    VMM_MEMORY_FLAGS_NORMAL);
		if ((rc = mmu_map_hypervisor_page(&hyppg))) {
			goto mmu_init_error;
		}
		sz -= l0_size;
		pa += l0_size;
		va += l0_size;
	}

	/* Clear memory of free translation tables. This cannot be done before
	 * we map reserved space (core reserved + arch reserved).
	 */
	list_for_each_entry(pgtbl, &mmuctrl.pgtbl_pool_free_list, head) {
		arch_mmu_pgtbl_clear(pgtbl->tbl_va);
	}

	return VMM_OK;

mmu_init_error:
	return rc;
}

int __cpuinit arch_cpu_aspace_secondary_init(void)
{
	/* Nothing to do here. */
	return VMM_OK;
}
