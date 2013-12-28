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
 * @file cpu_pgtbl_helper.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic Pagetable handling code. It is shared both by
 *        host MMU code and code that handles guest.
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

static struct page_table *mmu_pgtbl_find(struct pgtbl_ctrl *ctrl, physical_addr_t tbl_pa)
{
	int index;

	tbl_pa &= ~(PGTBL_TABLE_SIZE - 1);

	if (tbl_pa == ctrl->pgtbl_pml4.tbl_pa) {
		return &ctrl->pgtbl_pml4;
	} else if (tbl_pa == ctrl->pgtbl_pgdp.tbl_pa) {
		return &ctrl->pgtbl_pgdp;
	} else if (tbl_pa == ctrl->pgtbl_pgdi.tbl_pa) {
		return &ctrl->pgtbl_pgdi;
	} else if (tbl_pa == ctrl->pgtbl_pgti.tbl_pa) {
		return &ctrl->pgtbl_pgti;
	}

	if ((ctrl->pgtbl_base_pa <= tbl_pa) &&
	    (tbl_pa <= (ctrl->pgtbl_base_pa + ctrl->pgtbl_max_size))) {
		tbl_pa = tbl_pa - ctrl->pgtbl_base_pa;
		index = tbl_pa >> PGTBL_TABLE_SIZE_SHIFT;
		if (index < ctrl->pgtbl_max_count) {
			return &ctrl->pgtbl_array[index];
		}
	}

	return NULL;
}

static inline bool mmu_pgtbl_isattached(struct page_table *child)
{
	return ((child != NULL) && (child->parent != NULL));
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

struct page_table *mmu_pgtbl_alloc(struct pgtbl_ctrl *ctrl, int stage)
{
	irq_flags_t flags;
	struct dlist *l;
	struct page_table *pgtbl;

	vmm_spin_lock_irqsave(&ctrl->alloc_lock, flags);

	if (list_empty(&ctrl->free_pgtbl_list)) {
		vmm_spin_unlock_irqrestore(&ctrl->alloc_lock, flags);
		return NULL;
	}

	l = list_pop(&ctrl->free_pgtbl_list);
	pgtbl = list_entry(l, struct page_table, head);
	ctrl->pgtbl_alloc_count++;

	vmm_spin_unlock_irqrestore(&ctrl->alloc_lock, flags);

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

int mmu_pgtbl_free(struct pgtbl_ctrl *ctrl, struct page_table *pgtbl)
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
		if ((rc = mmu_pgtbl_free(ctrl, child))) {
			return rc;
		}
	}

	vmm_spin_lock_irqsave(&pgtbl->tbl_lock, flags);
	pgtbl->pte_cnt = 0;
	memset((void *)pgtbl->tbl_va, 0, PGTBL_TABLE_SIZE);
	vmm_spin_unlock_irqrestore(&pgtbl->tbl_lock, flags);

	pgtbl->level = PGTBL_FIRST_LEVEL;
	pgtbl->map_ia = 0;

	vmm_spin_lock_irqsave(&ctrl->alloc_lock, flags);
	list_add_tail(&pgtbl->head, &ctrl->free_pgtbl_list);
	ctrl->pgtbl_alloc_count--;
	vmm_spin_unlock_irqrestore(&ctrl->alloc_lock, flags);

	return VMM_OK;
}

struct page_table *mmu_pgtbl_get_child(struct pgtbl_ctrl *ctrl,
				       struct page_table *parent,
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
		child = mmu_pgtbl_find(ctrl, tbl_pa);
		if (child->parent == parent) {
			return child;
		}
		return NULL;
	}

	if (!create) {
		return NULL;
	}

	child = mmu_pgtbl_alloc(ctrl, parent->stage);
	if (!child) {
		return NULL;
	}

	if ((rc = mmu_pgtbl_attach(parent, map_ia, child))) {
		mmu_pgtbl_free(ctrl, child);
	}

	return child;
}

int mmu_get_page(struct pgtbl_ctrl *ctrl, struct page_table *pgtbl, physical_addr_t ia, union page *pg)
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
		child = mmu_pgtbl_get_child(ctrl, pgtbl, ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		return mmu_get_page(ctrl, child, ia, pg);
	}

	pg->_val = pgt->_val;

	vmm_spin_unlock_irqrestore(&pgtbl->tbl_lock, flags);

	return VMM_OK;
}

int mmu_unmap_page(struct pgtbl_ctrl *ctrl, struct page_table *pgtbl, physical_addr_t ia)
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
		child = mmu_pgtbl_get_child(ctrl, pgtbl, ia, FALSE);
		if (!child) {
			return VMM_EFAIL;
		}
		rc = mmu_unmap_page(ctrl, child, ia);
		if ((pgtbl->pte_cnt == 0) && 
		    (pgtbl->level > PGTBL_FIRST_LEVEL)) {
			mmu_pgtbl_free(ctrl, pgtbl);
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
		mmu_pgtbl_free(ctrl, pgtbl);
	}

	return VMM_OK;
}

int mmu_map_page(struct pgtbl_ctrl *ctrl, struct page_table *pgtbl, physical_addr_t ia, union page *pg)
{
	int index;
	union page *pgt;
	irq_flags_t flags;
	struct page_table *child;

	if (!pgtbl || !pg) {
		return VMM_EFAIL;
	}

	if (pgtbl->level < PGTBL_LAST_LEVEL) {
		child = mmu_pgtbl_get_child(ctrl, pgtbl, ia, TRUE);
		if (!child) {
			return VMM_EFAIL;
		}

		return mmu_map_page(ctrl, child, ia, pg);
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
