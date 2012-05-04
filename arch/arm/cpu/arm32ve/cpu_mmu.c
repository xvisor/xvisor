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
 * @file cpu_mmu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of MMU for LPAE enabled ARM processor
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <vmm_main.h>
#include <vmm_stdio.h>
#include <vmm_types.h>
#include <arch_sections.h>
#include <cpu_defines.h>
#include <cpu_cache.h>
#include <cpu_barrier.h>
#include <cpu_inline_asm.h>
#include <cpu_mmu.h>

/* Note: we use 1/8th or 12.5% of VAPOOL memory as translation table pool. 
 * For example if VAPOOL is 8 MB then translation table pool will be 1 MB
 * or 1 MB / 4 KB = 256 translation tables
 */
#define TTBL_MAX_TABLE_COUNT 	(CONFIG_VAPOOL_SIZE << \
					(20 - 3 - TTBL_TABLE_SIZE_SHIFT))
#define TTBL_MAX_TABLE_SIZE	(TTBL_MAX_TABLE_COUNT * TTBL_TABLE_SIZE)
#define TTBL_INITIAL_TABLE_SIZE (TTBL_INITIAL_TABLE_COUNT * TTBL_TABLE_SIZE)

struct cpu_mmu_ctrl {
	struct cpu_ttbl * hyp_ttbl;
	virtual_addr_t ttbl_base_va;
	physical_addr_t ttbl_base_pa;
	virtual_addr_t ittbl_base_va;
	physical_addr_t ittbl_base_pa;
	struct cpu_ttbl ttbl_array[TTBL_MAX_TABLE_COUNT];
	struct cpu_ttbl ittbl_array[TTBL_INITIAL_TABLE_COUNT];
	u32 ttbl_alloc_count;
	struct dlist free_ttbl_list;
};

static struct cpu_mmu_ctrl mmuctrl;

u8 __attribute__ ((aligned(TTBL_TABLE_SIZE))) def_ttbl[TTBL_INITIAL_TABLE_SIZE];
int def_ttbl_tree[TTBL_INITIAL_TABLE_COUNT];

static struct cpu_ttbl * cpu_mmu_ttbl_find(physical_addr_t tbl_pa)
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

static bool cpu_mmu_ttbl_isattached(struct cpu_ttbl * child)
{
	if (!child) {
		return FALSE;
	}

	return (child->parent) ? TRUE : FALSE;
}

static bool cpu_mmu_valid_block_size(physical_size_t sz)
{
	if ((sz == TTBL_L3_BLOCK_SIZE) || 
	    (sz == TTBL_L2_BLOCK_SIZE) ||
	    (sz == TTBL_L1_BLOCK_SIZE)) {
		return TRUE;
	}
	return FALSE;
}

static physical_size_t cpu_mmu_level_block_size(int level)
{
	if (level == TTBL_LEVEL1) {
		return TTBL_L1_BLOCK_SIZE;
	} else if (level == TTBL_LEVEL2) {
		return TTBL_L2_BLOCK_SIZE;
	}
	return TTBL_L3_BLOCK_SIZE;
}

static physical_addr_t cpu_mmu_level_map_mask(int level)
{
	if (level == TTBL_LEVEL1) {
		return TTBL_L1_MAP_MASK;
	} else if (level == TTBL_LEVEL2) {
		return TTBL_L2_MAP_MASK;
	}
	return TTBL_L3_MAP_MASK;
}

static int cpu_mmu_level_index(physical_addr_t ia, int level)
{
	if (level == TTBL_LEVEL1) {
		return (ia & TTBL_L1_INDEX_MASK) >> TTBL_L1_INDEX_SHIFT;
	} else if (level == TTBL_LEVEL2) {
		return (ia & TTBL_L2_INDEX_MASK) >> TTBL_L2_INDEX_SHIFT;
	}
	return (ia & TTBL_L3_INDEX_MASK) >> TTBL_L3_INDEX_SHIFT;
}

static int cpu_mmu_level_index_shift(int level)
{
	if (level == TTBL_LEVEL1) {
		return TTBL_L1_INDEX_SHIFT;
	} else if (level == TTBL_LEVEL2) {
		return TTBL_L2_INDEX_SHIFT;
	}
	return TTBL_L3_INDEX_SHIFT;
}

static int cpu_mmu_ttbl_attach(struct cpu_ttbl * parent,
				physical_addr_t map_ia, 
				struct cpu_ttbl * child)
{
	int index;
	u64 * tte;

	if (!parent || !child) {
		return VMM_EFAIL;
	}
	if (cpu_mmu_ttbl_isattached(child)) {
		return VMM_EFAIL;
	}
	if ((parent->level == TTBL_LAST_LEVEL) || 
	    (child->stage != parent->stage)) {
		return VMM_EFAIL;
	}

	index = cpu_mmu_level_index(map_ia, parent->level);
	tte = (u64 *)parent->tbl_va;

	if (tte[index] & TTBL_VALID_MASK) {
		return VMM_EFAIL;
	}

	tte[index] = 0x0;
	tte[index] |= (child->tbl_pa & TTBL_OUTADDR_MASK);
	tte[index] |= (TTBL_TABLE_MASK | TTBL_VALID_MASK);

	child->parent = parent;
	child->level = parent->level + 1;
	child->map_ia = map_ia & cpu_mmu_level_map_mask(parent->level);
	parent->tte_cnt++;
	parent->child_cnt++;
	list_add(&parent->child_list, &child->head);

	return VMM_OK;
}

static int cpu_mmu_ttbl_deattach(struct cpu_ttbl * child)
{
	int index;
	u64 * tte;
	struct cpu_ttbl * parent;

	if (!child || !cpu_mmu_ttbl_isattached(child)) {
		return VMM_EFAIL;
	}

	parent = child->parent;
	index = cpu_mmu_level_index(child->map_ia, parent->level);
	tte = (u64 *)parent->tbl_va;

	if (!(tte[index] & TTBL_VALID_MASK)) {
		return VMM_EFAIL;
	}

	tte[index] = 0x0;

	child->parent = NULL;
	child->level = TTBL_FIRST_LEVEL;
	child->map_ia = 0;
	parent->tte_cnt--;
	parent->child_cnt--;
	list_del(&child->head);

	return VMM_OK;
}

struct cpu_ttbl *cpu_mmu_ttbl_alloc(int stage)
{
	struct dlist * l;
	struct cpu_ttbl * ttbl;

	if (list_empty(&mmuctrl.free_ttbl_list)) {
		return NULL;
	}

	l = list_pop(&mmuctrl.free_ttbl_list);
	ttbl = list_entry(l, struct cpu_ttbl, head);

	ttbl->parent = NULL;
	ttbl->stage = stage;
	ttbl->level = TTBL_FIRST_LEVEL;
	ttbl->map_ia = 0;
	ttbl->tte_cnt = 0;
	ttbl->child_cnt = 0;
	INIT_LIST_HEAD(&ttbl->child_list);

	return ttbl;
}

int cpu_mmu_ttbl_free(struct cpu_ttbl *ttbl)
{
	int rc = VMM_OK;
	struct dlist * l;
	struct cpu_ttbl * child;

	if (!ttbl) {
		return VMM_EFAIL;
	}

	if (cpu_mmu_ttbl_isattached(ttbl)) {
		if ((rc = cpu_mmu_ttbl_deattach(ttbl))) {
			return rc;
		}
	}

	while (!list_empty(&ttbl->child_list)) {
		l = list_first(&ttbl->child_list);
		child = list_entry(l, struct cpu_ttbl, head);
		if ((rc = cpu_mmu_ttbl_deattach(child))) {
			return rc;
		}
		if ((rc = cpu_mmu_ttbl_free(child))) {
			return rc;
		}
	}

	ttbl->tte_cnt = 0;
	vmm_memset((void *)ttbl->tbl_va, 0, TTBL_TABLE_SIZE);

	list_add_tail(&mmuctrl.free_ttbl_list, &ttbl->head);

	return VMM_OK;
}

struct cpu_ttbl *cpu_mmu_ttbl_get_child(struct cpu_ttbl *parent,
					physical_addr_t map_ia,
					bool create)
{
	int rc, index;
	u64 * tte;
	physical_addr_t tbl_pa;
	struct cpu_ttbl *child;

	if (!parent) {
		return NULL;
	}

	index = cpu_mmu_level_index(map_ia, parent->level);
	tte = (u64 *)parent->tbl_va;

	if (tte[index] & TTBL_VALID_MASK) {
		if (tte[index] & TTBL_TABLE_MASK) {
			tbl_pa = tte[index] &  TTBL_OUTADDR_MASK;
			child = cpu_mmu_ttbl_find(tbl_pa);
			if (child->parent == parent) {
				return child;
			}
		}
		return NULL;
	}

	if (!create) {
		return NULL;
	}

	child = cpu_mmu_ttbl_alloc(parent->stage);
	if (!child) {
		return NULL;
	}

	if ((rc = cpu_mmu_ttbl_attach(parent, map_ia, child))) {
		cpu_mmu_ttbl_free(child);
	}

	return child;
}

u32 cpu_mmu_best_page_size(physical_addr_t ia, 
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

int cpu_mmu_get_page(struct cpu_ttbl * ttbl, 
		     physical_addr_t ia, 
		     struct cpu_page * pg)
{
	int index;
	u64 * tte;
	struct cpu_ttbl *child;

	if (!ttbl || !pg) {
		return VMM_EFAIL;
	}

	index = cpu_mmu_level_index(ia, ttbl->level);
	tte = (u64 *)ttbl->tbl_va;

	if (!(tte[index] & TTBL_VALID_MASK)) {
		return VMM_EFAIL;
	}
	if ((ttbl->level == TTBL_LAST_LEVEL) &&
	    !(tte[index] & TTBL_TABLE_MASK)) {
		return VMM_EFAIL;
	}

	if ((ttbl->level < TTBL_LAST_LEVEL) && 
	    (tte[index] & TTBL_TABLE_MASK)) {
		child = cpu_mmu_ttbl_get_child(ttbl, ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		return cpu_mmu_get_page(child, ia, pg);
	}

	vmm_memset(pg, 0, sizeof(struct cpu_page));

	pg->ia = ia & cpu_mmu_level_map_mask(ttbl->level);
	pg->oa = tte[index] & TTBL_OUTADDR_MASK;
	pg->sz = cpu_mmu_level_block_size(ttbl->level);

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

	return VMM_OK;
}

int cpu_mmu_unmap_page(struct cpu_ttbl * ttbl, struct cpu_page * pg)
{
	int index, rc;
	u64 * tte;
	struct cpu_ttbl *child;
	physical_size_t blksz;

	if (!ttbl || !pg) {
		return VMM_EFAIL;
	}
	if (!cpu_mmu_valid_block_size(pg->sz)) {
		return VMM_EFAIL;
	}

	blksz = cpu_mmu_level_block_size(ttbl->level);

	if (pg->sz > blksz ) {
		return VMM_EFAIL;
	}

	if (pg->sz < blksz) {
		child = cpu_mmu_ttbl_get_child(ttbl, pg->ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		rc = cpu_mmu_unmap_page(child, pg);
		if ((ttbl->tte_cnt == 0) && 
		    (ttbl->level > TTBL_FIRST_LEVEL)) {
			cpu_mmu_ttbl_free(ttbl);
		}
		return rc;
	}

	index = cpu_mmu_level_index(pg->ia, ttbl->level);
	tte = (u64 *)ttbl->tbl_va;

	if (!(tte[index] & TTBL_VALID_MASK)) {
		return VMM_EFAIL;
	}
	if ((ttbl->level == TTBL_LAST_LEVEL) &&
	    !(tte[index] & TTBL_TABLE_MASK)) {
		return VMM_EFAIL;
	}

	tte[index] = 0x0;

	if (ttbl->stage == TTBL_STAGE2) {
		invalid_nhtlb();
	} else {
		invalid_htlb_mva(((virtual_addr_t)pg->ia));
	}

	ttbl->tte_cnt--;
	if ((ttbl->tte_cnt == 0) && (ttbl->level > TTBL_FIRST_LEVEL)) {
		cpu_mmu_ttbl_free(ttbl);
	}

	return VMM_OK;
}

int cpu_mmu_map_page(struct cpu_ttbl * ttbl, struct cpu_page * pg)
{
	int index;
	u64 * tte;
	struct cpu_ttbl *child;
	physical_size_t blksz;

	if (!ttbl || !pg) {
		return VMM_EFAIL;
	}
	if (!cpu_mmu_valid_block_size(pg->sz)) {
		return VMM_EINVALID;
	}

	blksz = cpu_mmu_level_block_size(ttbl->level);

	if (pg->sz > blksz ) {
		return VMM_EFAIL;
	}

	if (pg->sz < blksz) {
		child = cpu_mmu_ttbl_get_child(ttbl, pg->ia, TRUE);
		if (!child) {
			return VMM_EFAIL;
		}
		return cpu_mmu_map_page(child, pg);
	}

	index = cpu_mmu_level_index(pg->ia, ttbl->level);
	tte = (u64 *)ttbl->tbl_va;

	if (tte[index] & TTBL_VALID_MASK) {
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
		(cpu_mmu_level_map_mask(ttbl->level) & TTBL_OUTADDR_MASK);

	if (ttbl->level == TTBL_LAST_LEVEL) {
		tte[index] |= TTBL_TABLE_MASK;
	}
	tte[index] |= TTBL_VALID_MASK;

	ttbl->tte_cnt++;

	return VMM_OK;
}

int cpu_mmu_get_hypervisor_page(virtual_addr_t va, struct cpu_page * pg)
{
	return cpu_mmu_get_page(mmuctrl.hyp_ttbl, va, pg);
}

int cpu_mmu_unmap_hypervisor_page(struct cpu_page * pg)
{
	return cpu_mmu_unmap_page(mmuctrl.hyp_ttbl, pg);
}

int cpu_mmu_map_hypervisor_page(struct cpu_page * pg)
{
	return cpu_mmu_map_page(mmuctrl.hyp_ttbl, pg);
}

struct cpu_ttbl * cpu_mmu_hypervisor_ttbl(void)
{
	return mmuctrl.hyp_ttbl;
}

struct cpu_ttbl *cpu_mmu_stage2_curttbl(void)
{
	return cpu_mmu_ttbl_find(read_vttbr() & VTTBR_BADDR_MASK);
}

u8 cpu_mmu_stage2_curvmid(void)
{
	return (read_vttbr() & VTTBR_VMID_MASK) >> VTTBR_VMID_SHIFT;
}

int cpu_mmu_stage2_chttbl(u8 vmid, struct cpu_ttbl * ttbl)
{
	u64 vttbr = 0x0;

	vttbr |= ((u64)vmid << VTTBR_VMID_SHIFT) & VTTBR_VMID_MASK;
	vttbr |= ttbl->tbl_pa  & VTTBR_BADDR_MASK;

	write_vttbr(vttbr);

	return VMM_OK;
}

int arch_cpu_aspace_map(virtual_addr_t va, 
			virtual_size_t sz, 
			physical_addr_t pa,
			u32 mem_flags)
{
	struct cpu_page p;

	vmm_memset(&p, 0, sizeof(p));
	p.ia = va;
	p.oa = pa;
	p.sz = sz;
	p.af = 1;
	if (mem_flags & VMM_MEMORY_WRITEABLE) {
		p.ap = TTBL_AP_SRW_U;
	} else if (mem_flags & VMM_MEMORY_READABLE) {
		p.ap = TTBL_AP_SR_U;
	} else {
		p.ap = TTBL_AP_SR_U;
	}
	p.xn = (mem_flags & VMM_MEMORY_EXECUTABLE) ? 0 : 1;

	if ((mem_flags & VMM_MEMORY_CACHEABLE) && 
	    (mem_flags & VMM_MEMORY_BUFFERABLE)) {
		/* FIXME: */
		p.aindex = AINDEX_NORMAL_WT;
	} else if (mem_flags & VMM_MEMORY_CACHEABLE) {
		/* FIXME: */
		p.aindex = AINDEX_NORMAL_WT;
	} else if (mem_flags & VMM_MEMORY_BUFFERABLE) {
		/* FIXME: */
		p.aindex = AINDEX_NORMAL_WT;
	} else {
		p.aindex = AINDEX_SO;
	}

	return cpu_mmu_map_hypervisor_page(&p);
}

int arch_cpu_aspace_unmap(virtual_addr_t va, virtual_size_t sz)
{
	int rc;
	struct cpu_page p;

	rc = cpu_mmu_get_hypervisor_page(va, &p);
	if (rc) {
		return rc;
	}

	return cpu_mmu_unmap_hypervisor_page(&p);
}

int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t * pa)
{
	int rc = VMM_OK;
	struct cpu_page p;

	if ((rc = cpu_mmu_get_hypervisor_page(va, &p))) {
		return rc;
	}

	*pa = p.oa + (va & (p.sz - 1));

	return VMM_OK;
}

int __init arch_cpu_aspace_init(physical_addr_t * core_resv_pa, 
				virtual_addr_t * core_resv_va,
				virtual_size_t * core_resv_sz,
				physical_addr_t * arch_resv_pa,
				virtual_addr_t * arch_resv_va,
				virtual_size_t * arch_resv_sz)
{
	int i, t, rc = VMM_EFAIL;
	virtual_addr_t va, resv_va = *core_resv_va;
	virtual_size_t sz, resv_sz = *core_resv_sz;
	physical_addr_t pa, resv_pa = *core_resv_pa;
	struct dlist *l;
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
	vmm_memset(&mmuctrl, 0, sizeof(mmuctrl));
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
	mmuctrl.ttbl_alloc_count = 0x0;
	INIT_LIST_HEAD(&mmuctrl.free_ttbl_list);
	for (i = 1; i < TTBL_INITIAL_TABLE_COUNT; i++) {
		if (def_ttbl_tree[i] != -1) {
			continue;
		}
		ttbl = &mmuctrl.ittbl_array[i];
		vmm_memset(ttbl, 0, sizeof(struct cpu_ttbl));
		ttbl->tbl_pa = mmuctrl.ittbl_base_pa + i * TTBL_TABLE_SIZE;
		ttbl->tbl_va = mmuctrl.ittbl_base_va + i * TTBL_TABLE_SIZE;
		INIT_LIST_HEAD(&ttbl->head);
		INIT_LIST_HEAD(&ttbl->child_list);
		list_add_tail(&mmuctrl.free_ttbl_list, &ttbl->head);
	}
	for (i = 0; i < TTBL_MAX_TABLE_COUNT; i++) {
		ttbl = &mmuctrl.ttbl_array[i];
		vmm_memset(ttbl, 0, sizeof(struct cpu_ttbl));
		ttbl->tbl_pa = mmuctrl.ttbl_base_pa + i * TTBL_TABLE_SIZE;
		ttbl->tbl_va = mmuctrl.ttbl_base_va + i * TTBL_TABLE_SIZE;
		INIT_LIST_HEAD(&ttbl->head);
		INIT_LIST_HEAD(&ttbl->child_list);
		list_add_tail(&mmuctrl.free_ttbl_list, &ttbl->head);
	}

	/* Handcraft hypervisor translation table */
	mmuctrl.hyp_ttbl = &mmuctrl.ittbl_array[0];
	vmm_memset(mmuctrl.hyp_ttbl, 0, sizeof(struct cpu_ttbl));
	INIT_LIST_HEAD(&mmuctrl.hyp_ttbl->head);
	mmuctrl.hyp_ttbl->parent = NULL;
	mmuctrl.hyp_ttbl->stage = TTBL_STAGE1;
	mmuctrl.hyp_ttbl->level = TTBL_FIRST_LEVEL;
	mmuctrl.hyp_ttbl->map_ia = 0x0;
	mmuctrl.hyp_ttbl->tbl_pa =  mmuctrl.ittbl_base_pa;
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
		vmm_memset(ttbl, 0, sizeof(struct cpu_ttbl));
		/* Handcraft child tree */
		ttbl->parent = parent;
		ttbl->stage = parent->stage;
		ttbl->level = parent->level + 1;
		ttbl->tbl_pa = mmuctrl.ittbl_base_pa + i * TTBL_TABLE_SIZE;
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
				cpu_mmu_level_index_shift(parent->level);
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
		list_add_tail(&parent->child_list, &ttbl->head);
		/* Update MMU control */
		mmuctrl.ttbl_alloc_count++;
	}

	/* Unmap identity mappings from hypervisor translation table */
	if (arch_code_paddr_start() != arch_code_vaddr_start()) {
		va = arch_code_paddr_start();
		sz = arch_code_size();
		while (sz) {
			vmm_memset(&hyppg, 0, sizeof(hyppg));
			if (!(rc = cpu_mmu_get_hypervisor_page(va, &hyppg))) {
				rc = cpu_mmu_unmap_hypervisor_page(&hyppg);
			}
			if (rc) {
				goto mmu_init_error;
			}
			sz -= TTBL_L3_BLOCK_SIZE;
			va += TTBL_L3_BLOCK_SIZE;
		}
		invalid_tlb();
	}

	/* Map reserved space (core reserved + arch reserved)
	 * We have kept our page table pool in reserved area pages
	 * as cacheable and write-back. We will clean data cache every
	 * time we modify a page table (or translation table) entry.
	 */
	pa = resv_pa;
	va = resv_va;
	sz = resv_sz;
	while (sz) {
		vmm_memset(&hyppg, 0, sizeof(hyppg));
		hyppg.oa = pa;
		hyppg.ia = va;
		hyppg.sz = TTBL_L3_BLOCK_SIZE;
		hyppg.af = 1;
		hyppg.ap = TTBL_AP_SRW_U;
		hyppg.aindex = AINDEX_NORMAL_WT;
		if ((rc = cpu_mmu_map_hypervisor_page(&hyppg))) {
			goto mmu_init_error;
		}
		sz -= TTBL_L3_BLOCK_SIZE;
		pa += TTBL_L3_BLOCK_SIZE;
		va += TTBL_L3_BLOCK_SIZE;
	}

	/* Clear memory of free translation tables. This cannot be done before
	 * we map reserved space (core reserved + arch reserved).
	 */
	list_for_each(l, &mmuctrl.free_ttbl_list) {
		ttbl = list_entry(l, struct cpu_ttbl, head);
		vmm_memset((void *)ttbl->tbl_va, 0, TTBL_TABLE_SIZE);
	}

	return VMM_OK;

mmu_init_error:
	return rc;
}
