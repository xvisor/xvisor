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
#include <vmm_compiler.h>
#include <vmm_types.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <libs/stringlib.h>
#include <libs/radix-tree.h>
#include <arch_config.h>
#include <arch_sections.h>
#include <arch_barrier.h>

#include <generic_mmu.h>

#define STAGE1_ROOT_ORDER	ARCH_MMU_STAGE1_ROOT_SIZE_ORDER
#define STAGE1_ROOT_SIZE	(1UL << STAGE1_ROOT_ORDER)
#define STAGE1_ROOT_ALIGN_ORDER ARCH_MMU_STAGE1_ROOT_ALIGN_ORDER
#define STAGE1_ROOT_ALIGN	(1UL << STAGE1_ROOT_ALIGN_ORDER)

#define STAGE1_NONROOT_ORDER	ARCH_MMU_STAGE1_NONROOT_SIZE_ORDER
#define STAGE1_NONROOT_SIZE	(1UL << STAGE1_NONROOT_ORDER)
#define STAGE1_NONROOT_ALIGN_ORDER ARCH_MMU_STAGE1_NONROOT_ALIGN_ORDER
#define STAGE1_NONROOT_ALIGN	(1UL << STAGE1_NONROOT_ALIGN_ORDER)

/*
 * NOTE: we use 1/64th or 1.5625% of VAPOOL memory as translation table pool.
 * For example if VAPOOL is 8 MB and page table size is 4KB then page table
 * pool will be 128 KB or 32 (= 128 KB / 4 KB) page tables.
 */
#define PGTBL_POOL_COUNT 	(CONFIG_VAPOOL_SIZE_MB << \
				 (20 - 6 - ARCH_MMU_STAGE1_NONROOT_SIZE_ORDER))
#define PGTBL_POOL_SIZE		(PGTBL_POOL_COUNT * STAGE1_NONROOT_SIZE)

#define INIT_PGTBL_COUNT	ARCH_MMU_STAGE1_NONROOT_INITIAL_COUNT
#define INIT_PGTBL_SIZE		(INIT_PGTBL_COUNT * STAGE1_NONROOT_SIZE)

#define PGTBL_POOL_TOTAL_COUNT	(INIT_PGTBL_COUNT + PGTBL_POOL_COUNT)
#define PGTBL_POOL_TOTAL_SIZE	(INIT_PGTBL_SIZE + PGTBL_POOL_SIZE)

struct mmu_ctrl {
	struct mmu_pgtbl hyp_pgtbl;
	virtual_addr_t pgtbl_base_va;
	physical_addr_t pgtbl_base_pa;
	virtual_addr_t ipgtbl_base_va;
	physical_addr_t ipgtbl_base_pa;
	vmm_rwlock_t pgtbl_pool_lock;
	struct mmu_pgtbl pgtbl_pool_array[PGTBL_POOL_COUNT];
	struct mmu_pgtbl ipgtbl_pool_array[INIT_PGTBL_COUNT];
	u64 pgtbl_pool_alloc_count;
	struct dlist pgtbl_pool_free_list;
	vmm_rwlock_t pgtbl_nonpool_lock;
	struct dlist pgtbl_nonpool_list;
	struct radix_tree_root pgtbl_nonpool_tree;
};

static struct mmu_ctrl mmuctrl;

u8 __aligned(STAGE1_ROOT_ALIGN) stage1_pgtbl_root[STAGE1_ROOT_SIZE] = { 0 };
u8 __aligned(STAGE1_NONROOT_ALIGN) stage1_pgtbl_nonroot[INIT_PGTBL_SIZE] = { 0 };

static struct mmu_pgtbl *mmu_pgtbl_pool_alloc(int stage, int level)
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

	tbl_pa &= ~(STAGE1_NONROOT_SIZE - 1);

	if ((mmuctrl.ipgtbl_base_pa <= tbl_pa) &&
	    (tbl_pa <= (mmuctrl.ipgtbl_base_pa + INIT_PGTBL_SIZE))) {
		tbl_pa = tbl_pa - mmuctrl.ipgtbl_base_pa;
		index = tbl_pa >> ARCH_MMU_STAGE1_NONROOT_SIZE_ORDER;
		if (index < INIT_PGTBL_COUNT) {
			return &mmuctrl.ipgtbl_pool_array[index];
		}
	}

	if ((mmuctrl.pgtbl_base_pa <= tbl_pa) &&
	    (tbl_pa <= (mmuctrl.pgtbl_base_pa + PGTBL_POOL_SIZE))) {
		tbl_pa = tbl_pa - mmuctrl.pgtbl_base_pa;
		index = tbl_pa >> ARCH_MMU_STAGE1_NONROOT_SIZE_ORDER;
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

	for (i = 0; i < INIT_PGTBL_COUNT; i++) {
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

static void mmu_pgtbl_pool_free(int stage, struct mmu_pgtbl *pgtbl)
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

static struct mmu_pgtbl *mmu_pgtbl_nonpool_alloc(int stage, int level)
{
	irq_flags_t flags;
	struct mmu_pgtbl *pgtbl;
	struct mmu_pgtbl_nonpool *npgtbl;

	npgtbl = vmm_zalloc(sizeof(*npgtbl));
	if (!npgtbl)
		return NULL;
	INIT_LIST_HEAD(&npgtbl->head);
	pgtbl = &npgtbl->pgtbl;

	pgtbl->tbl_sz = 1UL << arch_mmu_pgtbl_size_order(stage, level);
	pgtbl->tbl_va = vmm_host_alloc_aligned_pages(
				VMM_SIZE_TO_PAGE(pgtbl->tbl_sz),
				arch_mmu_pgtbl_align_order(stage, level),
				VMM_MEMORY_FLAGS_NORMAL);
	if (vmm_host_va2pa(pgtbl->tbl_va, &pgtbl->tbl_pa)) {
		vmm_host_free_pages(pgtbl->tbl_va,
				    VMM_SIZE_TO_PAGE(pgtbl->tbl_sz));
		vmm_free(npgtbl);
		return NULL;
	}

	vmm_write_lock_irqsave_lite(&mmuctrl.pgtbl_nonpool_lock, flags);

	if (radix_tree_insert(&mmuctrl.pgtbl_nonpool_tree,
			pgtbl->tbl_pa >> arch_mmu_pgtbl_min_align_order(stage),
			npgtbl)) {
		vmm_write_unlock_irqrestore_lite(&mmuctrl.pgtbl_nonpool_lock,
						 flags);
		vmm_host_free_pages(pgtbl->tbl_va,
				    VMM_SIZE_TO_PAGE(pgtbl->tbl_sz));
		vmm_free(npgtbl);
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
			tbl_pa >> arch_mmu_pgtbl_min_align_order(stage));
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

static void mmu_pgtbl_nonpool_free(int stage, struct mmu_pgtbl *pgtbl)
{
	irq_flags_t flags;
	struct mmu_pgtbl_nonpool *npgtbl =
			container_of(pgtbl, struct mmu_pgtbl_nonpool, pgtbl);

	vmm_write_lock_irqsave_lite(&mmuctrl.pgtbl_nonpool_lock, flags);

	list_del_init(&npgtbl->head);

	radix_tree_delete(&mmuctrl.pgtbl_nonpool_tree,
		pgtbl->tbl_pa >> arch_mmu_pgtbl_min_align_order(stage));

	vmm_write_unlock_irqrestore_lite(&mmuctrl.pgtbl_nonpool_lock, flags);

	vmm_host_free_pages(pgtbl->tbl_va,
			    VMM_SIZE_TO_PAGE(pgtbl->tbl_sz));

	vmm_free(npgtbl);
}

u64 mmu_pgtbl_count(int stage, int level)
{
	if (stage == MMU_STAGE1) {
		return mmu_pgtbl_pool_count(stage, level) +
			((level == arch_mmu_start_level(stage)) ? 1 : 0);
	}
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
		return VMM_EEXIST;
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

struct mmu_pgtbl *mmu_pgtbl_alloc(int stage, int level, u32 attr, u32 hw_tag)
{
	struct mmu_pgtbl *pgtbl;

	if (stage <= MMU_STAGE_UNKNOWN || MMU_STAGE_MAX <= stage)
		return NULL;

	if (level < 0)
		level = arch_mmu_start_level(stage);

	if (stage == MMU_STAGE1)
		pgtbl = mmu_pgtbl_pool_alloc(stage, level);
	else
		pgtbl = mmu_pgtbl_nonpool_alloc(stage, level);
	if (!pgtbl)
		return NULL;

	pgtbl->parent = NULL;
	pgtbl->stage = stage;
	pgtbl->level = level;
	pgtbl->attr = attr;
	pgtbl->hw_tag = hw_tag;
	pgtbl->map_ia = 0;
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->pte_cnt = 0;
	pgtbl->child_cnt = 0;
	INIT_LIST_HEAD(&pgtbl->child_list);
	memset((void *)pgtbl->tbl_va, 0, pgtbl->tbl_sz);

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
	pgtbl->attr = 0;
	pgtbl->hw_tag = 0;
	pgtbl->map_ia = 0;

	if (stage == MMU_STAGE1)
		mmu_pgtbl_pool_free(stage, pgtbl);
	else
		mmu_pgtbl_nonpool_free(stage, pgtbl);

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

	if (!parent || !parent->level) {
		return NULL;
	}

	index = arch_mmu_level_index(map_ia, parent->stage, parent->level);
	pte = (arch_pte_t *)parent->tbl_va;

	vmm_spin_lock_irqsave_lite(&parent->tbl_lock, flags);
	pte_val = pte[index];
	vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);

	if (arch_mmu_pte_is_valid(&pte_val, parent->stage, parent->level)) {
		child = NULL;
		if ((parent->level > 0) &&
		    arch_mmu_pte_is_table(&pte_val, parent->stage,
					  parent->level)) {
			tbl_pa = arch_mmu_pte_table_addr(&pte_val,
						parent->stage, parent->level);
			child = mmu_pgtbl_find(parent->stage, tbl_pa);
			if (!child || child->parent != parent) {
				vmm_printf("%s: invalid child for address "
					   "0x%"PRIPADDR" in page table at "
					   "0x%"PRIPADDR" stage=%d level=%d\n"
					   , __func__, map_ia, parent->tbl_pa,
					   parent->stage, parent->level);
				child = NULL;
			}
		}

		return child;
	}

	if (!create) {
		return NULL;
	}

	child = mmu_pgtbl_alloc(parent->stage, parent->level - 1,
				parent->attr, parent->hw_tag);
	if (!child) {
		vmm_printf("%s: failed to alloc child for address "
			   "0x%"PRIPADDR" in page table at "
			   "0x%"PRIPADDR" stage=%d level=%d\n",
			   __func__, map_ia, parent->tbl_pa,
			   parent->stage, parent->level);
		return NULL;
	}

	if ((rc = mmu_pgtbl_attach(parent, map_ia, child))) {
		if (rc != VMM_EEXIST) {
			vmm_printf("%s: failed to attach child for address "
				   "0x%"PRIPADDR" in page table at "
				   "0x%"PRIPADDR" stage=%d level=%d\n",
				   __func__, map_ia, parent->tbl_pa,
				   parent->stage, parent->level);
		}
		mmu_pgtbl_free(child);
		child = NULL;
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
		arch_mmu_stage1_tlbflush(
				mmu_pgtbl_need_remote_tlbflush(pgtbl),
				mmu_pgtbl_has_hw_tag(pgtbl),
				mmu_pgtbl_hw_tag(pgtbl),
				pg->ia, blksz);
	} else {
		arch_mmu_stage2_tlbflush(
				mmu_pgtbl_need_remote_tlbflush(pgtbl),
				mmu_pgtbl_has_hw_tag(pgtbl),
				mmu_pgtbl_hw_tag(pgtbl),
				pg->ia, blksz);
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
		arch_mmu_stage1_tlbflush(
				mmu_pgtbl_need_remote_tlbflush(pgtbl),
				mmu_pgtbl_has_hw_tag(pgtbl),
				mmu_pgtbl_hw_tag(pgtbl),
				pg->ia, blksz);
	} else {
		arch_mmu_stage2_tlbflush(
				mmu_pgtbl_need_remote_tlbflush(pgtbl),
				mmu_pgtbl_has_hw_tag(pgtbl),
				mmu_pgtbl_hw_tag(pgtbl),
				pg->ia, blksz);
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
	physical_size_t map_last;
	struct mmu_pgtbl *child;

	if (!pgtbl || !ptep || !pgtblp) {
		return VMM_EFAIL;
	}

	map_last = arch_mmu_level_block_size(pgtbl->stage, pgtbl->level);
	map_last *= (pgtbl->tbl_sz / sizeof(arch_pte_t));
	map_last -= 1;
	if ((ia < pgtbl->map_ia) ||
	    ((pgtbl->map_ia + map_last) < ia)) {
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

int mmu_get_guest_page(physical_addr_t pgtbl_guest_pa, int stage, int level,
		       const struct mmu_get_guest_page_ops *ops,
		       void *opaque, physical_addr_t guest_ia,
		       struct mmu_page *pg)
{
	int idx;
	arch_pte_t pte;
	physical_addr_t pte_pa;

	if (stage <= MMU_STAGE_UNKNOWN ||
	    MMU_STAGE_MAX <= stage ||
	    arch_mmu_start_level(stage) < level ||
	    !ops || !pg)
		return VMM_EINVALID;

	if (level < 0)
		level = arch_mmu_start_level(stage);

	idx = ops->gpa2hpa(opaque, stage, level, pgtbl_guest_pa, &pte_pa);
	if (idx) {
		if (idx == VMM_EFAULT) {
			ops->setfault(opaque, stage, level, guest_ia);
		}
		return idx;
	}

	idx = arch_mmu_level_index(guest_ia, stage, level);
	if (vmm_host_memory_read(pte_pa + idx * sizeof(pte),
				 &pte, sizeof(pte), TRUE) != sizeof(pte)) {
		ops->setfault(opaque, stage, level, guest_ia);
		return VMM_EFAULT;
	}

	if (!arch_mmu_pte_is_valid(&pte, stage, level)) {
		ops->setfault(opaque, stage, level, guest_ia);
		return VMM_EFAULT;
	}
	if ((level == 0) &&
	    arch_mmu_pte_is_table(&pte, stage, level)) {
		ops->setfault(opaque, stage, level, guest_ia);
		return VMM_EFAULT;
	}

	if ((level > 0) && arch_mmu_pte_is_table(&pte, stage, level)) {
		pte_pa = arch_mmu_pte_table_addr(&pte, stage, level);
		return mmu_get_guest_page(pte_pa, stage, level - 1,
					  ops, opaque, guest_ia, pg);
	}

	memset(pg, 0, sizeof(struct mmu_page));

	pg->ia = guest_ia & arch_mmu_level_map_mask(stage, level);
	pg->oa = arch_mmu_pte_addr(&pte, stage, level);
	pg->sz = arch_mmu_level_block_size(stage, level);
	arch_mmu_pte_flags(&pte, stage, level, &pg->flags);

	return VMM_OK;
}

void mmu_walk_address(struct mmu_pgtbl *pgtbl, physical_addr_t ia,
		      void (*fn)(struct mmu_pgtbl *, arch_pte_t *, void *),
		      void *opaque)
{
	int index;
	arch_pte_t *pte;
	irq_flags_t flags;
	physical_size_t map_last;
	struct mmu_pgtbl *child;

	if (!pgtbl || !fn) {
		return;
	}

	map_last = arch_mmu_level_block_size(pgtbl->stage, pgtbl->level);
	map_last *= (pgtbl->tbl_sz / sizeof(arch_pte_t));
	map_last -= 1;
	if ((ia < pgtbl->map_ia) ||
	    ((pgtbl->map_ia + map_last) < ia)) {
		return;
	}

	index = arch_mmu_level_index(ia, pgtbl->stage, pgtbl->level);
	pte = (arch_pte_t *)pgtbl->tbl_va;

	fn(pgtbl, &pte[index], opaque);

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);

	if (!arch_mmu_pte_is_valid(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return;
	}

	if ((pgtbl->level == 0) &&
	    arch_mmu_pte_is_table(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return;
	}

	if ((pgtbl->level > 0) &&
	    arch_mmu_pte_is_table(&pte[index], pgtbl->stage, pgtbl->level)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		child = mmu_pgtbl_get_child(pgtbl, ia, FALSE);
		if (!child) {
			return;
		}
		mmu_walk_address(child, ia, fn, opaque);
		return;
	}

	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
}

void mmu_walk_tables(struct mmu_pgtbl *pgtbl,
		     void (*fn)(struct mmu_pgtbl *pgtbl, void *),
		     void *opaque)
{
	irq_flags_t flags;
	struct mmu_pgtbl *child;

	if (!pgtbl || !fn) {
		return;
	}

	fn(pgtbl, opaque);

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);

	list_for_each_entry(child, &pgtbl->child_list, head) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		mmu_walk_tables(child, fn, opaque);
		vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);
	}

	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
}

struct free_address_walk {
	bool found;
	int level;
	physical_addr_t min_addr;
	physical_addr_t *addr;
};

static void free_address_walk(struct mmu_pgtbl *pgtbl, void *opaque)
{
	arch_pte_t *pte;
	irq_flags_t flags;
	physical_addr_t ia;
	int index, pte_count;
	struct free_address_walk *w = opaque;

	if (w->found || pgtbl->level != w->level) {
		return;
	}

	pte = (arch_pte_t *)pgtbl->tbl_va;
	pte_count = pgtbl->tbl_sz / sizeof(arch_pte_t);

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);

	for (index = 0; index < pte_count; index++) {
		if (arch_mmu_pte_is_valid(&pte[index],
					  pgtbl->stage, pgtbl->level)) {
			continue;
		}
		ia = pgtbl->map_ia +
		index * arch_mmu_level_block_size(pgtbl->stage, pgtbl->level);
		if (ia < w->min_addr) {
			continue;
		}
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		w->found = TRUE;
		*w->addr = ia;
		return;
	}

	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);

	return;
}

int mmu_find_free_address(struct mmu_pgtbl *pgtbl, physical_addr_t min_addr,
			   int page_order, physical_addr_t *addr)
{
	int level;
	struct free_address_walk w;

	if (!pgtbl || !addr) {
		return VMM_EINVALID;
	}

	for (level = 0; level <= pgtbl->level; level++) {
		if (arch_mmu_level_block_shift(pgtbl->stage, level) >=
			page_order) {
			break;
		}
	}
	if (pgtbl->level < level) {
		return VMM_EINVALID;
	}

	while (level <= pgtbl->level) {
		w.found = FALSE;
		w.level = level;
		w.min_addr = min_addr;
		w.addr = addr;

		mmu_walk_tables(pgtbl, free_address_walk, &w);
		if (w.found) {
			return VMM_OK;
		}

		level++;
	}

	return VMM_ENOTAVAIL;
}

struct idmap_nested_pgtbl_walk {
	struct mmu_pgtbl *s2_pgtbl;
	int map_level;
	physical_size_t map_size;
	u32 reg_flags;
	int error;
};

static void idmap_nested_pgtbl_walk(struct mmu_pgtbl *pgtbl, void *opaque)
{
	int rc;
	physical_addr_t ta;
	struct mmu_page pg = { 0 }, tpg;

	struct idmap_nested_pgtbl_walk *iw = opaque;

	if (iw->error) {
		return;
	}

	arch_mmu_pgflags_set(&pg.flags, MMU_STAGE2, iw->reg_flags);
	for (ta = 0; ta < pgtbl->tbl_sz; ta += iw->map_size) {
		pg.ia = pgtbl->tbl_pa + ta;
		pg.ia &= arch_mmu_level_map_mask(MMU_STAGE2, iw->map_level);
		pg.oa = pgtbl->tbl_pa + ta;
		pg.oa &= arch_mmu_level_map_mask(MMU_STAGE2, iw->map_level);
		pg.sz = iw->map_size;

		if (mmu_get_page(iw->s2_pgtbl, pg.ia, &tpg)) {
			rc = mmu_map_page(iw->s2_pgtbl, &pg);
			if (rc) {
				iw->error = rc;
				return;
			}
		} else {
			if (pg.ia != tpg.ia ||
			    pg.oa != tpg.oa ||
			    pg.sz != tpg.sz) {
				iw->error = VMM_EFAIL;
				return;
			}
		}
	}
}

int mmu_idmap_nested_pgtbl(struct mmu_pgtbl *s2_pgtbl,
			   struct mmu_pgtbl *s1_pgtbl,
			   physical_size_t map_size, u32 reg_flags)
{
	int level;
	struct idmap_nested_pgtbl_walk iw;

	if (!s2_pgtbl || (s2_pgtbl->stage != MMU_STAGE2)) {
		return VMM_EINVALID;
	}
	if (!s1_pgtbl || (s1_pgtbl->stage != MMU_STAGE1)) {
		return VMM_EINVALID;
	}

	for (level = 0; level <= s2_pgtbl->level; level++) {
		if (arch_mmu_level_block_size(s2_pgtbl->stage, level) ==
			map_size) {
			break;
		}
	}
	if (s2_pgtbl->level < level) {
		return VMM_EINVALID;
	}

	iw.s2_pgtbl = s2_pgtbl;
	iw.map_level = level;
	iw.map_size = map_size;
	iw.reg_flags = reg_flags;
	iw.error = VMM_OK;

	mmu_walk_tables(s1_pgtbl, idmap_nested_pgtbl_walk, &iw);

	return iw.error;
}

int mmu_test_nested_pgtbl(struct mmu_pgtbl *s2_pgtbl,
			  struct mmu_pgtbl *s1_pgtbl,
			  u32 flags, virtual_addr_t addr,
			  physical_addr_t expected_output_addr,
			  u32 expected_fault_flags)
{
	int rc;
	physical_addr_t oaddr = 0;
	u32 offlags = 0;

	if (!s2_pgtbl || (s2_pgtbl->stage != MMU_STAGE2)) {
		return VMM_EINVALID;
	}
	if (s1_pgtbl && (s1_pgtbl->stage != MMU_STAGE1)) {
		return VMM_EINVALID;
	}
	if (flags & ~MMU_TEST_VALID_MASK) {
		return VMM_EINVALID;
	}
	if ((flags & MMU_TEST_WIDTH_16BIT) && (addr & 0x1)) {
		return VMM_EINVALID;
	}
	if ((flags & MMU_TEST_WIDTH_32BIT) && (addr & 0x3)) {
		return VMM_EINVALID;
	}

	rc = arch_mmu_test_nested_pgtbl(s2_pgtbl->tbl_pa,
					(s1_pgtbl) ? TRUE : FALSE,
					(s1_pgtbl) ? s1_pgtbl->tbl_pa : 0,
					flags, addr, &oaddr, &offlags);
	if (rc) {
		return rc;
	}

	/* All expected fault bits should be set */
	if ((offlags & expected_fault_flags) ^ expected_fault_flags) {
		return VMM_EFAIL;
	}

	/* No unexpected fault bit should be set */
	if (offlags & ~expected_fault_flags) {
		return VMM_EFAIL;
	}

	/* Output address should match */
	if (oaddr != expected_output_addr) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

int mmu_get_hypervisor_page(virtual_addr_t va, struct mmu_page *pg)
{
	return mmu_get_page(&mmuctrl.hyp_pgtbl, va, pg);
}

int mmu_unmap_hypervisor_page(struct mmu_page *pg)
{
	return mmu_unmap_page(&mmuctrl.hyp_pgtbl, pg);
}

int mmu_map_hypervisor_page(struct mmu_page *pg)
{
	return mmu_map_page(&mmuctrl.hyp_pgtbl, pg);
}

struct mmu_pgtbl *mmu_hypervisor_pgtbl(void)
{
	return &mmuctrl.hyp_pgtbl;
}

#ifdef ARCH_HAS_MEMORY_READWRITE

/* Initialized by memory read/write init */
static struct mmu_pgtbl *mem_rw_pgtbl[CONFIG_CPU_COUNT];
static arch_pte_t *mem_rw_pte[CONFIG_CPU_COUNT];
static arch_pgflags_t mem_rw_pgflags_cache[CONFIG_CPU_COUNT];
static arch_pgflags_t mem_rw_pgflags_nocache[CONFIG_CPU_COUNT];

int arch_cpu_aspace_memory_read(virtual_addr_t tmp_va,
				physical_addr_t src,
				void *dst, u32 len, bool cacheable)
{
	arch_pte_t old_pte_val;
	u32 cpu = vmm_smp_processor_id();
	arch_pte_t *pte = mem_rw_pte[cpu];
	int pgtbl_level = mem_rw_pgtbl[cpu]->level;
	arch_pgflags_t *flags = (cacheable) ?
		&mem_rw_pgflags_cache[cpu] : &mem_rw_pgflags_nocache[cpu];
	virtual_addr_t offset = src & VMM_PAGE_MASK;

	old_pte_val = *pte;

	arch_mmu_pte_set(pte , MMU_STAGE1, pgtbl_level, src, flags);
	arch_mmu_pte_sync(pte, MMU_STAGE1, pgtbl_level);
	arch_mmu_stage1_tlbflush(FALSE, FALSE, 0, tmp_va, VMM_PAGE_SIZE);

	switch (len) {
	case 1:
		*((u8 *)dst) = *(u8 *)(tmp_va + offset);
		break;
	case 2:
		*((u16 *)dst) = *(u16 *)(tmp_va + offset);
		break;
	case 4:
		*((u32 *)dst) = *(u32 *)(tmp_va + offset);
		break;
	case 8:
		*((u64 *)dst) = *(u64 *)(tmp_va + offset);
		break;
	default:
		memcpy(dst, (void *)(tmp_va + offset), len);
		break;
	};

	*pte = old_pte_val;
	arch_mmu_pte_sync(pte, MMU_STAGE1, pgtbl_level);

	return VMM_OK;
}

int arch_cpu_aspace_memory_write(virtual_addr_t tmp_va,
				 physical_addr_t dst,
				 void *src, u32 len, bool cacheable)
{
	arch_pte_t old_pte_val;
	u32 cpu = vmm_smp_processor_id();
	arch_pte_t *pte = mem_rw_pte[cpu];
	int pgtbl_level = mem_rw_pgtbl[cpu]->level;
	arch_pgflags_t *flags = (cacheable) ?
		&mem_rw_pgflags_cache[cpu] : &mem_rw_pgflags_nocache[cpu];
	virtual_addr_t offset = dst & VMM_PAGE_MASK;

	old_pte_val = *pte;

	arch_mmu_pte_set(pte , MMU_STAGE1, pgtbl_level, dst, flags);
	arch_mmu_pte_sync(pte, MMU_STAGE1, pgtbl_level);
	arch_mmu_stage1_tlbflush(FALSE, FALSE, 0, tmp_va, VMM_PAGE_SIZE);

	switch (len) {
	case 1:
		*(u8 *)(tmp_va + offset) = *((u8 *)src);
		break;
	case 2:
		*(u16 *)(tmp_va + offset) = *((u16 *)src);
		break;
	case 4:
		*(u32 *)(tmp_va + offset) = *((u32 *)src);
		break;
	case 8:
		*(u64 *)(tmp_va + offset) = *((u64 *)src);
		break;
	default:
		memcpy((void *)(tmp_va + offset), src, len);
		break;
	};

	*pte = old_pte_val;
	arch_mmu_pte_sync(pte, MMU_STAGE1, pgtbl_level);

	return VMM_OK;
}

int __cpuinit arch_cpu_aspace_memory_rwinit(virtual_addr_t tmp_va)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();
	struct mmu_page p;

	memset(&p, 0, sizeof(p));
	p.ia = tmp_va;
	p.oa = 0x0;
	p.sz = VMM_PAGE_SIZE;
	arch_mmu_pgflags_set(&p.flags, MMU_STAGE1, VMM_MEMORY_FLAGS_NORMAL);

	rc = mmu_map_hypervisor_page(&p);
	if (rc) {
		return rc;
	}

	mem_rw_pte[cpu] = NULL;
	mem_rw_pgtbl[cpu] = NULL;

	rc = mmu_find_pte(mmu_hypervisor_pgtbl(), tmp_va,
			  &mem_rw_pte[cpu], &mem_rw_pgtbl[cpu]);
	if (rc) {
		return rc;
	}

	arch_mmu_pgflags_set(&mem_rw_pgflags_cache[cpu],
			     MMU_STAGE1, VMM_MEMORY_FLAGS_NORMAL);
	arch_mmu_pgflags_set(&mem_rw_pgflags_nocache[cpu],
			     MMU_STAGE1, VMM_MEMORY_FLAGS_NORMAL_NOCACHE);

	return VMM_OK;
}

#endif

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
	arch_mmu_pgflags_set(&p.flags, MMU_STAGE1, mem_flags);

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

static void __init mmu_scan_initial_pgtbl(struct mmu_pgtbl *pgtbl)
{
	int i, child_idx;
	arch_pte_t *pte;
	struct mmu_pgtbl *child;
	physical_addr_t child_pa;
	physical_addr_t ipgtbl_start = mmuctrl.ipgtbl_base_pa;
	physical_addr_t ipgtbl_end = ipgtbl_start + INIT_PGTBL_SIZE;

	/* Scan all page table entries */
	for (i = 0; i < (STAGE1_NONROOT_SIZE / sizeof(*pte)); i++) {
		pte = &((arch_pte_t *)pgtbl->tbl_va)[i];

		/* Check for valid page table entry */
		if (!arch_mmu_pte_is_valid(pte, pgtbl->stage, pgtbl->level)) {
			continue;
		}
		pgtbl->pte_cnt++;

		/* Check for child page table */
		if (!arch_mmu_pte_is_table(pte, pgtbl->stage, pgtbl->level)) {
			continue;
		}

		/* Current page table level has to be non-zero */
		if (!pgtbl->level) {
			while (1);
		}

		/* Find child page table address */
		child_pa = arch_mmu_pte_table_addr(pte, pgtbl->stage,
						   pgtbl->level);
		if ((child_pa < ipgtbl_start) || (ipgtbl_end <= child_pa)) {
			while (1);
		}

		/* Find child page table pointer */
		child_idx = (child_pa - ipgtbl_start) >> STAGE1_NONROOT_ORDER;
		if (INIT_PGTBL_COUNT <= child_idx) {
			while (1);
		}
		child = &mmuctrl.ipgtbl_pool_array[child_idx];
		if (pgtbl == child) {
			while (1);
		}

		/* Handcraft child page table */
		child->parent = pgtbl;
		child->stage = pgtbl->stage;
		child->level = pgtbl->level - 1;
		child->attr = pgtbl->attr;
		child->map_ia = pgtbl->map_ia;
		child->map_ia += ((arch_pte_t)i) <<
			arch_mmu_level_index_shift(pgtbl->stage,
						   pgtbl->level);

		/* Update page table children */
		pgtbl->child_cnt++;
		list_add_tail(&child->head, &pgtbl->child_list);

		/* Update alloc count */
		mmuctrl.pgtbl_pool_alloc_count++;

		/* Scan child page table */
		mmu_scan_initial_pgtbl(child);
	}
}

int __init arch_cpu_aspace_primary_init(physical_addr_t *core_resv_pa,
					virtual_addr_t *core_resv_va,
					virtual_size_t *core_resv_sz,
					physical_addr_t *arch_resv_pa,
					virtual_addr_t *arch_resv_va,
					virtual_size_t *arch_resv_sz)
{
	int i, rc = VMM_EFAIL;
	virtual_addr_t va, resv_va;
	virtual_size_t sz, resv_sz;
	physical_addr_t pa, resv_pa;
	struct mmu_page hyppg;
	struct mmu_pgtbl *pgtbl;
	virtual_addr_t l0_shift = arch_mmu_level_block_shift(MMU_STAGE1, 0);
	virtual_addr_t l0_size = arch_mmu_level_block_size(MMU_STAGE1, 0);

	/* Check constraints of generic MMU */
	if (STAGE1_NONROOT_SIZE != l0_size ||
	    STAGE1_NONROOT_ALIGN_ORDER > l0_shift)
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
	resv_sz += PGTBL_POOL_SIZE;
	if (resv_sz & (l0_size - 1)) {
		resv_sz += l0_size - (resv_sz & (l0_size - 1));
	}
	*arch_resv_sz = resv_sz - *arch_resv_sz;
	mmuctrl.ipgtbl_base_va = (virtual_addr_t)&stage1_pgtbl_nonroot;
	mmuctrl.ipgtbl_base_pa = mmuctrl.ipgtbl_base_va -
				arch_code_vaddr_start() +
				arch_code_paddr_start();
	INIT_RW_LOCK(&mmuctrl.pgtbl_pool_lock);
	mmuctrl.pgtbl_pool_alloc_count = 0x0;
	INIT_LIST_HEAD(&mmuctrl.pgtbl_pool_free_list);
	INIT_RW_LOCK(&mmuctrl.pgtbl_nonpool_lock);
	INIT_LIST_HEAD(&mmuctrl.pgtbl_nonpool_list);
	INIT_RADIX_TREE(&mmuctrl.pgtbl_nonpool_tree, 0);
	for (i = 0; i < INIT_PGTBL_COUNT; i++) {
		pgtbl = &mmuctrl.ipgtbl_pool_array[i];
		memset(pgtbl, 0, sizeof(struct mmu_pgtbl));
		pgtbl->tbl_pa = mmuctrl.ipgtbl_base_pa + i * STAGE1_NONROOT_SIZE;
		INIT_SPIN_LOCK(&pgtbl->tbl_lock);
		pgtbl->tbl_va = mmuctrl.ipgtbl_base_va + i * STAGE1_NONROOT_SIZE;
		pgtbl->tbl_sz = STAGE1_NONROOT_SIZE;
		INIT_LIST_HEAD(&pgtbl->head);
		INIT_LIST_HEAD(&pgtbl->child_list);
	}
	for (i = 0; i < PGTBL_POOL_COUNT; i++) {
		pgtbl = &mmuctrl.pgtbl_pool_array[i];
		memset(pgtbl, 0, sizeof(struct mmu_pgtbl));
		pgtbl->tbl_pa = mmuctrl.pgtbl_base_pa + i * STAGE1_NONROOT_SIZE;
		INIT_SPIN_LOCK(&pgtbl->tbl_lock);
		pgtbl->tbl_va = mmuctrl.pgtbl_base_va + i * STAGE1_NONROOT_SIZE;
		pgtbl->tbl_sz = STAGE1_NONROOT_SIZE;
		INIT_LIST_HEAD(&pgtbl->head);
		INIT_LIST_HEAD(&pgtbl->child_list);
		list_add_tail(&pgtbl->head, &mmuctrl.pgtbl_pool_free_list);
	}

	/* Handcraft hypervisor page table */
	pgtbl = &mmuctrl.hyp_pgtbl;
	memset(pgtbl, 0, sizeof(struct mmu_pgtbl));
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->tbl_va = (virtual_addr_t)&stage1_pgtbl_root;
	pgtbl->tbl_pa = pgtbl->tbl_va -
			arch_code_vaddr_start() +
			arch_code_paddr_start();
	pgtbl->tbl_sz = STAGE1_ROOT_SIZE;
	INIT_LIST_HEAD(&pgtbl->head);
	INIT_LIST_HEAD(&pgtbl->child_list);
	pgtbl->parent = NULL;
	pgtbl->stage = MMU_STAGE1;
	pgtbl->level = arch_mmu_start_level(MMU_STAGE1);
	pgtbl->attr = MMU_ATTR_REMOTE_TLB_FLUSH;
	pgtbl->map_ia = 0x0;
	mmu_scan_initial_pgtbl(pgtbl);
	for (i = 0; i < INIT_PGTBL_COUNT; i++) {
		pgtbl = &mmuctrl.ipgtbl_pool_array[i];
		if (pgtbl->stage == MMU_STAGE_UNKNOWN) {
			list_add_tail(&pgtbl->head,
				      &mmuctrl.pgtbl_pool_free_list);
		}
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
		arch_mmu_pgflags_set(&hyppg.flags, MMU_STAGE1,
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
		memset((void *)pgtbl->tbl_va, 0, STAGE1_NONROOT_SIZE);
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
