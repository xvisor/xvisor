/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file mmu_lpae.c
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Implementation of MMU for LPAE enabled ARM processor
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <arch_sections.h>
#include <arch_barrier.h>
#include <libs/stringlib.h>
#include <cpu_mmu_lpae.h>
#include <mmu_lpae.h>

/* Note: we use 1/8th or 12.5% of VAPOOL memory as translation table pool.
 * For example if VAPOOL is 8 MB then translation table pool will be 1 MB
 * or 1 MB / 4 KB = 256 translation tables
 */
#define TTBL_MAX_TABLE_COUNT 	(CONFIG_VAPOOL_SIZE_MB << \
					(20 - 3 - TTBL_TABLE_SIZE_SHIFT))
#define TTBL_MAX_TABLE_SIZE	(TTBL_MAX_TABLE_COUNT * TTBL_TABLE_SIZE)
#define TTBL_INITIAL_TABLE_SIZE (TTBL_INITIAL_TABLE_COUNT * TTBL_TABLE_SIZE)

struct mmu_lpae_ctrl {
	struct cpu_ttbl *hyp_ttbl;
	virtual_addr_t ttbl_base_va;
	physical_addr_t ttbl_base_pa;
	virtual_addr_t ittbl_base_va;
	physical_addr_t ittbl_base_pa;
	struct cpu_ttbl ttbl_array[TTBL_MAX_TABLE_COUNT];
	struct cpu_ttbl ittbl_array[TTBL_INITIAL_TABLE_COUNT];
	vmm_spinlock_t alloc_lock;
	u32 ttbl_alloc_count;
	struct dlist free_ttbl_list;
	/* Initialized by memory read/write init */
	struct cpu_ttbl *mem_rw_ttbl[CONFIG_CPU_COUNT];
	u64 *mem_rw_tte[CONFIG_CPU_COUNT];
};

static struct mmu_lpae_ctrl mmuctrl;

u8 __attribute__ ((aligned(TTBL_TABLE_SIZE))) def_ttbl[TTBL_INITIAL_TABLE_SIZE] = { 0 };
int def_ttbl_tree[TTBL_INITIAL_TABLE_COUNT];

static struct cpu_ttbl *mmu_lpae_ttbl_find(physical_addr_t tbl_pa)
{
	int index;

	tbl_pa &= ~(TTBL_TABLE_SIZE - 1);

	if ((mmuctrl.ittbl_base_pa <= tbl_pa) &&
	    (tbl_pa <= (mmuctrl.ittbl_base_pa + TTBL_INITIAL_TABLE_SIZE))) {
		tbl_pa = tbl_pa - mmuctrl.ittbl_base_pa;
		index = tbl_pa >> TTBL_TABLE_SIZE_SHIFT;
		if (index < TTBL_INITIAL_TABLE_COUNT) {
			return &mmuctrl.ittbl_array[index];
		}
	}

	if ((mmuctrl.ttbl_base_pa <= tbl_pa) &&
	    (tbl_pa <= (mmuctrl.ttbl_base_pa + TTBL_MAX_TABLE_SIZE))) {
		tbl_pa = tbl_pa - mmuctrl.ttbl_base_pa;
		index = tbl_pa >> TTBL_TABLE_SIZE_SHIFT;
		if (index < TTBL_MAX_TABLE_COUNT) {
			return &mmuctrl.ttbl_array[index];
		}
	}

	return NULL;
}

static inline bool mmu_lpae_ttbl_isattached(struct cpu_ttbl *child)
{
	return ((child != NULL) && (child->parent != NULL));
}

static inline bool mmu_lpae_valid_block_size(physical_size_t sz)
{
	if ((sz == TTBL_L3_BLOCK_SIZE) ||
	    (sz == TTBL_L2_BLOCK_SIZE) ||
	    (sz == TTBL_L1_BLOCK_SIZE)) {
		return TRUE;
	}
	return FALSE;
}

static inline physical_size_t mmu_lpae_level_block_size(int level)
{
	if (level == TTBL_LEVEL1) {
		return TTBL_L1_BLOCK_SIZE;
	} else if (level == TTBL_LEVEL2) {
		return TTBL_L2_BLOCK_SIZE;
	}
	return TTBL_L3_BLOCK_SIZE;
}

static inline physical_addr_t mmu_lpae_level_map_mask(int level)
{
	if (level == TTBL_LEVEL1) {
		return TTBL_L1_MAP_MASK;
	} else if (level == TTBL_LEVEL2) {
		return TTBL_L2_MAP_MASK;
	}
	return TTBL_L3_MAP_MASK;
}

static inline int mmu_lpae_level_index(physical_addr_t ia, int level)
{
	if (level == TTBL_LEVEL1) {
		return (ia & TTBL_L1_INDEX_MASK) >> TTBL_L1_INDEX_SHIFT;
	} else if (level == TTBL_LEVEL2) {
		return (ia & TTBL_L2_INDEX_MASK) >> TTBL_L2_INDEX_SHIFT;
	}
	return (ia & TTBL_L3_INDEX_MASK) >> TTBL_L3_INDEX_SHIFT;
}

static inline int mmu_lpae_level_index_shift(int level)
{
	if (level == TTBL_LEVEL1) {
		return TTBL_L1_INDEX_SHIFT;
	} else if (level == TTBL_LEVEL2) {
		return TTBL_L2_INDEX_SHIFT;
	}
	return TTBL_L3_INDEX_SHIFT;
}

static int mmu_lpae_ttbl_attach(struct cpu_ttbl *parent,
				physical_addr_t map_ia,
				struct cpu_ttbl *child)
{
	int index;
	u64 *tte;
	irq_flags_t flags;

	if (!parent || !child) {
		return VMM_EFAIL;
	}
	if (mmu_lpae_ttbl_isattached(child)) {
		return VMM_EFAIL;
	}
	if ((parent->level == TTBL_LAST_LEVEL) ||
	    (child->stage != parent->stage)) {
		return VMM_EFAIL;
	}

	index = mmu_lpae_level_index(map_ia, parent->level);
	tte = (u64 *)parent->tbl_va;

	vmm_spin_lock_irqsave_lite(&parent->tbl_lock, flags);

	if (tte[index] & TTBL_VALID_MASK) {
		vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);
		return VMM_EFAIL;
	}

	tte[index] = 0x0;
	tte[index] |= (child->tbl_pa & TTBL_OUTADDR_MASK);
	tte[index] |= (TTBL_TABLE_MASK | TTBL_VALID_MASK);
	cpu_mmu_sync_tte(&tte[index]);

	child->parent = parent;
	child->level = parent->level + 1;
	child->map_ia = map_ia & mmu_lpae_level_map_mask(parent->level);
	parent->tte_cnt++;
	parent->child_cnt++;
	list_add(&child->head, &parent->child_list);

	vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);

	return VMM_OK;
}

static int mmu_lpae_ttbl_deattach(struct cpu_ttbl *child)
{
	int index;
	u64 *tte;
	irq_flags_t flags;
	struct cpu_ttbl *parent;

	if (!child || !mmu_lpae_ttbl_isattached(child)) {
		return VMM_EFAIL;
	}

	parent = child->parent;
	index = mmu_lpae_level_index(child->map_ia, parent->level);
	tte = (u64 *)parent->tbl_va;

	vmm_spin_lock_irqsave_lite(&parent->tbl_lock, flags);

	if (!(tte[index] & TTBL_VALID_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);
		return VMM_EFAIL;
	}

	tte[index] = 0x0;
	cpu_mmu_sync_tte(&tte[index]);

	child->parent = NULL;
	parent->tte_cnt--;
	parent->child_cnt--;
	list_del(&child->head);

	vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);

	return VMM_OK;
}

struct cpu_ttbl *mmu_lpae_ttbl_alloc(int stage)
{
	irq_flags_t flags;
	struct dlist *l;
	struct cpu_ttbl *ttbl;

	vmm_spin_lock_irqsave_lite(&mmuctrl.alloc_lock, flags);

	if (list_empty(&mmuctrl.free_ttbl_list)) {
		vmm_spin_unlock_irqrestore_lite(&mmuctrl.alloc_lock, flags);
		return NULL;
	}

	l = list_pop(&mmuctrl.free_ttbl_list);
	ttbl = list_entry(l, struct cpu_ttbl, head);
	mmuctrl.ttbl_alloc_count++;

	vmm_spin_unlock_irqrestore_lite(&mmuctrl.alloc_lock, flags);

	ttbl->parent = NULL;
	ttbl->stage = stage;
	ttbl->level = TTBL_FIRST_LEVEL;
	ttbl->map_ia = 0;
	INIT_SPIN_LOCK(&ttbl->tbl_lock);
	ttbl->tte_cnt = 0;
	ttbl->child_cnt = 0;
	INIT_LIST_HEAD(&ttbl->child_list);

	return ttbl;
}

int mmu_lpae_ttbl_free(struct cpu_ttbl *ttbl)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct dlist *l;
	struct cpu_ttbl *child;

	if (!ttbl) {
		return VMM_EFAIL;
	}

	if (mmu_lpae_ttbl_isattached(ttbl)) {
		if ((rc = mmu_lpae_ttbl_deattach(ttbl))) {
			return rc;
		}
	}

	while (!list_empty(&ttbl->child_list)) {
		l = list_first(&ttbl->child_list);
		child = list_entry(l, struct cpu_ttbl, head);
		if ((rc = mmu_lpae_ttbl_deattach(child))) {
			return rc;
		}
		if ((rc = mmu_lpae_ttbl_free(child))) {
			return rc;
		}
	}

	vmm_spin_lock_irqsave_lite(&ttbl->tbl_lock, flags);
	ttbl->tte_cnt = 0;
	memset((void *)ttbl->tbl_va, 0, TTBL_TABLE_SIZE);
	vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);

	ttbl->level = TTBL_FIRST_LEVEL;
	ttbl->map_ia = 0;

	vmm_spin_lock_irqsave_lite(&mmuctrl.alloc_lock, flags);
	list_add_tail(&ttbl->head, &mmuctrl.free_ttbl_list);
	mmuctrl.ttbl_alloc_count--;
	vmm_spin_unlock_irqrestore_lite(&mmuctrl.alloc_lock, flags);

	return VMM_OK;
}

struct cpu_ttbl *mmu_lpae_ttbl_get_child(struct cpu_ttbl *parent,
					physical_addr_t map_ia,
					bool create)
{
	int rc, index;
	u64 *tte, tte_val;
	irq_flags_t flags;
	physical_addr_t tbl_pa;
	struct cpu_ttbl *child;

	if (!parent) {
		return NULL;
	}

	index = mmu_lpae_level_index(map_ia, parent->level);
	tte = (u64 *)parent->tbl_va;

	vmm_spin_lock_irqsave_lite(&parent->tbl_lock, flags);
	tte_val = tte[index];
	vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);

	if (tte_val & TTBL_VALID_MASK) {
		if (tte_val & TTBL_TABLE_MASK) {
			tbl_pa = tte_val &  TTBL_OUTADDR_MASK;
			child = mmu_lpae_ttbl_find(tbl_pa);
			if (child->parent == parent) {
				return child;
			}
		}
		return NULL;
	}

	if (!create) {
		return NULL;
	}

	child = mmu_lpae_ttbl_alloc(parent->stage);
	if (!child) {
		return NULL;
	}

	if ((rc = mmu_lpae_ttbl_attach(parent, map_ia, child))) {
		mmu_lpae_ttbl_free(child);
	}

	return child;
}

u32 mmu_lpae_best_page_size(physical_addr_t ia,
			   physical_addr_t oa,
			   u32 availsz)
{
	if (!(ia & (TTBL_L1_BLOCK_SIZE - 1)) &&
	    !(oa & (TTBL_L1_BLOCK_SIZE - 1)) &&
	    (TTBL_L1_BLOCK_SIZE <= availsz)) {
		return TTBL_L1_BLOCK_SIZE;
	}

	if (!(ia & (TTBL_L2_BLOCK_SIZE - 1)) &&
	    !(oa & (TTBL_L2_BLOCK_SIZE - 1)) &&
	    (TTBL_L2_BLOCK_SIZE <= availsz)) {
		return TTBL_L2_BLOCK_SIZE;
	}

	return TTBL_L3_BLOCK_SIZE;
}

int mmu_lpae_get_page(struct cpu_ttbl *ttbl,
		      physical_addr_t ia, struct cpu_page *pg)
{
	int index;
	u64 *tte;
	irq_flags_t flags;
	struct cpu_ttbl *child;

	if (!ttbl || !pg) {
		return VMM_EFAIL;
	}

	index = mmu_lpae_level_index(ia, ttbl->level);
	tte = (u64 *)ttbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&ttbl->tbl_lock, flags);

	if (!(tte[index] & TTBL_VALID_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);
		return VMM_EFAIL;
	}
	if ((ttbl->level == TTBL_LAST_LEVEL) &&
	    !(tte[index] & TTBL_TABLE_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	if ((ttbl->level < TTBL_LAST_LEVEL) &&
	    (tte[index] & TTBL_TABLE_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);
		child = mmu_lpae_ttbl_get_child(ttbl, ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		return mmu_lpae_get_page(child, ia, pg);
	}

	memset(pg, 0, sizeof(struct cpu_page));

	pg->ia = ia & mmu_lpae_level_map_mask(ttbl->level);
	pg->oa = tte[index] & TTBL_OUTADDR_MASK;
	pg->sz = mmu_lpae_level_block_size(ttbl->level);

	if (ttbl->stage == TTBL_STAGE2) {
		pg->xn = (tte[index] & TTBL_STAGE2_UPPER_XN_MASK) >>
						TTBL_STAGE2_UPPER_XN_SHIFT;
		pg->cont = (tte[index] & TTBL_STAGE2_UPPER_CONT_MASK) >>
						TTBL_STAGE2_UPPER_CONT_SHIFT;
		pg->af = (tte[index] & TTBL_STAGE2_LOWER_AF_MASK) >>
						TTBL_STAGE2_LOWER_AF_SHIFT;
		pg->sh = (tte[index] & TTBL_STAGE2_LOWER_SH_MASK) >>
						TTBL_STAGE2_LOWER_SH_SHIFT;
		pg->ap = (tte[index] & TTBL_STAGE2_LOWER_HAP_MASK) >>
						TTBL_STAGE2_LOWER_HAP_SHIFT;
		pg->memattr = (tte[index] & TTBL_STAGE2_LOWER_MEMATTR_MASK) >>
					TTBL_STAGE2_LOWER_MEMATTR_SHIFT;
	} else {
		pg->xn = (tte[index] & TTBL_STAGE1_UPPER_XN_MASK) >>
						TTBL_STAGE1_UPPER_XN_SHIFT;
		pg->pxn = (tte[index] & TTBL_STAGE1_UPPER_PXN_MASK) >>
						TTBL_STAGE1_UPPER_PXN_SHIFT;
		pg->cont = (tte[index] & TTBL_STAGE1_UPPER_CONT_MASK) >>
						TTBL_STAGE1_UPPER_CONT_SHIFT;
		pg->ng = (tte[index] & TTBL_STAGE1_LOWER_NG_MASK) >>
						TTBL_STAGE1_LOWER_NG_SHIFT;
		pg->af = (tte[index] & TTBL_STAGE1_LOWER_AF_MASK) >>
						TTBL_STAGE1_LOWER_AF_SHIFT;
		pg->sh = (tte[index] & TTBL_STAGE1_LOWER_SH_MASK) >>
						TTBL_STAGE1_LOWER_SH_SHIFT;
		pg->ap = (tte[index] & TTBL_STAGE1_LOWER_AP_MASK) >>
						TTBL_STAGE1_LOWER_AP_SHIFT;
		pg->ns = (tte[index] & TTBL_STAGE1_LOWER_NS_MASK) >>
						TTBL_STAGE1_LOWER_NS_SHIFT;
		pg->aindex = (tte[index] & TTBL_STAGE1_LOWER_AINDEX_MASK) >>
						TTBL_STAGE1_LOWER_AINDEX_SHIFT;
	}

	vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);

	return VMM_OK;
}

int mmu_lpae_unmap_page(struct cpu_ttbl *ttbl, struct cpu_page *pg)
{
	int index, rc;
	bool free_ttbl;
	u64 *tte;
	irq_flags_t flags;
	struct cpu_ttbl *child;
	physical_size_t blksz;

	if (!ttbl || !pg) {
		return VMM_EFAIL;
	}
	if (!mmu_lpae_valid_block_size(pg->sz)) {
		return VMM_EFAIL;
	}

	blksz = mmu_lpae_level_block_size(ttbl->level);

	if (pg->sz > blksz ) {
		return VMM_EFAIL;
	}

	if (pg->sz < blksz) {
		child = mmu_lpae_ttbl_get_child(ttbl, pg->ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		rc = mmu_lpae_unmap_page(child, pg);
		if ((ttbl->tte_cnt == 0) &&
		    (ttbl->level > TTBL_FIRST_LEVEL)) {
			mmu_lpae_ttbl_free(ttbl);
		}
		return rc;
	}

	index = mmu_lpae_level_index(pg->ia, ttbl->level);
	tte = (u64 *)ttbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&ttbl->tbl_lock, flags);

	if (!(tte[index] & TTBL_VALID_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);
		return VMM_EFAIL;
	}
	if ((ttbl->level == TTBL_LAST_LEVEL) &&
	    !(tte[index] & TTBL_TABLE_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	tte[index] = 0x0;
	cpu_mmu_sync_tte(&tte[index]);

	if (ttbl->stage == TTBL_STAGE2) {
		cpu_invalid_ipa_guest_tlb((pg->ia));
	} else {
		cpu_invalid_va_hypervisor_tlb(((virtual_addr_t)pg->ia));
	}

	ttbl->tte_cnt--;
	free_ttbl = FALSE;
	if ((ttbl->tte_cnt == 0) && (ttbl->level > TTBL_FIRST_LEVEL)) {
		free_ttbl = TRUE;
	}

	vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);

	if (free_ttbl) {
		mmu_lpae_ttbl_free(ttbl);
	}

	return VMM_OK;
}

int mmu_lpae_map_page(struct cpu_ttbl *ttbl, struct cpu_page *pg)
{
	int index;
	u64 *tte;
	irq_flags_t flags;
	struct cpu_ttbl *child;
	physical_size_t blksz;

	if (!ttbl || !pg) {
		return VMM_EFAIL;
	}
	if (!mmu_lpae_valid_block_size(pg->sz)) {
		return VMM_EINVALID;
	}

	blksz = mmu_lpae_level_block_size(ttbl->level);

	if (pg->sz > blksz ) {
		return VMM_EFAIL;
	}

	if (pg->sz < blksz) {
		child = mmu_lpae_ttbl_get_child(ttbl, pg->ia, TRUE);
		if (!child) {
			return VMM_EFAIL;
		}
		return mmu_lpae_map_page(child, pg);
	}

	index = mmu_lpae_level_index(pg->ia, ttbl->level);
	tte = (u64 *)ttbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&ttbl->tbl_lock, flags);

	if (tte[index] & TTBL_VALID_MASK) {
		vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	tte[index] = 0x0;
	
	if (ttbl->stage == TTBL_STAGE2) {
		tte[index] |= ((u64)pg->xn << TTBL_STAGE2_UPPER_XN_SHIFT) &
						TTBL_STAGE2_UPPER_XN_MASK;
		tte[index] |= ((u64)pg->cont << TTBL_STAGE2_UPPER_CONT_SHIFT) &
						TTBL_STAGE2_UPPER_CONT_MASK;
		tte[index] |= ((u64)pg->af << TTBL_STAGE2_LOWER_AF_SHIFT) &
						TTBL_STAGE1_LOWER_AF_MASK;
		tte[index] |= ((u64)pg->sh << TTBL_STAGE2_LOWER_SH_SHIFT) &
						TTBL_STAGE2_LOWER_SH_MASK;
		tte[index] |= ((u64)pg->ap << TTBL_STAGE2_LOWER_HAP_SHIFT) &
						TTBL_STAGE2_LOWER_HAP_MASK;
		tte[index] |= ((u64)pg->memattr <<
				TTBL_STAGE2_LOWER_MEMATTR_SHIFT) &
				TTBL_STAGE2_LOWER_MEMATTR_MASK;
	} else {
		tte[index] |= ((u64)pg->xn << TTBL_STAGE1_UPPER_XN_SHIFT) &
						TTBL_STAGE1_UPPER_XN_MASK;
		tte[index] |= ((u64)pg->pxn << TTBL_STAGE1_UPPER_PXN_SHIFT) &
						TTBL_STAGE1_UPPER_PXN_MASK;
		tte[index] |= ((u64)pg->cont << TTBL_STAGE1_UPPER_CONT_SHIFT) &
						TTBL_STAGE1_UPPER_CONT_MASK;
		tte[index] |= ((u64)pg->ng << TTBL_STAGE1_LOWER_NG_SHIFT) &
						TTBL_STAGE1_LOWER_NG_MASK;
		tte[index] |= ((u64)pg->af << TTBL_STAGE1_LOWER_AF_SHIFT) &
						TTBL_STAGE1_LOWER_AF_MASK;
		tte[index] |= ((u64)pg->sh << TTBL_STAGE1_LOWER_SH_SHIFT) &
						TTBL_STAGE1_LOWER_SH_MASK;
		tte[index] |= ((u64)pg->ap << TTBL_STAGE1_LOWER_AP_SHIFT) &
						TTBL_STAGE1_LOWER_AP_MASK;
		tte[index] |= ((u64)pg->ns << TTBL_STAGE1_LOWER_NS_SHIFT) &
						TTBL_STAGE1_LOWER_NS_MASK;
		tte[index] |= ((u64)pg->aindex <<
				TTBL_STAGE1_LOWER_AINDEX_SHIFT) &
				TTBL_STAGE1_LOWER_AINDEX_MASK;
	}

	tte[index] |= pg->oa &
		(mmu_lpae_level_map_mask(ttbl->level) & TTBL_OUTADDR_MASK);

	if (ttbl->level == TTBL_LAST_LEVEL) {
		tte[index] |= TTBL_TABLE_MASK;
	}
	tte[index] |= TTBL_VALID_MASK;

	cpu_mmu_sync_tte(&tte[index]);

	if (ttbl->stage == TTBL_STAGE2) {
		cpu_invalid_ipa_guest_tlb((pg->ia));
	} else {
		cpu_invalid_va_hypervisor_tlb(((virtual_addr_t)pg->ia));
	}

	ttbl->tte_cnt++;

	vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);

	return VMM_OK;
}

int mmu_lpae_get_hypervisor_page(virtual_addr_t va, struct cpu_page *pg)
{
	return mmu_lpae_get_page(mmuctrl.hyp_ttbl, va, pg);
}

int mmu_lpae_unmap_hypervisor_page(struct cpu_page *pg)
{
	return mmu_lpae_unmap_page(mmuctrl.hyp_ttbl, pg);
}

int mmu_lpae_map_hypervisor_page(struct cpu_page *pg)
{
	return mmu_lpae_map_page(mmuctrl.hyp_ttbl, pg);
}

struct cpu_ttbl *mmu_lpae_stage2_curttbl(void)
{
	return mmu_lpae_ttbl_find(cpu_stage2_ttbl_pa());
}

u8 mmu_lpae_stage2_curvmid(void)
{
	return cpu_stage2_vmid();
}

int mmu_lpae_stage2_chttbl(u8 vmid, struct cpu_ttbl *ttbl)
{
	cpu_stage2_update(ttbl->tbl_pa, vmid);
	return VMM_OK;
}

#define PHYS_RW_TTE							\
	((TTBL_TABLE_MASK|TTBL_VALID_MASK)	|			\
	 ((0x1ULL << TTBL_STAGE1_UPPER_XN_SHIFT) &			\
	    TTBL_STAGE1_UPPER_XN_MASK)		|			\
	 ((0x1 << TTBL_STAGE1_LOWER_AF_SHIFT) &				\
	    TTBL_STAGE1_LOWER_AF_MASK)		|			\
	 ((TTBL_SH_INNER_SHAREABLE << TTBL_STAGE1_LOWER_SH_SHIFT) &	\
	    TTBL_STAGE1_LOWER_SH_MASK)		|			\
	 ((TTBL_AP_SRW_U << TTBL_STAGE1_LOWER_AP_SHIFT) &		\
	    TTBL_STAGE1_LOWER_AP_MASK)		|			\
	 ((0x1 << TTBL_STAGE1_LOWER_NS_SHIFT) &				\
	    TTBL_STAGE1_LOWER_NS_MASK))

#define PHYS_RW_TTE_NOCACHE						\
	(PHYS_RW_TTE |							\
	 ((AINDEX_NORMAL_NC << TTBL_STAGE1_LOWER_AINDEX_SHIFT) &	\
	    TTBL_STAGE1_LOWER_AINDEX_MASK))

#define PHYS_RW_TTE_CACHE						\
	(PHYS_RW_TTE |							\
	 ((AINDEX_NORMAL_WB << TTBL_STAGE1_LOWER_AINDEX_SHIFT) &	\
	    TTBL_STAGE1_LOWER_AINDEX_MASK))

int arch_cpu_aspace_memory_read(virtual_addr_t tmp_va,
				physical_addr_t src,
				void *dst, u32 len, bool cacheable)
{
	u64 old_tte_val;
	u64 *tte = mmuctrl.mem_rw_tte[vmm_smp_processor_id()];
	struct cpu_ttbl *ttbl = mmuctrl.mem_rw_ttbl[vmm_smp_processor_id()];
	virtual_addr_t offset = (src & VMM_PAGE_MASK);

	old_tte_val = *tte;

	if (cacheable) {
		*tte = PHYS_RW_TTE_CACHE;
	} else {
		*tte = PHYS_RW_TTE_NOCACHE;
	}
	*tte |= src &
		(mmu_lpae_level_map_mask(ttbl->level) & TTBL_OUTADDR_MASK);

	cpu_mmu_sync_tte(tte);
	cpu_invalid_va_hypervisor_tlb(tmp_va);

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

	*tte = old_tte_val;
	cpu_mmu_sync_tte(tte);

	return VMM_OK;
}

int arch_cpu_aspace_memory_write(virtual_addr_t tmp_va,
				 physical_addr_t dst,
				 void *src, u32 len, bool cacheable)
{
	u64 old_tte_val;
	u64 *tte = mmuctrl.mem_rw_tte[vmm_smp_processor_id()];
	struct cpu_ttbl *ttbl = mmuctrl.mem_rw_ttbl[vmm_smp_processor_id()];
	virtual_addr_t offset = (dst & VMM_PAGE_MASK);

	old_tte_val = *tte;

	if (cacheable) {
		*tte = PHYS_RW_TTE_CACHE;
	} else {
		*tte = PHYS_RW_TTE_NOCACHE;
	}
	*tte |= dst &
		(mmu_lpae_level_map_mask(ttbl->level) & TTBL_OUTADDR_MASK);

	cpu_mmu_sync_tte(tte);
	cpu_invalid_va_hypervisor_tlb(tmp_va);

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

	*tte = old_tte_val;
	cpu_mmu_sync_tte(tte);

	return VMM_OK;
}

static int __cpuinit mmu_lpae_find_tte(struct cpu_ttbl *ttbl,
				       physical_addr_t ia,
				       u64 **ttep, struct cpu_ttbl **ttblp)
{
	int index;
	u64 *tte;
	irq_flags_t flags;
	struct cpu_ttbl *child;

	if (!ttbl || !ttep || !ttblp) {
		return VMM_EFAIL;
	}

	index = mmu_lpae_level_index(ia, ttbl->level);
	tte = (u64 *)ttbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&ttbl->tbl_lock, flags);

	if (!(tte[index] & TTBL_VALID_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);
		return VMM_EFAIL;
	}
	if ((ttbl->level == TTBL_LAST_LEVEL) &&
	    !(tte[index] & TTBL_TABLE_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	if ((ttbl->level < TTBL_LAST_LEVEL) &&
	    (tte[index] & TTBL_TABLE_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);
		child = mmu_lpae_ttbl_get_child(ttbl, ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		return mmu_lpae_find_tte(child, ia, ttep, ttblp);
	}

	*ttep = &tte[index];
	*ttblp = ttbl;

	vmm_spin_unlock_irqrestore_lite(&ttbl->tbl_lock, flags);

	return VMM_OK;
}

int __cpuinit arch_cpu_aspace_memory_rwinit(virtual_addr_t tmp_va)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();
	struct cpu_page p;

	memset(&p, 0, sizeof(p));
	p.ia = tmp_va;
	p.oa = 0x0;
	p.sz = VMM_PAGE_SIZE;
	p.af = 1;
	p.ap = TTBL_AP_SR_U;
	p.xn = 1;
	p.ns = 1;
	p.sh = TTBL_SH_INNER_SHAREABLE;
	p.aindex = AINDEX_NORMAL_NC;

	rc = mmu_lpae_map_hypervisor_page(&p);
	if (rc) {
		return rc;
	}

	mmuctrl.mem_rw_tte[cpu] = NULL;
	mmuctrl.mem_rw_ttbl[cpu] = NULL;
	rc = mmu_lpae_find_tte(mmuctrl.hyp_ttbl, tmp_va,
			       &mmuctrl.mem_rw_tte[cpu],
			       &mmuctrl.mem_rw_ttbl[cpu]);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int arch_cpu_aspace_map(virtual_addr_t page_va,
			physical_addr_t page_pa,
			u32 mem_flags)
{
	struct cpu_page p;

	memset(&p, 0, sizeof(p));
	p.ia = page_va;
	p.oa = page_pa;
	p.sz = VMM_PAGE_SIZE;
	p.af = 1;
	if (mem_flags & VMM_MEMORY_WRITEABLE) {
		p.ap = TTBL_AP_SRW_U;
	} else if (mem_flags & VMM_MEMORY_READABLE) {
		p.ap = TTBL_AP_SR_U;
	} else {
		p.ap = TTBL_AP_SR_U;
	}
	p.xn = (mem_flags & VMM_MEMORY_EXECUTABLE) ? 0 : 1;
	p.ns = 1;
	p.sh = TTBL_SH_INNER_SHAREABLE;

	if ((mem_flags & VMM_MEMORY_CACHEABLE) &&
	    (mem_flags & VMM_MEMORY_BUFFERABLE)) {
		p.aindex = AINDEX_NORMAL_WB;
	} else if (mem_flags & VMM_MEMORY_CACHEABLE) {
		p.aindex = AINDEX_NORMAL_WT;
	} else if (mem_flags & VMM_MEMORY_BUFFERABLE) {
		p.aindex = AINDEX_NORMAL_WB;
	} else if (mem_flags & VMM_MEMORY_IO_DEVICE) {
		p.aindex = AINDEX_DEVICE_nGnRE;
	} else if (mem_flags & VMM_MEMORY_DMA_COHERENT) {
		p.aindex = AINDEX_NORMAL_WB;
	} else if (mem_flags & VMM_MEMORY_DMA_NONCOHERENT) {
		p.aindex = AINDEX_NORMAL_NC;
	} else {
		p.aindex = AINDEX_DEVICE_nGnRnE;
	}

	return mmu_lpae_map_hypervisor_page(&p);
}

int arch_cpu_aspace_unmap(virtual_addr_t page_va)
{
	int rc;
	struct cpu_page p;

	rc = mmu_lpae_get_hypervisor_page(page_va, &p);
	if (rc) {
		return rc;
	}

	return mmu_lpae_unmap_hypervisor_page(&p);
}

int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t *pa)
{
	int rc = VMM_OK;
	struct cpu_page p;

	if ((rc = mmu_lpae_get_hypervisor_page(va, &p))) {
		return rc;
	}

	*pa = p.oa + (va & (p.sz - 1));

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
	struct cpu_page hyppg;
	struct cpu_ttbl *ttbl, *parent;

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
	if (resv_va & (TTBL_L3_BLOCK_SIZE - 1)) {
		resv_va += TTBL_L3_BLOCK_SIZE -
			    (resv_va & (TTBL_L3_BLOCK_SIZE - 1));
	}
	if (resv_pa & (TTBL_L3_BLOCK_SIZE - 1)) {
		resv_pa += TTBL_L3_BLOCK_SIZE -
			    (resv_pa & (TTBL_L3_BLOCK_SIZE - 1));
	}
	if (resv_sz & (TTBL_L3_BLOCK_SIZE - 1)) {
		resv_sz += TTBL_L3_BLOCK_SIZE -
			    (resv_sz & (TTBL_L3_BLOCK_SIZE - 1));
	}
	*core_resv_pa = resv_pa;
	*core_resv_va = resv_va;
	*core_resv_sz = resv_sz;

	/* Initialize MMU control and allocate arch reserved space and
	 * update the *arch_resv_pa, *arch_resv_va, and *arch_resv_sz
	 * parameters to inform host aspace about the arch reserved space.
	 */
	memset(&mmuctrl, 0, sizeof(mmuctrl));
	*arch_resv_va = (resv_va + resv_sz);
	*arch_resv_pa = (resv_pa + resv_sz);
	*arch_resv_sz = resv_sz;
	mmuctrl.ttbl_base_va = resv_va + resv_sz;
	mmuctrl.ttbl_base_pa = resv_pa + resv_sz;
	resv_sz += TTBL_TABLE_SIZE * TTBL_MAX_TABLE_COUNT;
	*arch_resv_sz = resv_sz - *arch_resv_sz;
	mmuctrl.ittbl_base_va = (virtual_addr_t)&def_ttbl;
	mmuctrl.ittbl_base_pa = mmuctrl.ittbl_base_va -
				arch_code_vaddr_start() +
				arch_code_paddr_start();
	INIT_SPIN_LOCK(&mmuctrl.alloc_lock);
	mmuctrl.ttbl_alloc_count = 0x0;
	INIT_LIST_HEAD(&mmuctrl.free_ttbl_list);
	for (i = 1; i < TTBL_INITIAL_TABLE_COUNT; i++) {
		if (def_ttbl_tree[i] != -1) {
			continue;
		}
		ttbl = &mmuctrl.ittbl_array[i];
		memset(ttbl, 0, sizeof(struct cpu_ttbl));
		ttbl->tbl_pa = mmuctrl.ittbl_base_pa + i * TTBL_TABLE_SIZE;
		INIT_SPIN_LOCK(&ttbl->tbl_lock);
		ttbl->tbl_va = mmuctrl.ittbl_base_va + i * TTBL_TABLE_SIZE;
		INIT_LIST_HEAD(&ttbl->head);
		INIT_LIST_HEAD(&ttbl->child_list);
		list_add_tail(&ttbl->head, &mmuctrl.free_ttbl_list);
	}
	for (i = 0; i < TTBL_MAX_TABLE_COUNT; i++) {
		ttbl = &mmuctrl.ttbl_array[i];
		memset(ttbl, 0, sizeof(struct cpu_ttbl));
		ttbl->tbl_pa = mmuctrl.ttbl_base_pa + i * TTBL_TABLE_SIZE;
		INIT_SPIN_LOCK(&ttbl->tbl_lock);
		ttbl->tbl_va = mmuctrl.ttbl_base_va + i * TTBL_TABLE_SIZE;
		INIT_LIST_HEAD(&ttbl->head);
		INIT_LIST_HEAD(&ttbl->child_list);
		list_add_tail(&ttbl->head, &mmuctrl.free_ttbl_list);
	}

	/* Handcraft hypervisor translation table */
	mmuctrl.hyp_ttbl = &mmuctrl.ittbl_array[0];
	memset(mmuctrl.hyp_ttbl, 0, sizeof(struct cpu_ttbl));
	INIT_LIST_HEAD(&mmuctrl.hyp_ttbl->head);
	mmuctrl.hyp_ttbl->parent = NULL;
	mmuctrl.hyp_ttbl->stage = TTBL_STAGE1;
	mmuctrl.hyp_ttbl->level = TTBL_FIRST_LEVEL;
	mmuctrl.hyp_ttbl->map_ia = 0x0;
	mmuctrl.hyp_ttbl->tbl_pa =  mmuctrl.ittbl_base_pa;
	INIT_SPIN_LOCK(&mmuctrl.hyp_ttbl->tbl_lock);
	mmuctrl.hyp_ttbl->tbl_va =  mmuctrl.ittbl_base_va;
	mmuctrl.hyp_ttbl->tte_cnt = 0x0;
	mmuctrl.hyp_ttbl->child_cnt = 0x0;
	INIT_LIST_HEAD(&mmuctrl.hyp_ttbl->child_list);
	/* Scan table */
	for (t = 0; t < TTBL_TABLE_ENTCNT; t++) {
		if (((u64 *)mmuctrl.hyp_ttbl->tbl_va)[t] & TTBL_VALID_MASK) {
			mmuctrl.hyp_ttbl->tte_cnt++;
		}
	}
	/* Update MMU control */
	mmuctrl.ttbl_alloc_count++;
	for (i = 1; i < TTBL_INITIAL_TABLE_COUNT; i++) {
		if (def_ttbl_tree[i] == -1) {
			break;
		}
		ttbl = &mmuctrl.ittbl_array[i];
		parent = &mmuctrl.ittbl_array[def_ttbl_tree[i]];
		memset(ttbl, 0, sizeof(struct cpu_ttbl));
		/* Handcraft child tree */
		ttbl->parent = parent;
		ttbl->stage = parent->stage;
		ttbl->level = parent->level + 1;
		ttbl->tbl_pa = mmuctrl.ittbl_base_pa + i * TTBL_TABLE_SIZE;
		INIT_SPIN_LOCK(&ttbl->tbl_lock);
		ttbl->tbl_va = mmuctrl.ittbl_base_va + i * TTBL_TABLE_SIZE;
		for (t = 0; t < TTBL_TABLE_ENTCNT; t++) {
			if (!(((u64 *)parent->tbl_va)[t] & TTBL_VALID_MASK)) {
				continue;
			}
			if (!(((u64 *)parent->tbl_va)[t] & TTBL_TABLE_MASK)) {
				continue;
			}
			if ((((u64 *)parent->tbl_va)[t] & TTBL_OUTADDR_MASK)
			    == ttbl->tbl_pa) {
				ttbl->map_ia = parent->map_ia;
				ttbl->map_ia += ((u64)t) <<
				mmu_lpae_level_index_shift(parent->level);
				break;
			}
		}
		INIT_LIST_HEAD(&ttbl->head);
		INIT_LIST_HEAD(&ttbl->child_list);
		/* Scan table enteries */
		for (t = 0; t < TTBL_TABLE_ENTCNT; t++) {
			if (((u64 *)ttbl->tbl_va)[t] & TTBL_VALID_MASK) {
				ttbl->tte_cnt++;
			}
		}
		/* Update parent */
		parent->child_cnt++;
		list_add_tail(&ttbl->head, &parent->child_list);
		/* Update MMU control */
		mmuctrl.ttbl_alloc_count++;
	}

	/* TODO: Unmap identity mappings from hypervisor ttbl
	 *
	 * The only issue in unmapping identity mapping from hypervisor ttbl
	 * is that secondary cores (in SMP systems) crash immediatly after
	 * enabling MMU.
	 *
	 * For now as a quick fix, we keep the identity mappings.
	 */
#if 0
	if (arch_code_paddr_start() != arch_code_vaddr_start()) {
		va = arch_code_paddr_start();
		sz = arch_code_size();
		while (sz) {
			memset(&hyppg, 0, sizeof(hyppg));
			if (!(rc = mmu_lpae_get_hypervisor_page(va, &hyppg))) {
				rc = mmu_lpae_unmap_hypervisor_page(&hyppg);
			}
			if (rc) {
				goto mmu_init_error;
			}
			sz -= TTBL_L3_BLOCK_SIZE;
			va += TTBL_L3_BLOCK_SIZE;
		}
		cpu_invalid_all_tlbs();
	}
#endif

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
		hyppg.sz = TTBL_L3_BLOCK_SIZE;
		hyppg.af = 1;
		hyppg.sh = TTBL_SH_INNER_SHAREABLE;
		hyppg.ap = TTBL_AP_SRW_U;
		hyppg.ns = 1;
		hyppg.aindex = AINDEX_NORMAL_WB;
		if ((rc = mmu_lpae_map_hypervisor_page(&hyppg))) {
			goto mmu_init_error;
		}
		sz -= TTBL_L3_BLOCK_SIZE;
		pa += TTBL_L3_BLOCK_SIZE;
		va += TTBL_L3_BLOCK_SIZE;
	}

	/* Clear memory of free translation tables. This cannot be done before
	 * we map reserved space (core reserved + arch reserved).
	 */
	list_for_each_entry(ttbl, &mmuctrl.free_ttbl_list, head) {
		memset((void *)ttbl->tbl_va, 0, TTBL_TABLE_SIZE);
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


