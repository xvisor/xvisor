/**
 * Copyright (c) 2011 Anup Patel.
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
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Implementation of memory managment unit for ARM processors
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_host_aspace.h>
#include <arch_barrier.h>
#include <arch_cpu_irq.h>
#include <arch_sections.h>
#include <libs/stringlib.h>
#include <cpu_defines.h>
#include <cpu_proc.h>
#include <cpu_cache.h>
#include <cpu_inline_asm.h>
#include <cpu_mmu.h>

/* Note: we use 1/8th or 12.5% of VAPOOL memory as translation table pool. 
 * For example if VAPOOL is 8 MB then translation table pool will be 1 MB
 * or 1 MB / 4 KB = 256 translation tables
 */
#define TTBL_POOL_MAX_SIZE 	(CONFIG_VAPOOL_SIZE_MB << (20 - 3))

#define TTBL_MAX_L1TBL_COUNT	(CONFIG_MAX_VCPU_COUNT)

#define TTBL_MAX_L2TBL_COUNT	(((TTBL_POOL_MAX_SIZE - \
				  (TTBL_MAX_L1TBL_COUNT * TTBL_L1TBL_SIZE))) / \
				 TTBL_L2TBL_SIZE)

u8 __attribute__((aligned(TTBL_L1TBL_SIZE))) tmpl1_mem[TTBL_L1TBL_SIZE];
u8 __attribute__((aligned(TTBL_L1TBL_SIZE))) defl1_mem[TTBL_L1TBL_SIZE];

struct cpu_mmu_ctrl {
	vmm_spinlock_t defl1_lock;
	struct cpu_l1tbl defl1;
	virtual_addr_t l1_base_va;
	physical_addr_t l1_base_pa;
	struct cpu_l1tbl *l1_array;
	virtual_addr_t l2_base_va;
	physical_addr_t l2_base_pa;
	struct cpu_l2tbl *l2_array;
	vmm_spinlock_t l1_alloc_lock;
	u32 l1_next_contextid;
	u32 l1_alloc_count;
	struct dlist l1tbl_list;
	struct dlist free_l1tbl_list;
	vmm_spinlock_t l2_alloc_lock;
	u32 l2_alloc_count;
	struct dlist free_l2tbl_list;
};

static struct cpu_mmu_ctrl mmuctrl;

static inline void cpu_mmu_sync_tte(u32 *tte)
{
	clean_dcache_mva((virtual_addr_t)tte);
	isb();
	dsb();
}

/** Find L2 page table at given physical address from L1 page table */
static struct cpu_l2tbl *cpu_mmu_l2tbl_find_tbl_pa(physical_addr_t tbl_pa)
{
	u32 tmp = (tbl_pa - mmuctrl.l2_base_pa) >> TTBL_L2TBL_SIZE_SHIFT;

	if (tmp < TTBL_MAX_L2TBL_COUNT) {
		return &mmuctrl.l2_array[tmp];
	}

	return NULL;
}

/** Check whether a L2 page table is attached or not */
int cpu_mmu_l2tbl_is_attached(struct cpu_l2tbl *l2)
{
	if (!l2) {
		return 0;
	}
	return (l2->l1) ? 1 : 0;
}

/** Detach a L2 page table */
int cpu_mmu_l2tbl_detach(struct cpu_l2tbl *l2)
{
	u32 *l1_tte;
	u32 l1_tte_type;

	/* Sanity check */
	if (!l2) {
		return VMM_EFAIL;
	}

	/* if it is not attached already, nothing to do */
	if (!cpu_mmu_l2tbl_is_attached(l2)) {
		return VMM_OK;
	}

	l1_tte = (u32 *) (l2->l1->tbl_va +
			  ((l2->map_va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_FAULT) {
		return VMM_EFAIL;
	}

	*l1_tte = 0x0;
	cpu_mmu_sync_tte(l1_tte);
	l2->l1->tte_cnt--;
	l2->l1->l2tbl_cnt--;
	l2->l1 = NULL;
	l2->tte_cnt = 0;

	memset((void *)l2->tbl_va, 0, TTBL_L2TBL_SIZE);

	/* remove the L2 page table from the list it is attached */
	list_del(&l2->head);

	return VMM_OK;
}

/** Attach a L2 page table to a particular L1 page table */
int cpu_mmu_l2tbl_attach(struct cpu_l1tbl *l1, struct cpu_l2tbl *l2, u32 new_imp,
			 u32 new_domain, virtual_addr_t new_map_va, bool force)
{
	u32 *l1_tte, l1_tte_new;
	u32 l1_tte_type;

	/* Sanity check */
	if (!l2 || !l1) {
		return VMM_EFAIL;
	}

	/* If the L2 page is already attached */
	if (cpu_mmu_l2tbl_is_attached(l2)) {
		return VMM_EFAIL;
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((new_map_va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	if ((l1_tte_type != TTBL_L1TBL_TTE_TYPE_FAULT) && !force) {
		return VMM_EFAIL;
	}

	l2->l1 = l1;
#if defined(CONFIG_ARMV5)
	l2->imp = 0;
#else
	l2->imp =
	    new_imp & (TTBL_L1TBL_TTE_IMP_MASK >> TTBL_L1TBL_TTE_IMP_SHIFT);
#endif
	l2->domain =
	    new_domain & (TTBL_L1TBL_TTE_DOM_MASK >> TTBL_L1TBL_TTE_DOM_SHIFT);
	l2->map_va = new_map_va & TTBL_L1TBL_TTE_OFFSET_MASK;

#if defined(CONFIG_ARMV5)
	l1_tte_new = TTBL_L1TBL_TTE_REQ_MASK;
	l1_tte_new |= (l2->domain) << TTBL_L1TBL_TTE_DOM_SHIFT;
	l1_tte_new |= (l2->tbl_pa & TTBL_L1TBL_TTE_BASE10_MASK);
	l1_tte_new |= TTBL_L1TBL_TTE_TYPE_COARSE_L2TBL;
#else
	l1_tte_new = 0x0;
	l1_tte_new |= (l2->imp) << TTBL_L1TBL_TTE_IMP_SHIFT;
	l1_tte_new |= (l2->domain) << TTBL_L1TBL_TTE_DOM_SHIFT;
	l1_tte_new |= (l2->tbl_pa & TTBL_L1TBL_TTE_BASE10_MASK);
	l1_tte_new |= TTBL_L1TBL_TTE_TYPE_L2TBL;
#endif

	*l1_tte = l1_tte_new;
	cpu_mmu_sync_tte(l1_tte);

	if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_FAULT) {
		l1->tte_cnt++;
	}
	l1->l2tbl_cnt++;

	list_add(&l2->head, &l1->l2tbl_list);

	return VMM_OK;
}

/** Allocate a L2 page table of given type */
struct cpu_l2tbl *cpu_mmu_l2tbl_alloc(void)
{
	irq_flags_t flags;
	struct cpu_l2tbl *l2;

	vmm_spin_lock_irqsave(&mmuctrl.l2_alloc_lock, flags);
	if (list_empty(&mmuctrl.free_l2tbl_list)) {
		vmm_spin_unlock_irqrestore(&mmuctrl.l2_alloc_lock, flags);
		return NULL;
	}
	l2 = list_entry(list_first(&mmuctrl.free_l2tbl_list), 
			struct cpu_l2tbl, head);
	list_del(&l2->head);
	mmuctrl.l2_alloc_count++;
	vmm_spin_unlock_irqrestore(&mmuctrl.l2_alloc_lock, flags);

	INIT_LIST_HEAD(&l2->head);
	l2->l1 = NULL;
	l2->imp = 0;
	l2->domain = 0;
	l2->map_va = 0;
	l2->tte_cnt = 0;
	memset((void *)l2->tbl_va, 0, TTBL_L2TBL_SIZE);

	return l2;
}

/** Free a L2 page table */
int cpu_mmu_l2tbl_free(struct cpu_l2tbl *l2)
{
	int rc;
	irq_flags_t flags;

	if (!l2) {
		return VMM_EFAIL;
	}

	if (cpu_mmu_l2tbl_is_attached(l2)) {
		rc = cpu_mmu_l2tbl_detach(l2);
		if (rc) {
			return rc;
		}
	}

	INIT_LIST_HEAD(&l2->head);
	l2->l1 = NULL;

	vmm_spin_lock_irqsave(&mmuctrl.l2_alloc_lock, flags);
	list_add_tail(&l2->head, &mmuctrl.free_l2tbl_list);
	mmuctrl.l2_alloc_count--;
	vmm_spin_unlock_irqrestore(&mmuctrl.l2_alloc_lock, flags);

	return VMM_OK;
}

/** Find L1 page table at given physical address */
struct cpu_l1tbl *cpu_mmu_l1tbl_find_tbl_pa(physical_addr_t tbl_pa)
{
	u32 tmp;

	if (tbl_pa == mmuctrl.defl1.tbl_pa) {
		return &mmuctrl.defl1;
	}

	tmp = (tbl_pa - mmuctrl.l1_base_pa) >> TTBL_L1TBL_SIZE_SHIFT;

	if (tmp < TTBL_MAX_L1TBL_COUNT) {
		return &mmuctrl.l1_array[tmp];
	}

	return NULL;
}

u32 cpu_mmu_best_page_size(virtual_addr_t va, physical_addr_t pa, u32 availsz)
{
	if (!(va & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) &&
	    !(pa & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) &&
	    (TTBL_L1TBL_SECTION_PAGE_SIZE <= availsz)) {
		return TTBL_L1TBL_SECTION_PAGE_SIZE;
	}

	if (!(va & (TTBL_L2TBL_LARGE_PAGE_SIZE - 1)) &&
	    !(pa & (TTBL_L2TBL_LARGE_PAGE_SIZE - 1)) &&
	    (TTBL_L2TBL_LARGE_PAGE_SIZE <= availsz)) {
		return TTBL_L2TBL_LARGE_PAGE_SIZE;
	}

	return TTBL_L2TBL_SMALL_PAGE_SIZE;
}

int cpu_mmu_get_page(struct cpu_l1tbl *l1, virtual_addr_t va, struct cpu_page *pg)
{
	int ret = VMM_EFAIL;
	u32 *l1_tte, *l2_tte;
	u32 l1_tte_type;
#if !defined(CONFIG_ARMV5)
	u32 l1_sec_type;
#endif
	physical_addr_t l2base;
	struct cpu_l2tbl *l2;
	struct cpu_page r;

	if (!l1) {
		return VMM_EFAIL;
	}

	if (!pg) {
		pg = &r;
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;

#if defined(CONFIG_ARMV5)
	switch (l1_tte_type) {
	case TTBL_L1TBL_TTE_TYPE_SECTION:
		pg->va = va & TTBL_L1TBL_TTE_OFFSET_MASK;
		pg->ap = (*l1_tte & TTBL_L1TBL_TTE_AP_MASK) >>
		    TTBL_L1TBL_TTE_AP_SHIFT;
		pg->c = (*l1_tte & TTBL_L1TBL_TTE_C_MASK) >>
		    TTBL_L1TBL_TTE_C_SHIFT;
		pg->b = (*l1_tte & TTBL_L1TBL_TTE_B_MASK) >>
		    TTBL_L1TBL_TTE_B_SHIFT;
		pg->pa = *l1_tte & TTBL_L1TBL_TTE_BASE20_MASK;
		pg->sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		pg->dom = (*l1_tte & TTBL_L1TBL_TTE_DOM_MASK) >>
		    TTBL_L1TBL_TTE_DOM_SHIFT;
		ret = VMM_OK;
		break;
	case TTBL_L1TBL_TTE_TYPE_COARSE_L2TBL:
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		l2_tte = (u32 *) ((va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
		if (!(l2 = cpu_mmu_l2tbl_find_tbl_pa(l2base))) {
			break;
		}
		l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
		pg->va = va & TTBL_L2TBL_TTE_BASE12_MASK;
		pg->dom = (*l1_tte & TTBL_L1TBL_TTE_DOM_MASK) >>
		    TTBL_L1TBL_TTE_DOM_SHIFT;
		pg->ap = (*l2_tte & TTBL_L2TBL_TTE_V5_AP0_MASK) >>
		    TTBL_L2TBL_TTE_V5_AP0_SHIFT;
		pg->c = (*l2_tte & TTBL_L2TBL_TTE_C_MASK) >>
		    TTBL_L2TBL_TTE_C_SHIFT;
		pg->b = (*l2_tte & TTBL_L2TBL_TTE_B_MASK) >>
		    TTBL_L2TBL_TTE_B_SHIFT;
		switch (*l2_tte & TTBL_L2TBL_TTE_TYPE_MASK) {
		case TTBL_L2TBL_TTE_TYPE_LARGE:
			pg->pa = *l2_tte & TTBL_L2TBL_TTE_BASE16_MASK;
			pg->sz = TTBL_L2TBL_LARGE_PAGE_SIZE;
			ret = VMM_OK;
			break;
		case TTBL_L2TBL_TTE_TYPE_SMALL:
			pg->pa = *l2_tte & TTBL_L2TBL_TTE_BASE12_MASK;
			pg->sz = TTBL_L2TBL_SMALL_PAGE_SIZE;
			ret = VMM_OK;
			break;
		default:
			ret = VMM_ENOTAVAIL;
			break;
		}
		break;
	default:
		memset(pg, 0, sizeof(struct cpu_page));
		ret = VMM_ENOTAVAIL;
		break;
	}
#else
	switch (l1_tte_type) {
	case TTBL_L1TBL_TTE_TYPE_SECTION:
		pg->va = va & TTBL_L1TBL_TTE_OFFSET_MASK;
		pg->ns = (*l1_tte & TTBL_L1TBL_TTE_NS2_MASK) >> 
						TTBL_L1TBL_TTE_NS2_SHIFT;
		pg->ng = (*l1_tte & TTBL_L1TBL_TTE_NG_MASK) >> 
						TTBL_L1TBL_TTE_NG_SHIFT;
		pg->s = (*l1_tte & TTBL_L1TBL_TTE_S_MASK) >> 
						TTBL_L1TBL_TTE_S_SHIFT;
		pg->ap = (*l1_tte & TTBL_L1TBL_TTE_AP2_MASK) >> 
						(TTBL_L1TBL_TTE_AP2_SHIFT - 2);
		pg->tex = (*l1_tte & TTBL_L1TBL_TTE_TEX_MASK) >> 
						TTBL_L1TBL_TTE_TEX_SHIFT;
		pg->ap |= (*l1_tte & TTBL_L1TBL_TTE_AP_MASK) >> 
						TTBL_L1TBL_TTE_AP_SHIFT;
		pg->imp = (*l1_tte & TTBL_L1TBL_TTE_IMP_MASK) >> 
						TTBL_L1TBL_TTE_IMP_SHIFT;
		pg->xn = (*l1_tte & TTBL_L1TBL_TTE_XN_MASK) >> 
						TTBL_L1TBL_TTE_XN_SHIFT;
		pg->c = (*l1_tte & TTBL_L1TBL_TTE_C_MASK) >> 
						TTBL_L1TBL_TTE_C_SHIFT;
		pg->b = (*l1_tte & TTBL_L1TBL_TTE_B_MASK) >> 
						TTBL_L1TBL_TTE_B_SHIFT;
		l1_sec_type = (*l1_tte & TTBL_L1TBL_TTE_SECTYPE_MASK) >>
	    					TTBL_L1TBL_TTE_SECTYPE_SHIFT;
		if (l1_sec_type) {
			pg->pa = *l1_tte & TTBL_L1TBL_TTE_BASE24_MASK;
			pg->sz = TTBL_L1TBL_SUPSECTION_PAGE_SIZE;
			pg->dom = 0;
		} else {
			pg->pa = *l1_tte & TTBL_L1TBL_TTE_BASE20_MASK;
			pg->sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
			pg->dom = (*l1_tte & TTBL_L1TBL_TTE_DOM_MASK) >> 
						TTBL_L1TBL_TTE_DOM_SHIFT;
		}
		ret = VMM_OK;
		break;
	case TTBL_L1TBL_TTE_TYPE_L2TBL:
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		l2_tte = (u32 *) ((va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2 = cpu_mmu_l2tbl_find_tbl_pa(l2base);
		if (l2) {
			l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
			pg->va = va & TTBL_L2TBL_TTE_BASE12_MASK;
			pg->imp = (*l1_tte & TTBL_L1TBL_TTE_IMP_MASK) >> 
						TTBL_L1TBL_TTE_IMP_SHIFT;
			pg->dom = (*l1_tte & TTBL_L1TBL_TTE_DOM_MASK) >> 
						TTBL_L1TBL_TTE_DOM_SHIFT;
			pg->ns = (*l1_tte & TTBL_L1TBL_TTE_NS1_MASK) >> 
						TTBL_L1TBL_TTE_NS1_SHIFT;
			pg->ng = (*l2_tte & TTBL_L2TBL_TTE_NG_MASK) >> 
						TTBL_L2TBL_TTE_NG_SHIFT;
			pg->s = (*l2_tte & TTBL_L2TBL_TTE_S_MASK) >> 
						TTBL_L2TBL_TTE_S_SHIFT;
			pg->ap = (*l2_tte & TTBL_L2TBL_TTE_AP2_MASK) >> 
						(TTBL_L2TBL_TTE_AP2_SHIFT - 2);
			pg->ap |= (*l2_tte & TTBL_L2TBL_TTE_AP_MASK) >> 
						TTBL_L2TBL_TTE_AP_SHIFT;
			pg->c = (*l2_tte & TTBL_L2TBL_TTE_C_MASK) >> 
						TTBL_L2TBL_TTE_C_SHIFT;
			pg->b = (*l2_tte & TTBL_L2TBL_TTE_B_MASK) >> 
						TTBL_L2TBL_TTE_B_SHIFT;
			switch (*l2_tte & TTBL_L2TBL_TTE_TYPE_MASK) {
			case TTBL_L2TBL_TTE_TYPE_LARGE:
				pg->pa = *l2_tte & TTBL_L2TBL_TTE_BASE16_MASK;
				pg->xn = (*l2_tte & TTBL_L2TBL_TTE_LXN_MASK) >> 
						TTBL_L2TBL_TTE_LXN_SHIFT;
				pg->tex = (*l2_tte & TTBL_L2TBL_TTE_LTEX_MASK) >> 
						TTBL_L2TBL_TTE_LTEX_SHIFT;
				pg->sz = TTBL_L2TBL_LARGE_PAGE_SIZE;
				ret = VMM_OK;
				break;
			case TTBL_L2TBL_TTE_TYPE_SMALL_X:
			case TTBL_L2TBL_TTE_TYPE_SMALL_XN:
				pg->pa = *l2_tte & TTBL_L2TBL_TTE_BASE12_MASK;
				pg->tex = (*l2_tte & TTBL_L2TBL_TTE_STEX_MASK) >>
						TTBL_L2TBL_TTE_STEX_SHIFT;
				pg->xn = *l2_tte & TTBL_L2TBL_TTE_SXN_MASK;
				pg->sz = TTBL_L2TBL_SMALL_PAGE_SIZE;
				ret = VMM_OK;
				break;
			default:
				ret = VMM_ENOTAVAIL;
				break;
			}
		}
		break;
	default:
		memset(pg, 0, sizeof(struct cpu_page));
		ret = VMM_ENOTAVAIL;
		break;
	};
#endif

	return ret;
}

int cpu_mmu_unmap_page(struct cpu_l1tbl *l1, struct cpu_page *pg)
{
	int ret = VMM_EFAIL;
	u32 ite, *l1_tte, *l2_tte;
	u32 l1_tte_type;
#if !defined(CONFIG_ARMV5)
	u32 l1_sec_type, found = 0;
#endif
	physical_addr_t l2base, pgpa, chkpa;
	virtual_size_t chksz;
	struct cpu_l2tbl *l2 = NULL;

	if (!l1 || !pg) {
		return ret;
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((pg->va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;

#if defined(CONFIG_ARMV5)
	switch (l1_tte_type) {
	case TTBL_L1TBL_TTE_TYPE_FAULT:
		break;
	case TTBL_L1TBL_TTE_TYPE_SECTION:
		pgpa = pg->pa & TTBL_L1TBL_TTE_BASE20_MASK;
		chkpa = *l1_tte & TTBL_L1TBL_TTE_BASE20_MASK;
		chksz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
			*l1_tte = 0x0;
			cpu_mmu_sync_tte(l1_tte);
			l1->tte_cnt--;
			ret = VMM_OK;
		}
		break;
	case TTBL_L1TBL_TTE_TYPE_COARSE_L2TBL:
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		if (!(l2 = cpu_mmu_l2tbl_find_tbl_pa(l2base))) {
			break;
		}
		l2_tte = (u32 *) ((pg->va & ~TTBL_L1TBL_TTE_OFFSET_MASK)
				  >> TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
		switch (*l2_tte & TTBL_L2TBL_TTE_TYPE_MASK) {
		case TTBL_L2TBL_TTE_TYPE_LARGE:
			l2_tte = l2_tte - ((u32) l2_tte % 64) / 4;
			pgpa = pg->pa & TTBL_L2TBL_TTE_BASE16_MASK;
			chkpa = *l2_tte & TTBL_L2TBL_TTE_BASE16_MASK;
			chksz = TTBL_L2TBL_LARGE_PAGE_SIZE;
			if ((pgpa == chkpa) && (pg->sz == chksz)) {
				for (ite = 0; ite < 16; ite++) {
					l2_tte[ite] = 0x0;
					cpu_mmu_sync_tte(&l2_tte[ite]);
					l2->tte_cnt--;
				}
				if (!l2->tte_cnt) {
					cpu_mmu_l2tbl_free(l2);
				}
				ret = VMM_OK;
			}
			break;
		case TTBL_L2TBL_TTE_TYPE_SMALL:
			pgpa = pg->pa & TTBL_L2TBL_TTE_BASE12_MASK;
			chkpa = *l2_tte & TTBL_L2TBL_TTE_BASE12_MASK;
			chksz = TTBL_L2TBL_SMALL_PAGE_SIZE;
			if ((pgpa == chkpa) && (pg->sz == chksz)) {
				*l2_tte = 0x0;
				cpu_mmu_sync_tte(l2_tte);
				l2->tte_cnt--;
				if (!l2->tte_cnt) {
					cpu_mmu_l2tbl_free(l2);
				}
				ret = VMM_OK;
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
#else
	found = 0;
	switch (l1_tte_type) {
	case TTBL_L1TBL_TTE_TYPE_FAULT:
		break;
	case TTBL_L1TBL_TTE_TYPE_SECTION:
		l1_sec_type = (*l1_tte & TTBL_L1TBL_TTE_SECTYPE_MASK) >>
						TTBL_L1TBL_TTE_SECTYPE_SHIFT;
		if (l1_sec_type) {
			l1_tte = l1_tte - ((u32)l1_tte % 64) / 4;
			pgpa = pg->pa & TTBL_L1TBL_TTE_BASE24_MASK;
			chkpa = *l1_tte & TTBL_L1TBL_TTE_BASE24_MASK;
			chksz = TTBL_L1TBL_SUPSECTION_PAGE_SIZE;
			found = 1;
		} else {
			pgpa = pg->pa & TTBL_L1TBL_TTE_BASE20_MASK;
			chkpa = *l1_tte & TTBL_L1TBL_TTE_BASE20_MASK;
			chksz = TTBL_L1TBL_SECTION_PAGE_SIZE;
			found = 2;
		}
		break;
	case TTBL_L1TBL_TTE_TYPE_L2TBL:
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		l2_tte = (u32 *) ((pg->va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2 = cpu_mmu_l2tbl_find_tbl_pa(l2base);
		if (l2) {
			l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
			switch (*l2_tte & TTBL_L2TBL_TTE_TYPE_MASK) {
			case TTBL_L2TBL_TTE_TYPE_LARGE:
				l2_tte = l2_tte - ((u32)l2_tte % 64) / 4;
				pgpa = pg->pa & TTBL_L2TBL_TTE_BASE16_MASK;
				chkpa = *l2_tte & TTBL_L2TBL_TTE_BASE16_MASK;
				chksz = TTBL_L2TBL_LARGE_PAGE_SIZE;
				found = 3;
				break;
			case TTBL_L2TBL_TTE_TYPE_SMALL_X:
			case TTBL_L2TBL_TTE_TYPE_SMALL_XN:
				pgpa = pg->pa & TTBL_L2TBL_TTE_BASE12_MASK;
				chkpa = *l2_tte & TTBL_L2TBL_TTE_BASE12_MASK;
				chksz = TTBL_L2TBL_SMALL_PAGE_SIZE;
				found = 4;
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	};

	switch (found) {
	case 1: /* Super Section */
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
			for (ite = 0; ite < 16; ite++) {
				l1_tte[ite] = 0x0;
				cpu_mmu_sync_tte(&l1_tte[ite]);
				l1->tte_cnt--;
			}
			ret = VMM_OK;
		}
		break;
	case 2: /* Section */
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
			*l1_tte = 0x0;
			cpu_mmu_sync_tte(l1_tte);
			l1->tte_cnt--;
			ret = VMM_OK;
		}
		break;
	case 3: /* Large Page */
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
			for (ite = 0; ite < 16; ite++) {
				l2_tte[ite] = 0x0;
				cpu_mmu_sync_tte(&l2_tte[ite]);
				l2->tte_cnt--;				
			}
			if (!l2->tte_cnt) {
				cpu_mmu_l2tbl_free(l2);
			}
			ret = VMM_OK;
		}
		break;
	case 4: /* Small Page */
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
			*l2_tte = 0x0;
			cpu_mmu_sync_tte(l2_tte);
			l2->tte_cnt--;
			if (!l2->tte_cnt) {
				cpu_mmu_l2tbl_free(l2);
			}
			ret = VMM_OK;
		}
		break;
	default:
		break;
	}
#endif

	if (!ret) {
		/* If given L1 page table is current then 
		 * invalidate tlb line 
		 */
		if(read_ttbr0() == l1->tbl_pa) {
			invalid_tlb_mva(pg->va);
			dsb();
			isb();
		}
	}

	return ret;
}

int cpu_mmu_map_page(struct cpu_l1tbl *l1, struct cpu_page *pg)
{
	int rc = VMM_OK;
	u32 *l1_tte, *l2_tte;
	u32 ite, l1_tte_type;
	virtual_addr_t pgva;
	virtual_size_t pgsz, minpgsz;
	physical_addr_t l2base;
	struct cpu_page upg;
	struct cpu_l2tbl *l2;

	if (!l1 || !pg) {
		rc = VMM_EFAIL;
		goto mmu_map_return;
	}

	/* Get the l1 TBL location */
	l1_tte = (u32 *) (l1->tbl_va +
			  ((pg->va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));

	/* Get l1 TLB value */
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;

	/* If the l1 TBL is already set */
	if (l1_tte_type != TTBL_L1TBL_TTE_TYPE_FAULT) {
		/* we need to check that the requested area is not already
		 * mapped
		 */
#if defined(CONFIG_ARMV5)
		if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_COARSE_L2TBL) {
#else
		if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_L2TBL) {
#endif
			minpgsz = TTBL_L2TBL_SMALL_PAGE_SIZE;
		} else {
			minpgsz = TTBL_L1TBL_SECTION_PAGE_SIZE;
			rc = VMM_EFAIL;
			goto mmu_map_return;
		}
		pgva = pg->va & ~(pg->sz - 1);
		pgva = pgva & ~(minpgsz - 1);
		pgsz = pg->sz;
		while (pgsz) {
			if (!cpu_mmu_get_page(l1, pgva, &upg)) {
				rc = VMM_EFAIL;
				goto mmu_map_return;
			}
			pgva += minpgsz;
			pgsz = (pgsz < minpgsz) ? 0 : (pgsz - minpgsz);
		}
	}

	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_FAULT) {
		/* The L1 is not already set */
		switch (pg->sz) {
		case TTBL_L2TBL_LARGE_PAGE_SIZE:	/* 64K Large Page */
		case TTBL_L2TBL_SMALL_PAGE_SIZE:	/* 4K Small Page */
			/* If small page requested, then alloc a level 2 TBL */
			if (!(l2 = cpu_mmu_l2tbl_alloc())) {
				rc = VMM_EFAIL;
				goto mmu_map_return;
			}
			/* And attach it to the L1 TBL */
			rc = cpu_mmu_l2tbl_attach(l1, l2, pg->imp, pg->dom,
						  pg->va, FALSE);
			if (rc) {
				goto mmu_map_return;
			}
			break;
		default:
			break;
		};
	}

	/* Now set up the mapping based on requested page size */
#if defined(CONFIG_ARMV5)
	switch (pg->sz) {
	case TTBL_L1TBL_SECTION_PAGE_SIZE:	/* 1M Section Page */
		*l1_tte = TTBL_L1TBL_TTE_REQ_MASK;
		*l1_tte |= (pg->pa & TTBL_L1TBL_TTE_BASE20_MASK);
		*l1_tte |=
		    (pg->dom << TTBL_L1TBL_TTE_DOM_SHIFT) &
		    TTBL_L1TBL_TTE_DOM_MASK;
		*l1_tte |=
		    (pg->ap << TTBL_L1TBL_TTE_AP_SHIFT) &
		    TTBL_L1TBL_TTE_AP_MASK;
		*l1_tte |=
		    (pg->c << TTBL_L1TBL_TTE_C_SHIFT) & TTBL_L1TBL_TTE_C_MASK;
		*l1_tte |=
		    (pg->b << TTBL_L1TBL_TTE_B_SHIFT) & TTBL_L1TBL_TTE_B_MASK;
		*l1_tte |= TTBL_L1TBL_TTE_TYPE_SECTION;
		cpu_mmu_sync_tte(l1_tte);
		l1->tte_cnt++;
		break;
	case TTBL_L2TBL_LARGE_PAGE_SIZE:	/* 64K Large Page */
	case TTBL_L2TBL_SMALL_PAGE_SIZE:	/* 4K Small Page */
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		if (!(l2 = cpu_mmu_l2tbl_find_tbl_pa(l2base))) {
			rc = VMM_EFAIL;
			goto mmu_map_return;
		}
		l2_tte =
		    (u32 *) ((pg->va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
			     TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
		if (pg->sz == TTBL_L2TBL_LARGE_PAGE_SIZE) {
			l2_tte -= ((u32) l2_tte % 64) >> 2;
			*l2_tte = (pg->pa & TTBL_L2TBL_TTE_BASE16_MASK);
			*l2_tte |= TTBL_L2TBL_TTE_TYPE_LARGE;
		} else {
			*l2_tte = (pg->pa & TTBL_L2TBL_TTE_BASE12_MASK);
			*l2_tte |= TTBL_L2TBL_TTE_TYPE_SMALL;
		}
		*l2_tte |=
		    (pg->ap << TTBL_L2TBL_TTE_V5_AP0_SHIFT) &
		    TTBL_L2TBL_TTE_V5_AP0_MASK;
		*l2_tte |=
		    (pg->ap << TTBL_L2TBL_TTE_V5_AP1_SHIFT) &
		    TTBL_L2TBL_TTE_V5_AP1_MASK;
		*l2_tte |=
		    (pg->ap << TTBL_L2TBL_TTE_V5_AP2_SHIFT) &
		    TTBL_L2TBL_TTE_V5_AP2_MASK;
		*l2_tte |=
		    (pg->ap << TTBL_L2TBL_TTE_V5_AP3_SHIFT) &
		    TTBL_L2TBL_TTE_V5_AP3_MASK;
		*l2_tte |=
		    (pg->c << TTBL_L2TBL_TTE_C_SHIFT) & TTBL_L2TBL_TTE_C_MASK;
		*l2_tte |=
		    (pg->b << TTBL_L2TBL_TTE_B_SHIFT) & TTBL_L2TBL_TTE_B_MASK;
		cpu_mmu_sync_tte(l2_tte);
		l2->tte_cnt++;
		if (pg->sz == TTBL_L2TBL_LARGE_PAGE_SIZE) {
			for (ite = 1; ite < 16; ite++) {
				l2_tte[ite] = l2_tte[0];
				cpu_mmu_sync_tte(&l2_tte[ite]);
				l2->tte_cnt++;
			}
		}
		break;
	default:
		break;
	};
#else
	switch (pg->sz) {
	case TTBL_L1TBL_SUPSECTION_PAGE_SIZE:	/* 16M Super Section Page */
	case TTBL_L1TBL_SECTION_PAGE_SIZE:	/* 1M Section Page */
		if (pg->sz == TTBL_L1TBL_SECTION_PAGE_SIZE) {
			*l1_tte = 0x0;
			*l1_tte |= (pg->pa & TTBL_L1TBL_TTE_BASE20_MASK);
			*l1_tte |= (pg->dom << TTBL_L1TBL_TTE_DOM_SHIFT) &
			    TTBL_L1TBL_TTE_DOM_MASK;
		} else {
			l1_tte = l1_tte - ((u32)l1_tte % 64) / 4;
			*l1_tte = 0x0;
			*l1_tte |= (pg->pa & TTBL_L1TBL_TTE_BASE24_MASK);
			*l1_tte |= (0x1 << TTBL_L1TBL_TTE_SECTYPE_SHIFT);
		}
		*l1_tte |= (pg->ns << TTBL_L1TBL_TTE_NS2_SHIFT) &
		    TTBL_L1TBL_TTE_NS2_MASK;
		*l1_tte |= (pg->ng << TTBL_L1TBL_TTE_NG_SHIFT) &
		    TTBL_L1TBL_TTE_NG_MASK;
		*l1_tte |= (pg->s << TTBL_L1TBL_TTE_S_SHIFT) &
		    TTBL_L1TBL_TTE_S_MASK;
		*l1_tte |= (pg->ap << (TTBL_L1TBL_TTE_AP2_SHIFT - 2)) &
		    TTBL_L1TBL_TTE_AP2_MASK;
		*l1_tte |= (pg->tex << TTBL_L1TBL_TTE_TEX_SHIFT) &
		    TTBL_L1TBL_TTE_TEX_MASK;
		*l1_tte |= (pg->ap << TTBL_L1TBL_TTE_AP_SHIFT) &
		    TTBL_L1TBL_TTE_AP_MASK;
		*l1_tte |= (pg->imp << TTBL_L1TBL_TTE_IMP_SHIFT) &
		    TTBL_L1TBL_TTE_IMP_MASK;
		*l1_tte |= (pg->xn << TTBL_L1TBL_TTE_XN_SHIFT) &
		    TTBL_L1TBL_TTE_XN_MASK;
		*l1_tte |= (pg->c << TTBL_L1TBL_TTE_C_SHIFT) &
		    TTBL_L1TBL_TTE_C_MASK;
		*l1_tte |= (pg->b << TTBL_L1TBL_TTE_B_SHIFT) &
		    TTBL_L1TBL_TTE_B_MASK;
		*l1_tte |= TTBL_L1TBL_TTE_TYPE_SECTION;
		cpu_mmu_sync_tte(l1_tte);
		l1->tte_cnt++;
		if (pg->sz == TTBL_L1TBL_SUPSECTION_PAGE_SIZE) {
			for (ite = 1; ite < 16; ite++) {
				l1_tte[ite] = l1_tte[0];
				cpu_mmu_sync_tte(&l1_tte[ite]);
				l1->tte_cnt++;
			}
		}
		break;
	case TTBL_L2TBL_LARGE_PAGE_SIZE:	/* 64K Large Page */
	case TTBL_L2TBL_SMALL_PAGE_SIZE:	/* 4K Small Page */
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		l2_tte = (u32 *) ((pg->va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2 = cpu_mmu_l2tbl_find_tbl_pa(l2base);
		if (l2) {
			l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
			if (pg->sz == TTBL_L2TBL_LARGE_PAGE_SIZE) {
				l2_tte = l2_tte - ((u32)l2_tte % 64) / 4;
				*l2_tte = 0x0;
				*l2_tte |= (pg->pa & TTBL_L2TBL_TTE_BASE16_MASK);
				*l2_tte |= TTBL_L2TBL_TTE_TYPE_LARGE;
				*l2_tte |= (pg->xn << TTBL_L2TBL_TTE_LXN_SHIFT) &
						TTBL_L2TBL_TTE_LXN_MASK;
				*l2_tte |= (pg->tex << TTBL_L2TBL_TTE_LTEX_SHIFT) &
						TTBL_L2TBL_TTE_LTEX_MASK;
			} else {
				*l2_tte = 0x0;
				*l2_tte |= (pg->pa & TTBL_L2TBL_TTE_BASE12_MASK);
				if (pg->xn) {
					*l2_tte |= TTBL_L2TBL_TTE_TYPE_SMALL_XN;
				} else {
					*l2_tte |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
				}
				*l2_tte |= (pg->tex << TTBL_L2TBL_TTE_STEX_SHIFT) &
						TTBL_L2TBL_TTE_STEX_MASK;
			}
			*l2_tte |= (pg->ng << TTBL_L2TBL_TTE_NG_SHIFT) &
			    TTBL_L2TBL_TTE_NG_MASK;
			*l2_tte |= (pg->s << TTBL_L2TBL_TTE_S_SHIFT) &
			    TTBL_L2TBL_TTE_S_MASK;
			*l2_tte |= (pg->ap << (TTBL_L2TBL_TTE_AP2_SHIFT - 2)) &
			    TTBL_L2TBL_TTE_AP2_MASK;
			*l2_tte |= (pg->ap << TTBL_L2TBL_TTE_AP_SHIFT) &
			    TTBL_L2TBL_TTE_AP_MASK;
			*l2_tte |= (pg->c << TTBL_L2TBL_TTE_C_SHIFT) &
			    TTBL_L2TBL_TTE_C_MASK;
			*l2_tte |= (pg->b << TTBL_L2TBL_TTE_B_SHIFT) &
			    TTBL_L2TBL_TTE_B_MASK;
			cpu_mmu_sync_tte(l2_tte);
			l2->tte_cnt++;
			if (pg->sz == TTBL_L2TBL_LARGE_PAGE_SIZE) {
				for (ite = 1; ite < 16; ite++) {
					l2_tte[ite] = l2_tte[0];
					cpu_mmu_sync_tte(&l2_tte[ite]);
					l2->tte_cnt++;
				}
			}
		} else {
			rc = VMM_EFAIL;
			goto mmu_map_return;
		}
		break;
	default:
		break;
	};
#endif

mmu_map_return:
	return rc;
}

static int cpu_mmu_split_reserved_page(struct cpu_page *pg, virtual_size_t rsize)
{
	int rc = VMM_EFAIL;
	int i, count;
	u32 *l2_tte;
	struct cpu_l1tbl *l1;
	struct cpu_l2tbl *l2;
	virtual_addr_t va;
	physical_addr_t pa;

	if (pg == NULL) {
		goto error;
	}

	l1 = &mmuctrl.defl1;

#if defined(CONFIG_ARMV5)
	/* XXX Currently, this function handles only
	 *     Section -> Pages splitting case.
	 */
	/* TODO Add other cases:
	 *        Section      -> Large Pages
	 *        Large Page   -> Pages
	 */
	switch (pg->sz) {
	case TTBL_L1TBL_SECTION_PAGE_SIZE:
		switch (rsize) {
		case TTBL_L2TBL_SMALL_PAGE_SIZE:
			count = TTBL_L1TBL_SECTION_PAGE_SIZE /
			    TTBL_L2TBL_SMALL_PAGE_SIZE;

			if (!(l2 = cpu_mmu_l2tbl_alloc())) {
				rc = VMM_EFAIL;
				goto error;
			}

			va = pg->va;
			pa = pg->pa;

			for (i = 0; i < count; i++) {
				l2_tte = (u32 *)
				    ((va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				     TTBL_L2TBL_TTE_OFFSET_SHIFT);
				l2_tte =
				    (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));

				*l2_tte = pa & TTBL_L2TBL_TTE_BASE12_MASK;
				*l2_tte |= TTBL_L2TBL_TTE_TYPE_SMALL;
				*l2_tte |=
				    (pg->ap << TTBL_L2TBL_TTE_V5_AP0_SHIFT) &
				    TTBL_L2TBL_TTE_V5_AP0_MASK;
				*l2_tte |=
				    (pg->ap << TTBL_L2TBL_TTE_V5_AP1_SHIFT) &
				    TTBL_L2TBL_TTE_V5_AP1_MASK;
				*l2_tte |=
				    (pg->ap << TTBL_L2TBL_TTE_V5_AP2_SHIFT) &
				    TTBL_L2TBL_TTE_V5_AP2_MASK;
				*l2_tte |=
				    (pg->ap << TTBL_L2TBL_TTE_V5_AP3_SHIFT) &
				    TTBL_L2TBL_TTE_V5_AP3_MASK;
				*l2_tte |=
				    (pg->c << TTBL_L2TBL_TTE_C_SHIFT) &
				    TTBL_L2TBL_TTE_C_MASK;
				*l2_tte |=
				    (pg->b << TTBL_L2TBL_TTE_B_SHIFT) &
				    TTBL_L2TBL_TTE_B_MASK;

				cpu_mmu_sync_tte(l2_tte);

				l2->tte_cnt++;

				va += TTBL_L2TBL_SMALL_PAGE_SIZE;
				pa += TTBL_L2TBL_SMALL_PAGE_SIZE;
			}

			cpu_mmu_l2tbl_attach(l1, l2, 0, pg->dom, pg->va, TRUE);

			invalid_tlb();

			break;
		default:
			vmm_printf("%s: Unimplemented (target size 0x%x)\n",
			       __func__, rsize);
			BUG();
			break;
		}
		break;
	default:
		vmm_printf("%s: Unimplemented (source size 0x%x)\n", __func__,
		       pg->sz);
		BUG();
		break;
	}
#else
	/* XXX Currently, this function handles only
	 *     Section -> Pages splitting case.
	 */
	/* TODO Add other cases:
	 *        Supersection -> Sections
	 *        Supersection -> Large Pages
	 *        Supersection -> Pages
	 *        Section      -> Large Pages
	 *        Large Page   -> Pages
	 */
	switch (pg->sz) {
	case TTBL_L1TBL_SECTION_PAGE_SIZE:
		switch (rsize) {
		case TTBL_L2TBL_SMALL_PAGE_SIZE:
			count = TTBL_L1TBL_SECTION_PAGE_SIZE /
				TTBL_L2TBL_SMALL_PAGE_SIZE;

			l2 = cpu_mmu_l2tbl_alloc();
			if (l2 == NULL) {
				rc = VMM_EFAIL;
				goto error;
			}
			va = pg->va;
			pa = pg->pa;

			for (i = 0; i < count; i++) {
				l2_tte = (u32 *)
					 ((va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
					  TTBL_L2TBL_TTE_OFFSET_SHIFT);
				l2_tte = (u32 *)(l2->tbl_va + ((u32)l2_tte << 2));

				*l2_tte = pa & TTBL_L2TBL_TTE_BASE12_MASK;
				*l2_tte |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
				*l2_tte |= (pg->tex << TTBL_L2TBL_TTE_STEX_SHIFT) &
						TTBL_L2TBL_TTE_STEX_MASK;
				*l2_tte |= (pg->ng << TTBL_L2TBL_TTE_NG_SHIFT) &
						TTBL_L2TBL_TTE_NG_MASK;
				*l2_tte |= (pg->s << TTBL_L2TBL_TTE_S_SHIFT) &
						TTBL_L2TBL_TTE_S_MASK;
				*l2_tte |= (pg->ap << (TTBL_L2TBL_TTE_AP2_SHIFT - 2)) &
						TTBL_L2TBL_TTE_AP2_MASK;
				*l2_tte |= (pg->ap << TTBL_L2TBL_TTE_AP_SHIFT) &
						TTBL_L2TBL_TTE_AP_MASK;
				*l2_tte |= (pg->c << TTBL_L2TBL_TTE_C_SHIFT) &
						TTBL_L2TBL_TTE_C_MASK;
				*l2_tte |= (pg->b << TTBL_L2TBL_TTE_B_SHIFT) &
						TTBL_L2TBL_TTE_B_MASK;
				cpu_mmu_sync_tte(l2_tte);
				l2->tte_cnt++;

				va += TTBL_L2TBL_SMALL_PAGE_SIZE;
				pa += TTBL_L2TBL_SMALL_PAGE_SIZE;
			}

			cpu_mmu_l2tbl_attach(l1, l2, pg->imp, pg->dom, pg->va,
					     TRUE);
			invalid_tlb();
			dsb();
			isb();
			break;
		default:
			vmm_printf("%s: Unimplemented (target size 0x%x)\n",
					__func__, rsize);
			BUG();
			break;
		}
		break;
	default:
		vmm_printf("%s: Unimplemented (source size 0x%x)\n", __func__,
				pg->sz);
		BUG();
		break;
	}
#endif

	rc = VMM_OK;
error:
	return rc;
}

int cpu_mmu_get_reserved_page(virtual_addr_t va, struct cpu_page *pg)
{
	int rc;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&mmuctrl.defl1_lock, flags);
	rc = cpu_mmu_get_page(&mmuctrl.defl1, va, pg);
	vmm_spin_unlock_irqrestore(&mmuctrl.defl1_lock, flags);

	return rc;
}

int cpu_mmu_unmap_reserved_page(struct cpu_page *pg)
{
	int rc;
	irq_flags_t flags;
	struct dlist *l;
	struct cpu_l1tbl *l1;

	if (!pg) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&mmuctrl.defl1_lock, flags);
	if ((rc = cpu_mmu_unmap_page(&mmuctrl.defl1, pg))) {
		vmm_spin_unlock_irqrestore(&mmuctrl.defl1_lock, flags);
		return rc;
	}
	vmm_spin_unlock_irqrestore(&mmuctrl.defl1_lock, flags);

	/* Note: It might be possible that the reserved page
	 * was mapped on-demand into l1 tables other than
	 * default l1 table so, we should try to remove mappings
	 * of this page from other l1 tables.
	 */

	vmm_spin_lock_irqsave(&mmuctrl.l1_alloc_lock, flags);

	list_for_each(l, &mmuctrl.l1tbl_list) {
		l1 = list_entry(l, struct cpu_l1tbl, head);
		rc = cpu_mmu_unmap_page(l1, pg);
	}

	vmm_spin_unlock_irqrestore(&mmuctrl.l1_alloc_lock, flags);

	return VMM_OK;
}

int cpu_mmu_map_reserved_page(struct cpu_page *pg)
{
	int rc;
	irq_flags_t flags;

	if (!pg) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&mmuctrl.defl1_lock, flags);
	if ((rc = cpu_mmu_map_page(&mmuctrl.defl1, pg))) {
		vmm_spin_unlock_irqrestore(&mmuctrl.defl1_lock, flags);
		return rc;
	}
	vmm_spin_unlock_irqrestore(&mmuctrl.defl1_lock, flags);

	/* Note: Ideally resereved page mapping should be created
	 * in each and every l1 table that exist, but that would
	 * be uneccesary and redundant. To avoid this we only
	 * create mapping in default l1 table and let other VCPUs
	 * fault on this page and load the page on-demand from
	 * data abort and prefetch abort handlers.
	 */

	return VMM_OK;
}

struct cpu_l1tbl *cpu_mmu_l1tbl_alloc(void)
{
	u32 i, *nl1_tte;
	irq_flags_t flags;
	struct cpu_l1tbl *nl1 = NULL;
	struct dlist *le;
	struct cpu_l2tbl *l2, *nl2;

	vmm_spin_lock_irqsave(&mmuctrl.l1_alloc_lock, flags);
	if (list_empty(&mmuctrl.free_l1tbl_list)) {
		vmm_spin_unlock_irqrestore(&mmuctrl.l1_alloc_lock, flags);
		return NULL;
	}
	nl1 = list_entry(list_first(&mmuctrl.free_l1tbl_list),
			 struct cpu_l1tbl, head);
	if (!nl1) {
		vmm_spin_unlock_irqrestore(&mmuctrl.l1_alloc_lock, flags);
		return NULL;
	}
	list_del(&nl1->head);
	mmuctrl.l1_alloc_count++;
	nl1->contextid = mmuctrl.l1_next_contextid;
	mmuctrl.l1_next_contextid++;
	vmm_spin_unlock_irqrestore(&mmuctrl.l1_alloc_lock, flags);

	INIT_LIST_HEAD(&nl1->l2tbl_list);
	nl1->tte_cnt = 0;
	nl1->l2tbl_cnt = 0;

	vmm_spin_lock_irqsave(&mmuctrl.defl1_lock, flags);

	for (i = 0; i < (TTBL_L1TBL_SIZE / 4); i++) {
		((u32 *)nl1->tbl_va)[i] = ((u32 *)mmuctrl.defl1.tbl_va)[i];
		cpu_mmu_sync_tte(&((u32 *)nl1->tbl_va)[i]);
	}
	nl1->tte_cnt = mmuctrl.defl1.tte_cnt;

	list_for_each(le, &mmuctrl.defl1.l2tbl_list) {
		l2 = list_entry(le, struct cpu_l2tbl, head);
		nl1_tte = (u32 *) (nl1->tbl_va +
			  ((l2->map_va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
		*nl1_tte = 0x0;
		cpu_mmu_sync_tte(nl1_tte);
		nl1->tte_cnt--;
		nl2 = cpu_mmu_l2tbl_alloc();
		if (!nl2) {
			vmm_spin_unlock_irqrestore(&mmuctrl.defl1_lock, flags);
			goto l1tbl_alloc_fail;
		}
		for (i = 0; i < (TTBL_L2TBL_SIZE / 4); i++) {
			((u32 *)nl2->tbl_va)[i] = ((u32 *)l2->tbl_va)[i];
			cpu_mmu_sync_tte(&((u32 *)nl2->tbl_va)[i]);
		}
		nl2->tte_cnt = l2->tte_cnt;
		if (cpu_mmu_l2tbl_attach
		    (nl1, nl2, l2->imp, l2->domain, l2->map_va, FALSE)) {
			vmm_spin_unlock_irqrestore(&mmuctrl.defl1_lock, flags);
			goto l1tbl_alloc_fail;
		}
	}
	nl1->l2tbl_cnt = mmuctrl.defl1.l2tbl_cnt;

	vmm_spin_unlock_irqrestore(&mmuctrl.defl1_lock, flags);

	vmm_spin_lock_irqsave(&mmuctrl.l1_alloc_lock, flags);
	list_add(&nl1->head, &mmuctrl.l1tbl_list);
	vmm_spin_unlock_irqrestore(&mmuctrl.l1_alloc_lock, flags);

	return nl1;

l1tbl_alloc_fail:
	while (!list_empty(&nl1->l2tbl_list)) {
		le = list_first(&nl1->l2tbl_list);
		nl2 = list_entry(le, struct cpu_l2tbl, head);
		cpu_mmu_l2tbl_free(nl2);
	}

	vmm_spin_lock_irqsave(&mmuctrl.l1_alloc_lock, flags);
	list_add_tail(&nl1->head, &mmuctrl.free_l1tbl_list);
	mmuctrl.l1_alloc_count--;
	vmm_spin_unlock_irqrestore(&mmuctrl.l1_alloc_lock, flags);

	return NULL;
}

int cpu_mmu_l1tbl_free(struct cpu_l1tbl *l1)
{
	irq_flags_t flags;
	struct dlist *le;
	struct cpu_l2tbl *l2;

	if (!l1) {
		return VMM_EFAIL;
	}
	if (l1->tbl_pa == mmuctrl.defl1.tbl_pa) {
		return VMM_EFAIL;
	}

	while (!list_empty(&l1->l2tbl_list)) {
		le = list_first(&l1->l2tbl_list);
		l2 = list_entry(le, struct cpu_l2tbl, head);
		cpu_mmu_l2tbl_free(l2);
	}

	vmm_spin_lock_irqsave(&mmuctrl.l1_alloc_lock, flags);
	list_del(&l1->head);
	list_add_tail(&l1->head, &mmuctrl.free_l1tbl_list);
	mmuctrl.l1_alloc_count--;
	vmm_spin_unlock_irqrestore(&mmuctrl.l1_alloc_lock, flags);

	return VMM_OK;
}

struct cpu_l1tbl *cpu_mmu_l1tbl_default(void)
{
	return &mmuctrl.defl1;
}

struct cpu_l1tbl *cpu_mmu_l1tbl_current(void)
{
	u32 ttbr0;

	ttbr0 = read_ttbr0();

	return cpu_mmu_l1tbl_find_tbl_pa(ttbr0);
}

int cpu_mmu_change_dacr(u32 new_dacr)
{
	u32 old_dacr;

	old_dacr = read_dacr();
	isb();

	new_dacr &= ~0x3;
	new_dacr |= old_dacr & 0x3;

	if (new_dacr != old_dacr) {
		write_dacr(new_dacr);
		isb();
	}

	return VMM_OK;
}

int cpu_mmu_change_ttbr(struct cpu_l1tbl *l1)
{
	struct cpu_l1tbl * curr_l1;

	if (!l1) {
		return VMM_EFAIL;
	}

	/* Do nothing if new l1 table is already current l1 table */
	curr_l1 = cpu_mmu_l1tbl_current();
	if (curr_l1 == l1) {
		return VMM_OK;
	}

	/* Call low-level MMU switch */
	proc_mmu_switch(l1->tbl_pa, l1->contextid & 0xFF);

	return VMM_OK;
}

int cpu_mmu_sync_ttbr(struct cpu_l1tbl *l1)
{
	if (!l1) {
		return VMM_EFAIL;
	}

#if defined(CONFIG_ARMV5)
	invalid_tlb();
#else
	invalid_tlb_asid(l1->contextid & 0xFF);
#endif	
	isb();

	return VMM_OK;
}

int cpu_mmu_sync_ttbr_va(struct cpu_l1tbl *l1, virtual_addr_t va)
{
	if (!l1) {
		return VMM_EFAIL;
	}

	invalid_tlb_mva(va);
	isb();

	return VMM_OK;
}

#if defined(CONFIG_ARMV5)

int arch_cpu_aspace_memory_read(virtual_addr_t tmp_va, 
			   	physical_addr_t src, 
				void *dst, u32 len, bool cacheable)
{
	int rc;
	struct cpu_page p;
	struct cpu_l1tbl *l1 = NULL;

	l1 = cpu_mmu_l1tbl_current();
	if (!l1) {
		return VMM_EFAIL;
	}

	p.pa = src & ~VMM_PAGE_MASK;
	p.va = tmp_va;
	p.sz = VMM_PAGE_SIZE;
	p.imp = 0;
	p.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
	p.ap = TTBL_AP_SRW_U;
	p.xn = 1;
	p.tex = 0;
	p.c = (cacheable) ? 1 : 0;
	p.b = (cacheable) ? 1 : 0;
	p.ng = 0;
	p.s = 0;

	if ((rc = cpu_mmu_map_page(l1, &p))) {
		return rc;
	}

	switch (len) {
	case 1:
		*((u8 *)dst) = *(u8 *)(tmp_va + (src & VMM_PAGE_MASK));
		break;
	case 2:
		*((u16 *)dst) = *(u16 *)(tmp_va + (src & VMM_PAGE_MASK));
		break;
	case 4:
		*((u32 *)dst) = *(u32 *)(tmp_va + (src & VMM_PAGE_MASK));
		break;
	default:
		memcpy(dst, (void *)(tmp_va + (src & VMM_PAGE_MASK)), len);
		break;
	};

	if ((rc = cpu_mmu_unmap_page(l1, &p))) {
		return rc;
	}

	return VMM_OK;
}

int arch_cpu_aspace_memory_write(virtual_addr_t tmp_va, 
				 physical_addr_t dst, 
				 void *src, u32 len, bool cacheable)
{
	int rc;
	struct cpu_page p;
	struct cpu_l1tbl *l1 = NULL;

	l1 = cpu_mmu_l1tbl_current();
	if (!l1) {
		return VMM_EFAIL;
	}

	p.pa = dst & ~VMM_PAGE_MASK;
	p.va = tmp_va;
	p.sz = VMM_PAGE_SIZE;
	p.imp = 0;
	p.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
	p.ap = TTBL_AP_SRW_U;
	p.xn = 1;
	p.tex = 0;
	p.c = (cacheable) ? 1 : 0;
	p.b = (cacheable) ? 1 : 0;
	p.ng = 0;
	p.s = 0;

	if ((rc = cpu_mmu_map_page(l1, &p))) {
		return rc;
	}

	switch (len) {
	case 1:
		*(u8 *)(tmp_va + (dst & VMM_PAGE_MASK)) = *((u8 *)src);
		break;
	case 2:
		*(u16 *)(tmp_va + (dst & VMM_PAGE_MASK)) = *((u16 *)src);
		break;
	case 4:
		*(u32 *)(tmp_va + (dst & VMM_PAGE_MASK)) = *((u32 *)src);
		break;
	default:
		memcpy((void *)(tmp_va + (dst & VMM_PAGE_MASK)), src, len);
		break;
	};

	if ((rc = cpu_mmu_unmap_page(l1, &p))) {
		return rc;
	}

	return VMM_OK;
}

#else

#define PHYS_RW_L1_TTE	(((TTBL_L1TBL_TTE_DOM_RESERVED <<		\
			   TTBL_L1TBL_TTE_DOM_SHIFT) &			\
			  TTBL_L1TBL_TTE_DOM_MASK)		|	\
			 ((0x0 << TTBL_L1TBL_TTE_NS2_SHIFT) &		\
			  TTBL_L1TBL_TTE_NS2_MASK)		|	\
			 ((0x0 << TTBL_L1TBL_TTE_NG_SHIFT) &		\
			  TTBL_L1TBL_TTE_NG_MASK)		|	\
			 ((0x0 << TTBL_L1TBL_TTE_S_SHIFT) &		\
			  TTBL_L1TBL_TTE_S_MASK)		|	\
			 ((TTBL_AP_SRW_U << 				\
			   (TTBL_L1TBL_TTE_AP2_SHIFT - 2)) &		\
			  TTBL_L1TBL_TTE_AP2_MASK)		|	\
			 ((0x0 << TTBL_L1TBL_TTE_TEX_SHIFT) &		\
			  TTBL_L1TBL_TTE_TEX_MASK)		|	\
			 ((TTBL_AP_SRW_U << TTBL_L1TBL_TTE_AP_SHIFT) &	\
			  TTBL_L1TBL_TTE_AP_MASK)		|	\
			 ((0x0 << TTBL_L1TBL_TTE_IMP_SHIFT) &		\
			  TTBL_L1TBL_TTE_IMP_MASK)		|	\
			 ((0x0 << TTBL_L1TBL_TTE_XN_SHIFT) &		\
			  TTBL_L1TBL_TTE_XN_MASK)		|	\
			 (TTBL_L1TBL_TTE_TYPE_SECTION))

#define PHYS_RW_L1_TTE_NOCACHE	\
			 (PHYS_RW_L1_TTE)

#define PHYS_RW_L1_TTE_CACHE	\
			 (PHYS_RW_L1_TTE |				\
			  ((0x1 << TTBL_L1TBL_TTE_C_SHIFT) &		\
			    TTBL_L1TBL_TTE_C_MASK)		|	\
			  ((0x1 << TTBL_L1TBL_TTE_B_SHIFT) &		\
			    TTBL_L1TBL_TTE_B_MASK))

#define PHYS_RW_L2_TTE	((TTBL_L2TBL_TTE_TYPE_SMALL_XN)		|	\
			 ((0x0 << TTBL_L2TBL_TTE_STEX_SHIFT) &		\
			  TTBL_L2TBL_TTE_STEX_MASK)		|	\
			 ((0x0 << TTBL_L2TBL_TTE_NG_SHIFT) &		\
			    TTBL_L2TBL_TTE_NG_MASK)		|	\
			 ((0x0 << TTBL_L2TBL_TTE_S_SHIFT) &		\
			    TTBL_L2TBL_TTE_S_MASK)		|	\
			 ((TTBL_AP_SRW_U << (TTBL_L2TBL_TTE_AP2_SHIFT - 2)) & \
			    TTBL_L2TBL_TTE_AP2_MASK)		|	\
			 ((TTBL_AP_SRW_U << TTBL_L2TBL_TTE_AP_SHIFT) &	\
			    TTBL_L2TBL_TTE_AP_MASK))

#define PHYS_RW_L2_TTE_NOCACHE	\
			 (PHYS_RW_L2_TTE)

#define PHYS_RW_L2_TTE_CACHE	\
			 (PHYS_RW_L2_TTE |				\
			  ((0x1 << TTBL_L2TBL_TTE_C_SHIFT) &		\
			     TTBL_L2TBL_TTE_C_MASK)		|	\
			  ((0x1 << TTBL_L2TBL_TTE_B_SHIFT) &		\
			     TTBL_L2TBL_TTE_B_MASK))

int arch_cpu_aspace_memory_read(virtual_addr_t tmp_va, 
				physical_addr_t src, 
				void *dst, u32 len, bool cacheable)
{
	u32 *l1_tte = NULL, *l2_tte = NULL;
	u32 l1_tte_type;
	physical_addr_t l2base;
	struct cpu_l1tbl *l1 = NULL;
	struct cpu_l2tbl *l2 = NULL;

	l1 = cpu_mmu_l1tbl_current();
	if (!l1) {
		return VMM_EFAIL;
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((tmp_va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));

	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	if (l1_tte_type != TTBL_L1TBL_TTE_TYPE_FAULT) {
		if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_L2TBL) {
			l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
			l2 = cpu_mmu_l2tbl_find_tbl_pa(l2base);
			if (!l2) {
				return VMM_EFAIL;
			}
			l2_tte = (u32 *) ((tmp_va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
			l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
		} else {
			return VMM_EFAIL;
		}
	}

	if (l2_tte) {
		if (cacheable) {
			*l2_tte = PHYS_RW_L2_TTE_CACHE;
		} else {
			*l2_tte = PHYS_RW_L2_TTE_NOCACHE;
		}
		*l2_tte |= (src & TTBL_L2TBL_TTE_BASE12_MASK);
		cpu_mmu_sync_tte(l2_tte);
	} else {
		if (cacheable) {
			*l1_tte = PHYS_RW_L1_TTE_CACHE;
		} else {
			*l1_tte = PHYS_RW_L1_TTE_NOCACHE;
		}
		*l1_tte |= (src & TTBL_L1TBL_TTE_BASE20_MASK);
		cpu_mmu_sync_tte(l1_tte);
	}

	switch (len) {
	case 1:
		*((u8 *)dst) = *(u8 *)(tmp_va + (src & VMM_PAGE_MASK));
		break;
	case 2:
		*((u16 *)dst) = *(u16 *)(tmp_va + (src & VMM_PAGE_MASK));
		break;
	case 4:
		*((u32 *)dst) = *(u32 *)(tmp_va + (src & VMM_PAGE_MASK));
		break;
	default:
		memcpy(dst, (void *)(tmp_va + (src & VMM_PAGE_MASK)), len);
		break;
	};

	if (l2_tte) {
		*l2_tte = 0x0;
		cpu_mmu_sync_tte(l2_tte);
	} else {
		*l1_tte = 0x0;
		cpu_mmu_sync_tte(l1_tte);
	}
	invalid_tlb_mva(tmp_va);
	dsb();
	isb();

	return VMM_OK;
}

int arch_cpu_aspace_memory_write(virtual_addr_t tmp_va, 
				 physical_addr_t dst, 
				 void *src, u32 len, bool cacheable)
{
	u32 *l1_tte = NULL, *l2_tte = NULL;
	u32 l1_tte_type;
	physical_addr_t l2base;
	struct cpu_l1tbl *l1 = NULL;
	struct cpu_l2tbl *l2 = NULL;

	l1 = cpu_mmu_l1tbl_current();
	if (!l1) {
		return VMM_EFAIL;
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((tmp_va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));

	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	if (l1_tte_type != TTBL_L1TBL_TTE_TYPE_FAULT) {
		if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_L2TBL) {
			l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
			l2 = cpu_mmu_l2tbl_find_tbl_pa(l2base);
			if (!l2) {
				return VMM_EFAIL;
			}
			l2_tte = (u32 *) ((tmp_va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
			l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
		} else {
			return VMM_EFAIL;
		}
	}

	if (l2_tte) {
		*l2_tte = PHYS_RW_L2_TTE;
		*l2_tte |= (dst & TTBL_L2TBL_TTE_BASE12_MASK);
		cpu_mmu_sync_tte(l2_tte);
	} else {
		*l1_tte = PHYS_RW_L1_TTE;
		*l1_tte |= (dst & TTBL_L1TBL_TTE_BASE20_MASK);
		cpu_mmu_sync_tte(l1_tte);
	}

	switch (len) {
	case 1:
		*(u8 *)(tmp_va + (dst & VMM_PAGE_MASK)) = *((u8 *)src);
		break;
	case 2:
		*(u16 *)(tmp_va + (dst & VMM_PAGE_MASK)) = *((u16 *)src);
		break;
	case 4:
		*(u32 *)(tmp_va + (dst & VMM_PAGE_MASK)) = *((u32 *)src);
		break;
	default:
		memcpy((void *)(tmp_va + (dst & VMM_PAGE_MASK)), src, len);
		break;
	};

	if (l2_tte) {
		*l2_tte = 0x0;
		cpu_mmu_sync_tte(l2_tte);
	} else {
		*l1_tte = 0x0;
		cpu_mmu_sync_tte(l1_tte);
	}
	invalid_tlb_mva(tmp_va);
	dsb();
	isb();

	return VMM_OK;
}

#endif

int arch_cpu_aspace_map(virtual_addr_t page_va, 
			physical_addr_t page_pa,
			u32 mem_flags)
{
	struct cpu_page p;
	memset(&p, 0, sizeof(p));

	/* Initialize the page struct */
#if defined(CONFIG_ARMV5)
	p.pa = page_pa;
	p.va = page_va;
	p.sz = VMM_PAGE_SIZE;
	p.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
	/* For ARMV5 we cannot prevent writing to priviledge mode */
	if (mem_flags & (VMM_MEMORY_READABLE | VMM_MEMORY_WRITEABLE)) {
		p.ap = TTBL_AP_SRW_U;
	} else {
		p.ap = TTBL_AP_S_U;
	}
	p.c = (mem_flags & VMM_MEMORY_CACHEABLE) ? 1 : 0;
	p.b = (mem_flags & VMM_MEMORY_BUFFERABLE) ? 1 : 0;
#else
	p.pa = page_pa;
	p.va = page_va;
	p.sz = VMM_PAGE_SIZE;
	p.imp = 0;
	p.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
	if (mem_flags & VMM_MEMORY_WRITEABLE) {
		p.ap = TTBL_AP_SRW_U;
	} else if (mem_flags & VMM_MEMORY_READABLE) {
		p.ap = TTBL_AP_SR_U;
	} else {
		p.ap = TTBL_AP_S_U;
	}
	p.xn = (mem_flags & VMM_MEMORY_EXECUTABLE) ? 0 : 1;
	p.tex = 0;
	p.c = (mem_flags & VMM_MEMORY_CACHEABLE) ? 1 : 0;
	p.b = (mem_flags & VMM_MEMORY_BUFFERABLE) ? 1 : 0;;
	p.ng = 0;
	p.s = 0;
#endif

	return cpu_mmu_map_reserved_page(&p);
}

int arch_cpu_aspace_unmap(virtual_addr_t page_va)
{
	int rc;
	struct cpu_page p;

	rc = cpu_mmu_get_reserved_page(page_va, &p);
	if (rc) {
		return rc;
	}

	if (p.sz > VMM_PAGE_SIZE) {
		rc = cpu_mmu_split_reserved_page(&p, VMM_PAGE_SIZE);
		if (rc) {
			return rc;
		}
		rc = cpu_mmu_get_reserved_page(page_va, &p);
		if (rc) {
			return rc;
		}
	}

	return cpu_mmu_unmap_reserved_page(&p);
}

int arch_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t *pa)
{
	int rc;
	struct cpu_page p;

	if ((rc = cpu_mmu_get_reserved_page(va, &p)) == VMM_OK) {
		*pa = p.pa + (va & (p.sz - 1));
	}

	return rc;
}

int __init arch_cpu_aspace_primary_init(physical_addr_t *core_resv_pa, 
					virtual_addr_t *core_resv_va,
					virtual_size_t *core_resv_sz,
					physical_addr_t *arch_resv_pa,
					virtual_addr_t *arch_resv_va,
					virtual_size_t *arch_resv_sz)
{
	int rc = VMM_EFAIL;
	u32 i, val;
	virtual_addr_t va, resv_va = *core_resv_va;
	virtual_size_t sz, resv_sz = *core_resv_sz;
	physical_addr_t pa, resv_pa = *core_resv_pa;
	struct cpu_page respg;

	/* Reset the memory of MMU control structure */
	memset(&mmuctrl, 0, sizeof(mmuctrl));

	/* Initialize list heads */
	INIT_SPIN_LOCK(&mmuctrl.defl1_lock);
	INIT_SPIN_LOCK(&mmuctrl.l1_alloc_lock);
	INIT_LIST_HEAD(&mmuctrl.l1tbl_list);
	INIT_LIST_HEAD(&mmuctrl.free_l1tbl_list);
	INIT_SPIN_LOCK(&mmuctrl.l2_alloc_lock);
	INIT_LIST_HEAD(&mmuctrl.free_l2tbl_list);

	/* Copy default (or master) ttbl from temporary ttbl */
	memcpy(defl1_mem, tmpl1_mem, TTBL_L1TBL_SIZE);
	clean_invalidate_dcache_mva_range((virtual_addr_t)defl1_mem, 
			(virtual_addr_t)defl1_mem + TTBL_L1TBL_SIZE);

	/* Handcraft default (or master) ttbl */
	INIT_LIST_HEAD(&mmuctrl.defl1.l2tbl_list);
	mmuctrl.defl1.num = TTBL_MAX_L1TBL_COUNT;
	mmuctrl.defl1.contextid = mmuctrl.l1_next_contextid;
	mmuctrl.l1_next_contextid++;
	mmuctrl.defl1.tbl_va = (virtual_addr_t)&defl1_mem;
	mmuctrl.defl1.tbl_pa = arch_code_paddr_start() + 
			       ((virtual_addr_t)&defl1_mem - arch_code_vaddr_start());

	/* Switch over to default (or master) ttbl
	 * Note: Low-level code will set temporary ttbl during boot-up
	 */
	proc_mmu_switch(mmuctrl.defl1.tbl_pa, mmuctrl.defl1.contextid & 0xFF);

	/* Remove all TLB enteries to start using default (or master) ttbl */
	invalid_tlb();

	/* If possible remove boot-time identity mappings */
	if (arch_code_paddr_start() != arch_code_vaddr_start()) {
		val = arch_code_paddr_start() >> TTBL_L1TBL_TTE_OFFSET_SHIFT;
		val = val << 2;
		*((u32 *)(mmuctrl.defl1.tbl_va + val)) = 0x0;
		invalid_tlb();
		dsb();
		isb();
	}

	/* Sync count default (or master) ttbl enteries */
	mmuctrl.defl1.tte_cnt = 0;
	for (i = 0; i < TTBL_L1TBL_SIZE; i += 4) {
		val = *((u32 *)(mmuctrl.defl1.tbl_va + i));
		if ((val & TTBL_L1TBL_TTE_TYPE_MASK) != 
				TTBL_L1TBL_TTE_TYPE_FAULT) {
			mmuctrl.defl1.tte_cnt++;
		}
	}
	mmuctrl.defl1.l2tbl_cnt = 0;

	/* Check & setup core reserved space and update the 
	 * core_resv_pa, core_resv_va, and core_resv_sz parameters
	 * to inform host aspace about correct placement of the
	 * core reserved space.
	 */
	pa = arch_code_paddr_start();
	va = arch_code_vaddr_start();
	sz = arch_code_size();
	if ((va <= resv_va) && (resv_va < (va + sz))) {
		resv_va = va + sz;
	} else if ((va <= (resv_va + resv_sz)) && 
		   ((resv_va + resv_sz) < (va + sz))) {
		resv_va = va + sz;
	}
	if ((pa <= resv_pa) && (resv_pa < (pa + sz))) {
		resv_pa = pa + sz;
	} else if ((pa <= (resv_pa + resv_sz)) && 
		   ((resv_pa + resv_sz) < (pa + sz))) {
		resv_pa = pa + sz;
	}
	if (resv_va & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) {
		resv_va += TTBL_L1TBL_SECTION_PAGE_SIZE - 
			    (resv_va & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1));
	}
	if (resv_pa & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) {
		resv_pa += TTBL_L1TBL_SECTION_PAGE_SIZE - 
			    (resv_pa & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1));
	}
	*core_resv_pa = resv_pa;
	*core_resv_va = resv_va;
	*core_resv_sz = resv_sz;

	/* Allocate arch reserved space and update the *arch_resv_pa, 
	 * *arch_resv_va, and *arch_resv_sz parameters to inform host
	 * aspace about the arch reserved space.
	 */
	*arch_resv_va = (resv_va + resv_sz);
	*arch_resv_pa = (resv_pa + resv_sz);
	*arch_resv_sz = resv_sz;
	resv_sz = (resv_sz & 0x3) ? (resv_sz & ~0x3) + 0x4 : resv_sz;
	mmuctrl.l1_array = (struct cpu_l1tbl *)(resv_va + resv_sz);
	resv_sz += sizeof(struct cpu_l1tbl) * TTBL_MAX_L1TBL_COUNT;
	resv_sz = (resv_sz & 0x3) ? (resv_sz & ~0x3) + 0x4 : resv_sz;
	mmuctrl.l2_array = (struct cpu_l2tbl *)(resv_va + resv_sz);
	resv_sz += sizeof(struct cpu_l2tbl) * TTBL_MAX_L2TBL_COUNT;
	resv_sz = (resv_sz & 0x3) ? (resv_sz & ~0x3) + 0x4 : resv_sz;
	if (resv_sz & (TTBL_L1TBL_SIZE - 1)) {
		resv_sz += TTBL_L1TBL_SIZE - 
			    (resv_sz & (TTBL_L1TBL_SIZE - 1));
	}
	mmuctrl.l1_base_va = resv_va + resv_sz;
	mmuctrl.l1_base_pa = resv_pa + resv_sz;
	resv_sz += TTBL_L1TBL_SIZE * TTBL_MAX_L1TBL_COUNT;
	mmuctrl.l2_base_va = resv_va + resv_sz;
	mmuctrl.l2_base_pa = resv_pa + resv_sz;
	resv_sz += TTBL_L2TBL_SIZE * TTBL_MAX_L2TBL_COUNT;
	if (resv_sz & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) {
		resv_sz += TTBL_L1TBL_SECTION_PAGE_SIZE - 
			    (resv_sz & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1));
	}
	*arch_resv_sz = resv_sz - *arch_resv_sz;
	
	/* Map reserved space (core reserved space + arch reserved space) 
	 * We have kept our page table pool in reserved area pages
	 * as cacheable and write-back. We will clean data cache every
	 * time we modify a page table (or translation table) entry.
	 */
	pa = resv_pa;
	va = resv_va;
	sz = resv_sz;
	while (sz) {
		memset(&respg, 0, sizeof(respg));
#if defined(CONFIG_ARMV5)
		respg.pa = pa;
		respg.va = va;
		respg.sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		respg.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
		respg.ap = TTBL_AP_SRW_U;
		respg.c = 1;
		respg.b = 1;
#else
		respg.pa = pa;
		respg.va = va;
		respg.sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		respg.imp = 0;
		respg.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
		respg.ap = TTBL_AP_SRW_U;
		respg.xn = 0;
		respg.tex = 0;
		respg.c = 1;
		respg.b = 1;
		respg.s = 0;
		respg.ng = 0;
#endif
		if ((rc = cpu_mmu_map_reserved_page(&respg))) {
			goto mmu_init_error;
		}
		sz -= TTBL_L1TBL_SECTION_PAGE_SIZE;
		pa += TTBL_L1TBL_SECTION_PAGE_SIZE;
		va += TTBL_L1TBL_SECTION_PAGE_SIZE;
	}

	/* Setup up l1 array */
	memset(mmuctrl.l1_array, 0x0, 
		   sizeof(struct cpu_l1tbl) * TTBL_MAX_L1TBL_COUNT);
	for (i = 0; i < TTBL_MAX_L1TBL_COUNT; i++) {
		INIT_LIST_HEAD(&mmuctrl.l1_array[i].head);
		mmuctrl.l1_array[i].num = i;
		mmuctrl.l1_array[i].tbl_pa = mmuctrl.l1_base_pa + 
						i * TTBL_L1TBL_SIZE;
		mmuctrl.l1_array[i].tbl_va = mmuctrl.l1_base_va + 
						i * TTBL_L1TBL_SIZE;
		mmuctrl.l1_array[i].tte_cnt = 0;
		mmuctrl.l1_array[i].l2tbl_cnt = 0;
		INIT_LIST_HEAD(&mmuctrl.l1_array[i].l2tbl_list);
		list_add_tail(&mmuctrl.l1_array[i].head,
			      &mmuctrl.free_l1tbl_list);
	}

	/* Setup up l2 array */
	memset(mmuctrl.l2_array, 0x0, 
		   sizeof(struct cpu_l2tbl) * TTBL_MAX_L2TBL_COUNT);
	for (i = 0; i < TTBL_MAX_L2TBL_COUNT; i++) {
		INIT_LIST_HEAD(&mmuctrl.l2_array[i].head);
		mmuctrl.l2_array[i].num = i;
		mmuctrl.l2_array[i].tbl_pa = mmuctrl.l2_base_pa + 
						i * TTBL_L2TBL_SIZE;
		mmuctrl.l2_array[i].tbl_va = mmuctrl.l2_base_va + 
						i * TTBL_L2TBL_SIZE;
		mmuctrl.l2_array[i].tte_cnt = 0;
		list_add_tail(&mmuctrl.l2_array[i].head,
			      &mmuctrl.free_l2tbl_list);
	}

	return VMM_OK;

mmu_init_error:
	return rc;
}

int __cpuinit arch_cpu_aspace_secondary_init(void)
{
	/* Switch over to default (or master) ttbl
	 * Note: Low-level code will set temporary ttbl during boot-up
	 */
	proc_mmu_switch(mmuctrl.defl1.tbl_pa, mmuctrl.defl1.contextid & 0xFF);

	/* Remove all TLB enteries to start using default (or master) ttbl */
	invalid_tlb();

	return VMM_OK;
}

