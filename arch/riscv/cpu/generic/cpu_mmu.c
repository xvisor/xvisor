/**
 * Copyright (c) 2018 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of MMU for RISC-V CPUs
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <libs/stringlib.h>
#include <arch_sections.h>
#include <arch_barrier.h>

#include <cpu_tlb.h>
#include <cpu_mmu.h>
#include <cpu_sbi.h>

/* Note: we use 1/8th or 12.5% of VAPOOL memory as translation table pool.
 * For example if VAPOOL is 8 MB then translation table pool will be 1 MB
 * or 1 MB / 4 KB = 256 translation tables
 */
#define PGTBL_MAX_TABLE_COUNT 	(CONFIG_VAPOOL_SIZE_MB << \
					(20 - 3 - PGTBL_TABLE_SIZE_SHIFT))
#define PGTBL_MAX_TABLE_SIZE	(PGTBL_MAX_TABLE_COUNT * PGTBL_TABLE_SIZE)
#define PGTBL_INITIAL_TABLE_SIZE (PGTBL_INITIAL_TABLE_COUNT * PGTBL_TABLE_SIZE)

struct cpu_mmu_ctrl {
	int stage1_start_level;
	int stage2_start_level;
	struct cpu_pgtbl *hyp_pgtbl;
	virtual_addr_t pgtbl_base_va;
	physical_addr_t pgtbl_base_pa;
	virtual_addr_t ipgtbl_base_va;
	physical_addr_t ipgtbl_base_pa;
	struct cpu_pgtbl pgtbl_array[PGTBL_MAX_TABLE_COUNT];
	struct cpu_pgtbl ipgtbl_array[PGTBL_INITIAL_TABLE_COUNT];
	vmm_spinlock_t alloc_lock;
	u32 pgtbl_alloc_count;
	struct dlist free_pgtbl_list;
	/* Initialized by memory read/write init */
	struct cpu_pgtbl *mem_rw_pgtbl[CONFIG_CPU_COUNT];
	cpu_pte_t *mem_rw_pte[CONFIG_CPU_COUNT];
	physical_addr_t mem_rw_outaddr_mask[CONFIG_CPU_COUNT];
};

static struct cpu_mmu_ctrl mmuctrl;

u8 __attribute__ ((aligned(PGTBL_TABLE_SIZE))) def_pgtbl[PGTBL_INITIAL_TABLE_SIZE] = { 0 };
int def_pgtbl_tree[PGTBL_INITIAL_TABLE_COUNT];

static inline void cpu_mmu_sync_pte(cpu_pte_t *pte)
{
	arch_smp_mb();
}

static inline void cpu_remote_gpa_guest_tlbflush(physical_addr_t gpa,
						 physical_size_t gsz)
{
	sbi_remote_hfence_gvma(NULL, gpa & ~(gsz - 1), gsz);
}

static inline void cpu_remote_va_hyp_tlb_flush(virtual_addr_t va,
					       virtual_size_t sz)
{
	sbi_remote_sfence_vma(NULL, va & ~(sz - 1), sz);
}

static struct cpu_pgtbl *cpu_mmu_pgtbl_find(physical_addr_t tbl_pa)
{
	int index;

	tbl_pa &= ~(PGTBL_TABLE_SIZE - 1);

	if ((mmuctrl.ipgtbl_base_pa <= tbl_pa) &&
	    (tbl_pa <= (mmuctrl.ipgtbl_base_pa + PGTBL_INITIAL_TABLE_SIZE))) {
		tbl_pa = tbl_pa - mmuctrl.ipgtbl_base_pa;
		index = tbl_pa >> PGTBL_TABLE_SIZE_SHIFT;
		if (index < PGTBL_INITIAL_TABLE_COUNT) {
			return &mmuctrl.ipgtbl_array[index];
		}
	}

	if ((mmuctrl.pgtbl_base_pa <= tbl_pa) &&
	    (tbl_pa <= (mmuctrl.pgtbl_base_pa + PGTBL_MAX_TABLE_SIZE))) {
		tbl_pa = tbl_pa - mmuctrl.pgtbl_base_pa;
		index = tbl_pa >> PGTBL_TABLE_SIZE_SHIFT;
		if (index < PGTBL_MAX_TABLE_COUNT) {
			return &mmuctrl.pgtbl_array[index];
		}
	}

	return NULL;
}

static inline bool cpu_mmu_pgtbl_isattached(struct cpu_pgtbl *child)
{
	return ((child != NULL) && (child->parent != NULL));
}

static inline bool cpu_mmu_valid_block_size(physical_size_t sz)
{
	if (
#ifdef CONFIG_64BIT
	    (sz == PGTBL_L3_BLOCK_SIZE) ||
	    (sz == PGTBL_L2_BLOCK_SIZE) ||
#endif
	    (sz == PGTBL_L1_BLOCK_SIZE) ||
	    (sz == PGTBL_L0_BLOCK_SIZE)) {
		return TRUE;
	}
	return FALSE;
}

static inline physical_size_t cpu_mmu_level_block_size(int level)
{
	switch (level) {
	case 0:
		return PGTBL_L0_BLOCK_SIZE;
	case 1:
		return PGTBL_L1_BLOCK_SIZE;
#ifdef CONFIG_64BIT
	case 2:
		return PGTBL_L2_BLOCK_SIZE;
	case 3:
		return PGTBL_L3_BLOCK_SIZE;
#endif
	default:
		break;
	};
	return PGTBL_L0_BLOCK_SIZE;
}

static inline physical_addr_t cpu_mmu_level_map_mask(int level)
{
	switch (level) {
	case 0:
		return PGTBL_L0_MAP_MASK;
	case 1:
		return PGTBL_L1_MAP_MASK;
#ifdef CONFIG_64BIT
	case 2:
		return PGTBL_L2_MAP_MASK;
	case 3:
		return PGTBL_L3_MAP_MASK;
#endif
	default:
		break;
	};
	return PGTBL_L0_MAP_MASK;
}

static inline int cpu_mmu_level_index(physical_addr_t ia, int level)
{
	switch (level) {
	case 0:
		return (ia & PGTBL_L0_INDEX_MASK) >> PGTBL_L0_INDEX_SHIFT;
	case 1:
		return (ia & PGTBL_L1_INDEX_MASK) >> PGTBL_L1_INDEX_SHIFT;
#ifdef CONFIG_64BIT
	case 2:
		return (ia & PGTBL_L2_INDEX_MASK) >> PGTBL_L2_INDEX_SHIFT;
	case 3:
		return (ia & PGTBL_L3_INDEX_MASK) >> PGTBL_L3_INDEX_SHIFT;
#endif
	default:
		break;
	};
	return (ia & PGTBL_L0_INDEX_MASK) >> PGTBL_L0_INDEX_SHIFT;
}

static inline int cpu_mmu_level_index_shift(int level)
{
	switch (level) {
	case 0:
		return PGTBL_L0_INDEX_SHIFT;
	case 1:
		return PGTBL_L1_INDEX_SHIFT;
#ifdef CONFIG_64BIT
	case 2:
		return PGTBL_L2_INDEX_SHIFT;
	case 3:
		return PGTBL_L3_INDEX_SHIFT;
#endif
	default:
		break;
	};
	return PGTBL_L0_INDEX_SHIFT;
}

static int cpu_mmu_pgtbl_attach(struct cpu_pgtbl *parent,
				physical_addr_t map_ia,
				struct cpu_pgtbl *child)
{
	int index;
	cpu_pte_t *pte;
	irq_flags_t flags;

	if (!parent || !child) {
		return VMM_EFAIL;
	}
	if (cpu_mmu_pgtbl_isattached(child)) {
		return VMM_EFAIL;
	}
	if ((parent->level == 0) ||
	    (child->stage != parent->stage)) {
		return VMM_EFAIL;
	}

	index = cpu_mmu_level_index(map_ia, parent->level);
	pte = (cpu_pte_t *)parent->tbl_va;

	vmm_spin_lock_irqsave_lite(&parent->tbl_lock, flags);

	if (pte[index] & PGTBL_PTE_VALID_MASK) {
		vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);
		return VMM_EFAIL;
	}

	pte[index] = child->tbl_pa >> PGTBL_PAGE_SIZE_SHIFT;
	pte[index] = pte[index] << PGTBL_PTE_ADDR_SHIFT;
	pte[index] |= PGTBL_PTE_VALID_MASK;

	cpu_mmu_sync_pte(&pte[index]);

	child->parent = parent;
	child->level = parent->level - 1;
	child->map_ia = map_ia & cpu_mmu_level_map_mask(parent->level);
	parent->pte_cnt++;
	parent->child_cnt++;
	list_add(&child->head, &parent->child_list);

	vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);

	return VMM_OK;
}

static int cpu_mmu_pgtbl_deattach(struct cpu_pgtbl *child)
{
	int index;
	cpu_pte_t *pte;
	irq_flags_t flags;
	struct cpu_pgtbl *parent;

	if (!child || !cpu_mmu_pgtbl_isattached(child)) {
		return VMM_EFAIL;
	}

	parent = child->parent;
	index = cpu_mmu_level_index(child->map_ia, parent->level);
	pte = (cpu_pte_t *)parent->tbl_va;

	vmm_spin_lock_irqsave_lite(&parent->tbl_lock, flags);

	if (!(pte[index] & PGTBL_PTE_VALID_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);
		return VMM_EFAIL;
	}

	pte[index] = 0x0;
	cpu_mmu_sync_pte(&pte[index]);

	child->parent = NULL;
	parent->pte_cnt--;
	parent->child_cnt--;
	list_del(&child->head);

	vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);

	return VMM_OK;
}

struct cpu_pgtbl *cpu_mmu_pgtbl_alloc(int stage)
{
	irq_flags_t flags;
	struct dlist *l;
	struct cpu_pgtbl *pgtbl;

	vmm_spin_lock_irqsave_lite(&mmuctrl.alloc_lock, flags);

	if (list_empty(&mmuctrl.free_pgtbl_list)) {
		vmm_spin_unlock_irqrestore_lite(&mmuctrl.alloc_lock, flags);
		return NULL;
	}

	l = list_pop(&mmuctrl.free_pgtbl_list);
	pgtbl = list_entry(l, struct cpu_pgtbl, head);
	mmuctrl.pgtbl_alloc_count++;

	vmm_spin_unlock_irqrestore_lite(&mmuctrl.alloc_lock, flags);

	pgtbl->parent = NULL;
	pgtbl->stage = stage;
	pgtbl->level = (stage == PGTBL_STAGE2) ?
		mmuctrl.stage2_start_level : mmuctrl.stage1_start_level;
	pgtbl->map_ia = 0;
	INIT_SPIN_LOCK(&pgtbl->tbl_lock);
	pgtbl->pte_cnt = 0;
	pgtbl->child_cnt = 0;
	INIT_LIST_HEAD(&pgtbl->child_list);
	memset((void *)pgtbl->tbl_va, 0, PGTBL_TABLE_SIZE);

	return pgtbl;
}

int cpu_mmu_pgtbl_free(struct cpu_pgtbl *pgtbl)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct dlist *l;
	struct cpu_pgtbl *child;

	if (!pgtbl) {
		return VMM_EFAIL;
	}

	if (cpu_mmu_pgtbl_isattached(pgtbl)) {
		if ((rc = cpu_mmu_pgtbl_deattach(pgtbl))) {
			return rc;
		}
	}

	while (!list_empty(&pgtbl->child_list)) {
		l = list_first(&pgtbl->child_list);
		child = list_entry(l, struct cpu_pgtbl, head);
		if ((rc = cpu_mmu_pgtbl_deattach(child))) {
			return rc;
		}
		if ((rc = cpu_mmu_pgtbl_free(child))) {
			return rc;
		}
	}

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);
	pgtbl->pte_cnt = 0;
	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);

	pgtbl->level = (pgtbl->stage == PGTBL_STAGE2) ?
		mmuctrl.stage2_start_level : mmuctrl.stage1_start_level;
	pgtbl->map_ia = 0;

	vmm_spin_lock_irqsave_lite(&mmuctrl.alloc_lock, flags);
	list_add_tail(&pgtbl->head, &mmuctrl.free_pgtbl_list);
	mmuctrl.pgtbl_alloc_count--;
	vmm_spin_unlock_irqrestore_lite(&mmuctrl.alloc_lock, flags);

	return VMM_OK;
}

struct cpu_pgtbl *cpu_mmu_pgtbl_get_child(struct cpu_pgtbl *parent,
					  physical_addr_t map_ia,
					  bool create)
{
	int rc, index;
	cpu_pte_t *pte, pte_val;
	irq_flags_t flags;
	physical_addr_t tbl_pa;
	struct cpu_pgtbl *child;

	if (!parent) {
		return NULL;
	}

	index = cpu_mmu_level_index(map_ia, parent->level);
	pte = (cpu_pte_t *)parent->tbl_va;

	vmm_spin_lock_irqsave_lite(&parent->tbl_lock, flags);
	pte_val = pte[index];
	vmm_spin_unlock_irqrestore_lite(&parent->tbl_lock, flags);

	if (pte_val & PGTBL_PTE_VALID_MASK) {
		if ((parent->level > 0) &&
		    !(pte_val & PGTBL_PTE_PERM_MASK)) {
			tbl_pa = pte_val &  PGTBL_PTE_ADDR_MASK;
			tbl_pa = tbl_pa >> PGTBL_PTE_ADDR_SHIFT;
			tbl_pa = tbl_pa << PGTBL_PAGE_SIZE_SHIFT;
			child = cpu_mmu_pgtbl_find(tbl_pa);
			if (child->parent == parent) {
				return child;
			}
		}
		return NULL;
	}

	if (!create) {
		return NULL;
	}

	child = cpu_mmu_pgtbl_alloc(parent->stage);
	if (!child) {
		return NULL;
	}

	if ((rc = cpu_mmu_pgtbl_attach(parent, map_ia, child))) {
		cpu_mmu_pgtbl_free(child);
	}

	return child;
}

u64 cpu_mmu_best_page_size(physical_addr_t ia,
			   physical_addr_t oa,
			   u32 availsz)
{
#ifdef CONFIG_64BIT
	if (!(ia & (PGTBL_L3_BLOCK_SIZE - 1)) &&
	    !(oa & (PGTBL_L3_BLOCK_SIZE - 1)) &&
	    (PGTBL_L3_BLOCK_SIZE <= availsz)) {
		return PGTBL_L3_BLOCK_SIZE;
	}

	if (!(ia & (PGTBL_L2_BLOCK_SIZE - 1)) &&
	    !(oa & (PGTBL_L2_BLOCK_SIZE - 1)) &&
	    (PGTBL_L2_BLOCK_SIZE <= availsz)) {
		return PGTBL_L2_BLOCK_SIZE;
	}
#endif
	if (!(ia & (PGTBL_L1_BLOCK_SIZE - 1)) &&
	    !(oa & (PGTBL_L1_BLOCK_SIZE - 1)) &&
	    (PGTBL_L1_BLOCK_SIZE <= availsz)) {
		return PGTBL_L1_BLOCK_SIZE;
	}

	return PGTBL_L0_BLOCK_SIZE;
}

int cpu_mmu_get_page(struct cpu_pgtbl *pgtbl,
		     physical_addr_t ia, struct cpu_page *pg)
{
	int index;
	cpu_pte_t *pte;
	irq_flags_t flags;
	struct cpu_pgtbl *child;

	if (!pgtbl || !pg) {
		return VMM_EFAIL;
	}

	index = cpu_mmu_level_index(ia, pgtbl->level);
	pte = (cpu_pte_t *)pgtbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);

	if (!(pte[index] & PGTBL_PTE_VALID_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}
	if ((pgtbl->level == 0) &&
	    !(pte[index] & PGTBL_PTE_PERM_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	if ((pgtbl->level > 0) &&
	    !(pte[index] & PGTBL_PTE_PERM_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		child = cpu_mmu_pgtbl_get_child(pgtbl, ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		return cpu_mmu_get_page(child, ia, pg);
	}

	memset(pg, 0, sizeof(struct cpu_page));

	pg->ia = ia & cpu_mmu_level_map_mask(pgtbl->level);
	pg->oa = pte[index] & PGTBL_PTE_ADDR_MASK;
	pg->oa = pg->oa >> PGTBL_PTE_ADDR_SHIFT;
	pg->oa = pg->oa << PGTBL_PAGE_SIZE_SHIFT;
	pg->sz = cpu_mmu_level_block_size(pgtbl->level);

	pg->rsw = (pte[index] & PGTBL_PTE_RSW_MASK) >>
					PGTBL_PTE_RSW_SHIFT;
	pg->dirty = (pte[index] & PGTBL_PTE_DIRTY_MASK) >>
					PGTBL_PTE_DIRTY_SHIFT;
	pg->accessed = (pte[index] & PGTBL_PTE_ACCESSED_MASK) >>
					PGTBL_PTE_ACCESSED_SHIFT;
	pg->global = (pte[index] & PGTBL_PTE_GLOBAL_MASK) >>
					PGTBL_PTE_GLOBAL_SHIFT;
	pg->user = (pte[index] & PGTBL_PTE_USER_MASK) >>
					PGTBL_PTE_USER_SHIFT;
	pg->execute = (pte[index] & PGTBL_PTE_EXECUTE_MASK) >>
					PGTBL_PTE_EXECUTE_SHIFT;
	pg->write = (pte[index] & PGTBL_PTE_WRITE_MASK) >>
					PGTBL_PTE_WRITE_SHIFT;
	pg->read = (pte[index] & PGTBL_PTE_READ_MASK) >>
					PGTBL_PTE_READ_SHIFT;
	pg->valid = (pte[index] & PGTBL_PTE_VALID_MASK) >>
					PGTBL_PTE_VALID_SHIFT;

	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);

	return VMM_OK;
}

int cpu_mmu_unmap_page(struct cpu_pgtbl *pgtbl, struct cpu_page *pg)
{
	int start_level;
	int index, rc;
	bool free_pgtbl;
	cpu_pte_t *pte;
	irq_flags_t flags;
	struct cpu_pgtbl *child;
	physical_size_t blksz;

	if (!pgtbl || !pg) {
		return VMM_EFAIL;
	}
	if (!cpu_mmu_valid_block_size(pg->sz)) {
		return VMM_EFAIL;
	}

	blksz = cpu_mmu_level_block_size(pgtbl->level);

	if (pg->sz > blksz ) {
		return VMM_EFAIL;
	}

	if (pg->sz < blksz) {
		child = cpu_mmu_pgtbl_get_child(pgtbl, pg->ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		rc = cpu_mmu_unmap_page(child, pg);
		if ((pgtbl->pte_cnt == 0) &&
		    (pgtbl->level > 0)) {
			cpu_mmu_pgtbl_free(pgtbl);
		}
		return rc;
	}

	index = cpu_mmu_level_index(pg->ia, pgtbl->level);
	pte = (cpu_pte_t *)pgtbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);

	if (!(pte[index] & PGTBL_PTE_VALID_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}
	if ((pgtbl->level == 0) &&
	    !(pte[index] & PGTBL_PTE_PERM_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	pte[index] = 0x0;
	cpu_mmu_sync_pte(&pte[index]);

	if (pgtbl->stage == PGTBL_STAGE2) {
		cpu_remote_gpa_guest_tlbflush(pg->ia, blksz);
		start_level = mmuctrl.stage2_start_level;
	} else {
		cpu_remote_va_hyp_tlb_flush((virtual_addr_t)pg->ia,
					    (virtual_size_t)blksz);
		start_level = mmuctrl.stage1_start_level;
	}

	pgtbl->pte_cnt--;
	free_pgtbl = FALSE;
	if ((pgtbl->pte_cnt == 0) && (pgtbl->level < start_level)) {
		free_pgtbl = TRUE;
	}

	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);

	if (free_pgtbl) {
		cpu_mmu_pgtbl_free(pgtbl);
	}

	return VMM_OK;
}

int cpu_mmu_map_page(struct cpu_pgtbl *pgtbl, struct cpu_page *pg)
{
	int index;
	cpu_pte_t *pte;
	irq_flags_t flags;
	struct cpu_pgtbl *child;
	physical_size_t blksz;

	if (!pgtbl || !pg) {
		return VMM_EFAIL;
	}
	if (!cpu_mmu_valid_block_size(pg->sz)) {
		return VMM_EINVALID;
	}

	blksz = cpu_mmu_level_block_size(pgtbl->level);
	if (pg->sz > blksz ) {
		return VMM_EFAIL;
	}

	if (pg->sz < blksz) {
		child = cpu_mmu_pgtbl_get_child(pgtbl, pg->ia, TRUE);
		if (!child) {
			return VMM_EFAIL;
		}
		return cpu_mmu_map_page(child, pg);
	}

	index = cpu_mmu_level_index(pg->ia, pgtbl->level);
	pte = (cpu_pte_t *)pgtbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);

	if (pte[index] & PGTBL_PTE_VALID_MASK) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	pte[index] = pg->oa >> PGTBL_PAGE_SIZE_SHIFT;
	pte[index] = pte[index] << PGTBL_PTE_ADDR_SHIFT;

	pte[index] |= ((cpu_pte_t)pg->rsw << PGTBL_PTE_RSW_SHIFT) &
					PGTBL_PTE_RSW_MASK;
	pte[index] |= ((cpu_pte_t)pg->dirty << PGTBL_PTE_DIRTY_SHIFT) &
					PGTBL_PTE_DIRTY_MASK;
	pte[index] |= ((cpu_pte_t)pg->accessed << PGTBL_PTE_ACCESSED_SHIFT) &
					PGTBL_PTE_ACCESSED_MASK;
	pte[index] |= ((cpu_pte_t)pg->global << PGTBL_PTE_GLOBAL_SHIFT) &
					PGTBL_PTE_GLOBAL_MASK;
	pte[index] |= ((cpu_pte_t)pg->user << PGTBL_PTE_USER_SHIFT) &
					PGTBL_PTE_USER_MASK;
	pte[index] |= ((cpu_pte_t)pg->execute << PGTBL_PTE_EXECUTE_SHIFT) &
					PGTBL_PTE_EXECUTE_MASK;
	pte[index] |= ((cpu_pte_t)pg->write << PGTBL_PTE_WRITE_SHIFT) &
					PGTBL_PTE_WRITE_MASK;
	pte[index] |= ((cpu_pte_t)pg->read << PGTBL_PTE_READ_SHIFT) &
					PGTBL_PTE_READ_MASK;

	pte[index] |= PGTBL_PTE_VALID_MASK;

	cpu_mmu_sync_pte(&pte[index]);

	if (pgtbl->stage == PGTBL_STAGE2) {
		cpu_remote_gpa_guest_tlbflush(pg->ia, blksz);
	} else {
		cpu_remote_va_hyp_tlb_flush((virtual_addr_t)pg->ia,
					    (virtual_size_t)blksz);
	}

	pgtbl->pte_cnt++;

	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);

	return VMM_OK;
}

int cpu_mmu_get_hypervisor_page(virtual_addr_t va, struct cpu_page *pg)
{
	return cpu_mmu_get_page(mmuctrl.hyp_pgtbl, va, pg);
}

int cpu_mmu_unmap_hypervisor_page(struct cpu_page *pg)
{
	return cpu_mmu_unmap_page(mmuctrl.hyp_pgtbl, pg);
}

int cpu_mmu_map_hypervisor_page(struct cpu_page *pg)
{
	return cpu_mmu_map_page(mmuctrl.hyp_pgtbl, pg);
}

struct cpu_pgtbl *cpu_mmu_stage2_current_pgtbl(void)
{
	unsigned long pgtbl_ppn = csr_read(CSR_HGATP) & HGATP_PPN;
	return cpu_mmu_pgtbl_find(pgtbl_ppn << PGTBL_PAGE_SIZE_SHIFT);
}

u32 cpu_mmu_stage2_current_vmid(void)
{
	return (csr_read(CSR_HGATP) & HGATP_VMID_MASK) >> HGATP_VMID_SHIFT;
}

int cpu_mmu_stage2_change_pgtbl(u32 vmid, struct cpu_pgtbl *pgtbl)
{
	unsigned long hgatp;

	hgatp = HGATP_MODE;
	hgatp |= ((unsigned long)vmid << HGATP_VMID_SHIFT) & HGATP_VMID_MASK;
	hgatp |= (pgtbl->tbl_pa >> PGTBL_PAGE_SIZE_SHIFT) & HGATP_PPN;

	csr_write(CSR_HGATP, hgatp);

	return VMM_OK;
}

#define PHYS_RW_PTE							\
	(PGTBL_PTE_VALID_MASK			|			\
	 PGTBL_PTE_READ_MASK			|			\
	 PGTBL_PTE_WRITE_MASK)

int arch_cpu_aspace_memory_read(virtual_addr_t tmp_va,
				physical_addr_t src,
				void *dst, u32 len, bool cacheable)
{
	cpu_pte_t old_pte_val;
	u32 cpu = vmm_smp_processor_id();
	cpu_pte_t *pte = mmuctrl.mem_rw_pte[cpu];
	physical_addr_t outaddr_mask = mmuctrl.mem_rw_outaddr_mask[cpu];
	virtual_addr_t offset = (src & ~outaddr_mask);

	old_pte_val = *pte;

	*pte = (src & outaddr_mask) >> PGTBL_PAGE_SIZE_SHIFT;
	*pte = (*pte) << PGTBL_PTE_ADDR_SHIFT;
	*pte |= PHYS_RW_PTE;

	cpu_mmu_sync_pte(pte);
	__sfence_vma_va(tmp_va);

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
	cpu_mmu_sync_pte(pte);

	return VMM_OK;
}

int arch_cpu_aspace_memory_write(virtual_addr_t tmp_va,
				 physical_addr_t dst,
				 void *src, u32 len, bool cacheable)
{
	cpu_pte_t old_pte_val;
	u32 cpu = vmm_smp_processor_id();
	cpu_pte_t *pte = mmuctrl.mem_rw_pte[cpu];
	physical_addr_t outaddr_mask = mmuctrl.mem_rw_outaddr_mask[cpu];
	virtual_addr_t offset = (dst & ~outaddr_mask);

	old_pte_val = *pte;

	*pte = (dst & outaddr_mask) >> PGTBL_PAGE_SIZE_SHIFT;
	*pte = (*pte) << PGTBL_PTE_ADDR_SHIFT;
	*pte |= PHYS_RW_PTE;

	cpu_mmu_sync_pte(pte);
	__sfence_vma_va(tmp_va);

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
	cpu_mmu_sync_pte(pte);

	return VMM_OK;
}

static int __cpuinit cpu_mmu_find_pte(struct cpu_pgtbl *pgtbl,
				      physical_addr_t ia,
				      cpu_pte_t **ptep,
				      struct cpu_pgtbl **pgtblp)
{
	int index;
	cpu_pte_t *pte;
	irq_flags_t flags;
	struct cpu_pgtbl *child;

	if (!pgtbl || !ptep || !pgtblp) {
		return VMM_EFAIL;
	}

	index = cpu_mmu_level_index(ia, pgtbl->level);
	pte = (cpu_pte_t *)pgtbl->tbl_va;

	vmm_spin_lock_irqsave_lite(&pgtbl->tbl_lock, flags);

	if (!(pte[index] & PGTBL_PTE_VALID_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}
	if ((pgtbl->level == 0) &&
	    !(pte[index] & PGTBL_PTE_PERM_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		return VMM_EFAIL;
	}

	if ((pgtbl->level > 0) &&
	    !(pte[index] & PGTBL_PTE_PERM_MASK)) {
		vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);
		child = cpu_mmu_pgtbl_get_child(pgtbl, ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		return cpu_mmu_find_pte(child, ia, ptep, pgtblp);
	}

	*ptep = &pte[index];
	*pgtblp = pgtbl;

	vmm_spin_unlock_irqrestore_lite(&pgtbl->tbl_lock, flags);

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
	p.rsw = 0;
	p.accessed = 0;
	p.dirty = 0;
	p.global = 1;
	p.user = 0;
	p.execute = 1;
	p.write = 1;
	p.read = 1;
	p.valid = 1;

	rc = cpu_mmu_map_hypervisor_page(&p);
	if (rc) {
		return rc;
	}

	mmuctrl.mem_rw_pte[cpu] = NULL;
	mmuctrl.mem_rw_pgtbl[cpu] = NULL;
	rc = cpu_mmu_find_pte(mmuctrl.hyp_pgtbl, tmp_va,
			      &mmuctrl.mem_rw_pte[cpu],
			      &mmuctrl.mem_rw_pgtbl[cpu]);
	if (rc) {
		return rc;
	}
	mmuctrl.mem_rw_outaddr_mask[cpu] =
		cpu_mmu_level_map_mask(mmuctrl.mem_rw_pgtbl[cpu]->level);

	return VMM_OK;
}

u32 arch_cpu_aspace_hugepage_log2size(void)
{
	return PGTBL_L1_BLOCK_SHIFT;
}

int arch_cpu_aspace_map(virtual_addr_t page_va,
			virtual_size_t page_sz,
			physical_addr_t page_pa,
			u32 mem_flags)
{
	struct cpu_page p;

	if (page_sz != PGTBL_L1_BLOCK_SIZE &&
	    page_sz != PGTBL_L0_BLOCK_SIZE)
		return VMM_EINVALID;

	memset(&p, 0, sizeof(p));
	p.ia = page_va;
	p.oa = page_pa;
	p.sz = page_sz;
	p.rsw = 0;
	p.accessed = 0;
	p.dirty = 0;
	p.global = 1;
	p.user = 0;
	p.execute = (mem_flags & VMM_MEMORY_EXECUTABLE) ? 0 : 1;
	p.write = (mem_flags & VMM_MEMORY_WRITEABLE) ? 1 : 0;
	p.read = (mem_flags & VMM_MEMORY_READABLE) ? 1 : 0;
	p.valid = 1;

	/*
	 * We ignore following flags:
	 * VMM_MEMORY_CACHEABLE
	 * VMM_MEMORY_BUFFERABLE
	 * VMM_MEMORY_IO_DEVICE
	 * VMM_MEMORY_DMA_COHERENT
	 * VMM_MEMORY_DMA_NONCOHERENT
	 */

	return cpu_mmu_map_hypervisor_page(&p);
}

int arch_cpu_aspace_unmap(virtual_addr_t page_va)
{
	int rc;
	struct cpu_page p;

	rc = cpu_mmu_get_hypervisor_page(page_va, &p);
	if (rc) {
		return rc;
	}

	return cpu_mmu_unmap_hypervisor_page(&p);
}

int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t *pa)
{
	int rc = VMM_OK;
	struct cpu_page p;

	if ((rc = cpu_mmu_get_hypervisor_page(va, &p))) {
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
	virtual_addr_t va, resv_va;
	virtual_size_t sz, resv_sz;
	physical_addr_t pa, resv_pa;
	struct cpu_page hyppg;
	struct cpu_pgtbl *pgtbl, *parent;

	/* Initial values of resv_va, resv_pa, and resv_sz */
	pa = arch_code_paddr_start();
	va = arch_code_vaddr_start();
	sz = arch_code_size();
	resv_va = va + sz;
	resv_pa = pa + sz;
	resv_sz = 0;
	if (resv_va & (PGTBL_L0_BLOCK_SIZE - 1)) {
		resv_va += PGTBL_L0_BLOCK_SIZE -
			    (resv_va & (PGTBL_L0_BLOCK_SIZE - 1));
	}
	if (resv_pa & (PGTBL_L0_BLOCK_SIZE - 1)) {
		resv_pa += PGTBL_L0_BLOCK_SIZE -
			    (resv_pa & (PGTBL_L0_BLOCK_SIZE - 1));
	}

	/* Initialize MMU control and allocate arch reserved space and
	 * update the *arch_resv_pa, *arch_resv_va, and *arch_resv_sz
	 * parameters to inform host aspace about the arch reserved space.
	 */
	memset(&mmuctrl, 0, sizeof(mmuctrl));
#ifdef CONFIG_64BIT
	mmuctrl.stage1_start_level = 2; /* Assume Sv39 */
	mmuctrl.stage2_start_level = 2; /* Assume Sv39 */
#else
	mmuctrl.stage1_start_level = 1; /* Assume Sv32 */
	mmuctrl.stage2_start_level = 1; /* Assume Sv32 */
#endif
	*arch_resv_va = (resv_va + resv_sz);
	*arch_resv_pa = (resv_pa + resv_sz);
	*arch_resv_sz = resv_sz;
	mmuctrl.pgtbl_base_va = resv_va + resv_sz;
	mmuctrl.pgtbl_base_pa = resv_pa + resv_sz;
	resv_sz += PGTBL_TABLE_SIZE * PGTBL_MAX_TABLE_COUNT;
	if (resv_sz & (PGTBL_L0_BLOCK_SIZE - 1)) {
		resv_sz += PGTBL_L0_BLOCK_SIZE -
			    (resv_sz & (PGTBL_L0_BLOCK_SIZE - 1));
	}
	*arch_resv_sz = resv_sz - *arch_resv_sz;
	mmuctrl.ipgtbl_base_va = (virtual_addr_t)&def_pgtbl;
	mmuctrl.ipgtbl_base_pa = mmuctrl.ipgtbl_base_va -
				arch_code_vaddr_start() +
				arch_code_paddr_start();
	INIT_SPIN_LOCK(&mmuctrl.alloc_lock);
	mmuctrl.pgtbl_alloc_count = 0x0;
	INIT_LIST_HEAD(&mmuctrl.free_pgtbl_list);
	for (i = 1; i < PGTBL_INITIAL_TABLE_COUNT; i++) {
		if (def_pgtbl_tree[i] != -1) {
			continue;
		}
		pgtbl = &mmuctrl.ipgtbl_array[i];
		memset(pgtbl, 0, sizeof(struct cpu_pgtbl));
		pgtbl->tbl_pa = mmuctrl.ipgtbl_base_pa + i * PGTBL_TABLE_SIZE;
		INIT_SPIN_LOCK(&pgtbl->tbl_lock);
		pgtbl->tbl_va = mmuctrl.ipgtbl_base_va + i * PGTBL_TABLE_SIZE;
		INIT_LIST_HEAD(&pgtbl->head);
		INIT_LIST_HEAD(&pgtbl->child_list);
		list_add_tail(&pgtbl->head, &mmuctrl.free_pgtbl_list);
	}
	for (i = 0; i < PGTBL_MAX_TABLE_COUNT; i++) {
		pgtbl = &mmuctrl.pgtbl_array[i];
		memset(pgtbl, 0, sizeof(struct cpu_pgtbl));
		pgtbl->tbl_pa = mmuctrl.pgtbl_base_pa + i * PGTBL_TABLE_SIZE;
		INIT_SPIN_LOCK(&pgtbl->tbl_lock);
		pgtbl->tbl_va = mmuctrl.pgtbl_base_va + i * PGTBL_TABLE_SIZE;
		INIT_LIST_HEAD(&pgtbl->head);
		INIT_LIST_HEAD(&pgtbl->child_list);
		list_add_tail(&pgtbl->head, &mmuctrl.free_pgtbl_list);
	}

	/* Handcraft hypervisor translation table */
	mmuctrl.hyp_pgtbl = &mmuctrl.ipgtbl_array[0];
	memset(mmuctrl.hyp_pgtbl, 0, sizeof(struct cpu_pgtbl));
	INIT_LIST_HEAD(&mmuctrl.hyp_pgtbl->head);
	mmuctrl.hyp_pgtbl->parent = NULL;
	mmuctrl.hyp_pgtbl->stage = PGTBL_STAGE1;
	mmuctrl.hyp_pgtbl->level = mmuctrl.stage1_start_level;
	mmuctrl.hyp_pgtbl->map_ia = 0x0;
	mmuctrl.hyp_pgtbl->tbl_pa =  mmuctrl.ipgtbl_base_pa;
	INIT_SPIN_LOCK(&mmuctrl.hyp_pgtbl->tbl_lock);
	mmuctrl.hyp_pgtbl->tbl_va =  mmuctrl.ipgtbl_base_va;
	mmuctrl.hyp_pgtbl->pte_cnt = 0x0;
	mmuctrl.hyp_pgtbl->child_cnt = 0x0;
	INIT_LIST_HEAD(&mmuctrl.hyp_pgtbl->child_list);
	/* Scan table */
	for (t = 0; t < PGTBL_TABLE_ENTCNT; t++) {
		if (((cpu_pte_t *)mmuctrl.hyp_pgtbl->tbl_va)[t] &
						PGTBL_PTE_VALID_MASK) {
			mmuctrl.hyp_pgtbl->pte_cnt++;
		}
	}
	/* Update MMU control */
	mmuctrl.pgtbl_alloc_count++;
	for (i = 1; i < PGTBL_INITIAL_TABLE_COUNT; i++) {
		if (def_pgtbl_tree[i] == -1) {
			break;
		}
		pgtbl = &mmuctrl.ipgtbl_array[i];
		parent = &mmuctrl.ipgtbl_array[def_pgtbl_tree[i]];
		memset(pgtbl, 0, sizeof(struct cpu_pgtbl));
		/* Handcraft child tree */
		pgtbl->parent = parent;
		pgtbl->stage = parent->stage;
		pgtbl->level = parent->level - 1;
		pgtbl->tbl_pa = mmuctrl.ipgtbl_base_pa + i * PGTBL_TABLE_SIZE;
		INIT_SPIN_LOCK(&pgtbl->tbl_lock);
		pgtbl->tbl_va = mmuctrl.ipgtbl_base_va + i * PGTBL_TABLE_SIZE;
		for (t = 0; t < PGTBL_TABLE_ENTCNT; t++) {
			if (!(((cpu_pte_t *)parent->tbl_va)[t] &
						PGTBL_PTE_VALID_MASK)) {
				continue;
			}
			pa = (((cpu_pte_t *)parent->tbl_va)[t] &
				PGTBL_PTE_ADDR_MASK) >> PGTBL_PTE_ADDR_SHIFT;
			pa = pa << PGTBL_PAGE_SIZE_SHIFT;
			if (pa == pgtbl->tbl_pa) {
				pgtbl->map_ia = parent->map_ia;
				pgtbl->map_ia += ((cpu_pte_t)t) <<
				cpu_mmu_level_index_shift(parent->level);
				break;
			}
		}
		INIT_LIST_HEAD(&pgtbl->head);
		INIT_LIST_HEAD(&pgtbl->child_list);
		/* Scan table enteries */
		for (t = 0; t < PGTBL_TABLE_ENTCNT; t++) {
			if (((cpu_pte_t *)pgtbl->tbl_va)[t] &
						PGTBL_PTE_VALID_MASK) {
				pgtbl->pte_cnt++;
			}
		}
		/* Update parent */
		parent->child_cnt++;
		list_add_tail(&pgtbl->head, &parent->child_list);
		/* Update MMU control */
		mmuctrl.pgtbl_alloc_count++;
	}


	/* Check & setup core reserved space and update the
	 * core_resv_pa, core_resv_va, and core_resv_sz parameters
	 * to inform host aspace about correct placement of the
	 * core reserved space.
	 */
	*core_resv_pa = resv_pa + resv_sz;
	*core_resv_va = resv_va + resv_sz;
	if (*core_resv_sz & (PGTBL_L0_BLOCK_SIZE - 1)) {
		*core_resv_sz += PGTBL_L0_BLOCK_SIZE -
			    (resv_sz & (PGTBL_L0_BLOCK_SIZE - 1));
	}
	resv_sz += *core_resv_sz;

	/* TODO: Unmap identity mappings from hypervisor pgtbl
	 *
	 * The only issue in unmapping identity mapping from hypervisor pgtbl
	 * is that secondary cores (in SMP systems) crash immediatly after
	 * enabling MMU.
	 *
	 * For now as a quick fix, we keep the identity mappings.
	 */

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
		hyppg.sz = PGTBL_L0_BLOCK_SIZE;
		hyppg.rsw = 0;
		hyppg.accessed = 0;
		hyppg.dirty = 0;
		hyppg.global = 0;
		hyppg.user = 0;
		hyppg.execute = 1;
		hyppg.write = 1;
		hyppg.read = 1;
		if ((rc = cpu_mmu_map_hypervisor_page(&hyppg))) {
			goto mmu_init_error;
		}
		sz -= PGTBL_L0_BLOCK_SIZE;
		pa += PGTBL_L0_BLOCK_SIZE;
		va += PGTBL_L0_BLOCK_SIZE;
	}

	/* Clear memory of free translation tables. This cannot be done before
	 * we map reserved space (core reserved + arch reserved).
	 */
	list_for_each_entry(pgtbl, &mmuctrl.free_pgtbl_list, head) {
		memset((void *)pgtbl->tbl_va, 0, PGTBL_TABLE_SIZE);
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
