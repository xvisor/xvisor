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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for memory management unit
 */

#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <cpu_defines.h>
#include <cpu_inline_asm.h>
#include <cpu_mmu.h>

cpu_mmu_ctrl_t mmuctrl;

/** Allocate memory in chunks of TTLB_MIN_SIZE from MMU Pool */
int cpu_mmu_pool_alloc(virtual_size_t pool_sz,
		       physical_addr_t * pool_pa, virtual_addr_t * pool_va)
{
	u32 i, found, bcnt, bpos, bfree;

	bcnt = 0;
	while (pool_sz > 0) {
		bcnt++;
		if (pool_sz > TTBL_MIN_SIZE) {
			pool_sz -= TTBL_MIN_SIZE;
		} else {
			pool_sz = 0;
		}
	}

	found = 0;
	for (bpos = 0; bpos < (mmuctrl.pool_sz / TTBL_MIN_SIZE); bpos += bcnt) {
		bfree = 0;
		for (i = bpos; i < (bpos + bcnt); i++) {
			if (mmuctrl.pool_bmap[i / 32] &
			    (0x1 << (31 - (i % 32)))) {
				break;
			}
			bfree++;
		}
		if (bfree == bcnt) {
			found = 1;
			break;
		}
	}
	if (!found) {
		return VMM_EFAIL;
	}

	*pool_pa = mmuctrl.pool_pa + bpos * TTBL_MIN_SIZE;
	*pool_va = mmuctrl.pool_va + bpos * TTBL_MIN_SIZE;
	for (i = bpos; i < (bpos + bcnt); i++) {
		mmuctrl.pool_bmap[i / 32] |= (0x1 << (31 - (i % 32)));
	}

	return VMM_OK;
}

/** Free memory back to MMU Pool */
int cpu_mmu_pool_free(virtual_addr_t pool_va, virtual_addr_t pool_sz)
{
	u32 i, bcnt, bpos;

	if (pool_va < mmuctrl.pool_va ||
	    (mmuctrl.pool_va + mmuctrl.pool_sz) <= pool_va) {
		return VMM_EFAIL;
	}

	bcnt = 0;
	while (pool_sz > 0) {
		bcnt++;
		if (pool_sz > TTBL_MIN_SIZE) {
			pool_sz -= TTBL_MIN_SIZE;
		} else {
			pool_sz = 0;
		}
	}

	bpos = (pool_va - mmuctrl.pool_va) / TTBL_MIN_SIZE;

	for (i = bpos; i < (bpos + bcnt); i++) {
		mmuctrl.pool_bmap[i / 32] &= ~(0x1 << (31 - (i % 32)));
	}

	return VMM_OK;
}

/** Find L2 page table at given physical address from L1 page table */
cpu_l2tbl_t *cpu_mmu_l2tbl_find_tbl_pa(cpu_l1tbl_t * l1, physical_addr_t tbl_pa)
{
	struct dlist *lentry;
	cpu_l2tbl_t *l2;

	if (!l1) {
		return NULL;
	}

	list_for_each(lentry, &l1->l2tbl_list) {
		l2 = list_entry(lentry, cpu_l2tbl_t, head);
		if (l2->tbl_pa == tbl_pa) {
			return l2;
		}
	}

	return NULL;
}

/** Check whether a L2 page table is attached or not */
int cpu_mmu_l2tbl_is_attached(cpu_l2tbl_t * l2)
{
	if (!l2) {
		return 0;
	}
	return (l2->l1) ? 1 : 0;
}

/** Detach a L2 page table */
int cpu_mmu_l2tbl_detach(cpu_l2tbl_t * l2)
{
	u32 *l1_tte;
	u32 l1_tte_type;

	if (!l2) {
		return VMM_EFAIL;
	}
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
	l2->l1->tte_cnt--;
	l2->l1->l2tbl_cnt--;
	l2->l1 = NULL;
	l2->tte_cnt = 0;
	vmm_memset((void *)l2->tbl_va, 0, TTBL_L2TBL_SIZE);

	list_del(&l2->head);
	list_add(&mmuctrl.l2tbl_list, &l2->head);

	return VMM_OK;
}

/** Attach a L2 page table to a particular L1 page table */
int cpu_mmu_l2tbl_attach(cpu_l1tbl_t * l1, cpu_l2tbl_t * l2, u32 new_imp,
			 u32 new_domain, virtual_addr_t new_map_va)
{
	int rc;
	u32 *l1_tte;
	u32 l1_tte_type;

	if (!l2 || !l1) {
		return VMM_EFAIL;
	}

	if (cpu_mmu_l2tbl_is_attached(l2)) {
		rc = cpu_mmu_l2tbl_detach(l2);
		if (rc) {
			return rc;
		}
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((new_map_va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	if (l1_tte_type != TTBL_L1TBL_TTE_TYPE_FAULT) {
		return VMM_EFAIL;
	}

	l2->l1 = l1;
	l2->imp =
	    new_imp & (TTBL_L1TBL_TTE_IMP_MASK >> TTBL_L1TBL_TTE_IMP_SHIFT);
	l2->domain =
	    new_domain & (TTBL_L1TBL_TTE_DOM_MASK >> TTBL_L1TBL_TTE_DOM_SHIFT);
	l2->map_va = new_map_va & TTBL_L1TBL_TTE_OFFSET_MASK;

	*l1_tte = 0x0;
	*l1_tte |= (l2->imp) << TTBL_L1TBL_TTE_IMP_SHIFT;
	*l1_tte |= (l2->domain) << TTBL_L1TBL_TTE_DOM_SHIFT;
	*l1_tte |= (l2->tbl_pa & TTBL_L1TBL_TTE_BASE10_MASK);
	*l1_tte |= TTBL_L1TBL_TTE_TYPE_L2TBL;
	l1->tte_cnt++;
	l1->l2tbl_cnt++;

	list_del(&l2->head);
	list_add(&l1->l2tbl_list, &l2->head);

	return VMM_OK;
}

/** Allocate a L2 page table of given type */
cpu_l2tbl_t *cpu_mmu_l2tbl_alloc(void)
{
	physical_addr_t l2_pa;
	virtual_addr_t l2_va;
	cpu_l2tbl_t *l2;

	if (!list_empty(&mmuctrl.l2tbl_list)) {
		return list_entry(mmuctrl.l2tbl_list.next, cpu_l2tbl_t, head);
	}

	if (cpu_mmu_pool_alloc(TTBL_L2TBL_SIZE, &l2_pa, &l2_va)) {
		return NULL;
	}

	l2 = vmm_malloc(sizeof(cpu_l2tbl_t));
	l2->l1 = NULL;
	l2->imp = 0;
	l2->domain = 0;
	l2->tbl_pa = l2_pa;
	l2->tbl_va = l2_va;
	l2->map_va = 0;
	l2->tte_cnt = 0;
	vmm_memset((void *)l2->tbl_va, 0, TTBL_L2TBL_SIZE);

	list_add(&mmuctrl.l2tbl_list, &l2->head);

	return l2;
}

/** Free a L2 page table */
int cpu_mmu_l2tbl_free(cpu_l2tbl_t * l2)
{
	int rc;

	if (!l2) {
		return VMM_EFAIL;
	}

	if (cpu_mmu_l2tbl_is_attached(l2)) {
		rc = cpu_mmu_l2tbl_detach(l2);
		if (rc) {
			return rc;
		}
	}

	cpu_mmu_pool_free(l2->tbl_va, TTBL_L2TBL_SIZE);

	list_del(&l2->head);

	vmm_free(l2);

	return VMM_OK;
}

int cpu_mmu_get_page(cpu_l1tbl_t * l1, virtual_addr_t va, cpu_page_t * pg)
{
	int ret = VMM_EFAIL;
	u32 *l1_tte, *l2_tte;
	u32 l1_tte_type, l1_sec_type;
	physical_addr_t l2base;
	cpu_l2tbl_t *l2;
	cpu_page_t r;

	if (!l1) {
		return VMM_EFAIL;
	}

	if (!pg) {
		pg = &r;
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	l1_sec_type = (*l1_tte & TTBL_L1TBL_TTE_SECTYPE_MASK) >>
	    TTBL_L1TBL_TTE_SECTYPE_SHIFT;
	vmm_memset(pg, 0, sizeof(cpu_page_t));

	switch (l1_tte_type) {
	case TTBL_L1TBL_TTE_TYPE_FAULT:
		break;
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
		l2 = cpu_mmu_l2tbl_find_tbl_pa(l1, l2base);
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
		ret = VMM_ENOTAVAIL;
		break;
	};

	return ret;
}

int cpu_mmu_unmap_page(cpu_l1tbl_t * l1, cpu_page_t * pg)
{
	int ret = VMM_EFAIL;
	u32 *l1_tte, *l2_tte;
	u32 l1_tte_type, chkimp, found = 0;
	physical_addr_t l2base, chkpa;
	virtual_size_t chksz;
	cpu_l2tbl_t *l2 = NULL;

	if (!l1 || !pg) {
		return ret;
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((pg->va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	chkimp = *l1_tte & TTBL_L1TBL_TTE_IMP_MASK;
	chkimp = chkimp >> TTBL_L1TBL_TTE_IMP_SHIFT;

	found = 0;
	switch (l1_tte_type) {
	case TTBL_L1TBL_TTE_TYPE_FAULT:
		break;
	case TTBL_L1TBL_TTE_TYPE_SECTION:
		chkpa = *l1_tte & TTBL_L1TBL_TTE_BASE20_MASK;
		chksz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		found = 1;
		break;
	case TTBL_L1TBL_TTE_TYPE_L2TBL:
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		l2_tte = (u32 *) ((pg->va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2 = cpu_mmu_l2tbl_find_tbl_pa(l1, l2base);
		if (l2) {
			l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
			switch (*l2_tte & TTBL_L2TBL_TTE_TYPE_MASK) {
			case TTBL_L2TBL_TTE_TYPE_LARGE:
				chkpa = *l2_tte & TTBL_L2TBL_TTE_BASE16_MASK;
				chksz = TTBL_L2TBL_LARGE_PAGE_SIZE;
				found = 2;
				break;
			case TTBL_L2TBL_TTE_TYPE_SMALL_X:
			case TTBL_L2TBL_TTE_TYPE_SMALL_XN:
				chkpa = *l2_tte & TTBL_L2TBL_TTE_BASE12_MASK;
				chksz = TTBL_L2TBL_SMALL_PAGE_SIZE;
				found = 2;
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
	case 1:
		if (pg->sz == chksz && pg->imp == chkimp) {
			*l1_tte = 0x0;
			l1->tte_cnt--;
			ret = VMM_OK;
		}
		break;
	case 2:
		if (pg->sz == chksz && pg->imp == chkimp) {
			*l2_tte = 0x0;
			l2->tte_cnt--;
			if (!l2->tte_cnt) {
				cpu_mmu_l2tbl_detach(l2);
			}
			ret = VMM_OK;
		}
		break;
	default:
		break;
	}

	if (!ret) {
		/* If given L1 page table is current then 
		 * invalidate tlb line 
		 */
		if(read_ttbr0() == l1->tbl_pa) {
			invalid_tlb_line(pg->va);
		}
	}

	return ret;
}

int cpu_mmu_map_page(cpu_l1tbl_t * l1, cpu_page_t * pg)
{
	int rc = VMM_OK;
	u32 *l1_tte, *l2_tte;
	u32 l1_tte_type, l1_tte_imp;
	virtual_addr_t pgva;
	virtual_size_t pgsz, minpgsz;
	physical_addr_t l2base;
	cpu_page_t upg;
	cpu_l2tbl_t *l2;

	if (!l1 || !pg) {
		rc = VMM_EFAIL;
		goto mmu_map_return;
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((pg->va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));

	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	l1_tte_imp = *l1_tte & TTBL_L1TBL_TTE_IMP_MASK;
	l1_tte_imp = l1_tte_imp >> TTBL_L1TBL_TTE_IMP_SHIFT;
	if (l1_tte_type != TTBL_L1TBL_TTE_TYPE_FAULT) {
		if (l1_tte_imp && pg->imp != l1_tte_imp) {
			rc = VMM_EFAIL;
			goto mmu_map_return;
		}
		if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_L2TBL) {
			minpgsz = TTBL_L2TBL_SMALL_PAGE_SIZE;
		} else {
			minpgsz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		}
		pgva = pg->va & ~(minpgsz - 1);
		pgsz = pg->sz;
		while (pgsz) {
			if (cpu_mmu_get_page(l1, pgva, &upg)) {
				pgva += minpgsz;
				pgsz = (pgsz < minpgsz) ? 0 : (pgsz - minpgsz);
			} else {
				if ((rc = cpu_mmu_unmap_page(l1, &upg))) {
					goto mmu_map_return;
				}
				pgva += upg.sz;
				pgsz = (pgsz < upg.sz) ? 0 : (pgsz - upg.sz);
			}
		}
	}

	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_FAULT) {
		switch (pg->sz) {
		case TTBL_L2TBL_LARGE_PAGE_SIZE:	/* 64K Large Page */
		case TTBL_L2TBL_SMALL_PAGE_SIZE:	/* 4K Small Page */
			if (!(l2 = cpu_mmu_l2tbl_alloc())) {
				rc = VMM_EFAIL;
				goto mmu_map_return;
			}
			rc = cpu_mmu_l2tbl_attach(l1, l2, pg->imp, pg->dom,
						  pg->va);
			if (rc) {
				goto mmu_map_return;
			}
			break;
		default:
			break;
		};
	}

	switch (pg->sz) {
	case TTBL_L1TBL_SUPSECTION_PAGE_SIZE:	/* 16M Super Section Page */
	case TTBL_L1TBL_SECTION_PAGE_SIZE:	/* 1M Section Page */
		*l1_tte = 0x0;
		if (pg->sz == TTBL_L1TBL_SECTION_PAGE_SIZE) {
			*l1_tte |= (pg->pa & TTBL_L1TBL_TTE_BASE20_MASK);
			*l1_tte |= (pg->dom << TTBL_L1TBL_TTE_DOM_SHIFT) &
			    TTBL_L1TBL_TTE_DOM_MASK;
		} else {
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
		l1->tte_cnt++;
		break;
	case TTBL_L2TBL_LARGE_PAGE_SIZE:	/* 64K Large Page */
		break;
	case TTBL_L2TBL_SMALL_PAGE_SIZE:	/* 4K Small Page */
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		l2_tte = (u32 *) ((pg->va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2 = cpu_mmu_l2tbl_find_tbl_pa(l1, l2base);
		if (l2) {
			l2_tte = (u32 *) (l2->tbl_va + ((u32) l2_tte << 2));
			*l2_tte |= (pg->pa & TTBL_L2TBL_TTE_BASE12_MASK);
			*l2_tte |= (pg->ap << TTBL_L2TBL_TTE_NG_SHIFT) &
			    TTBL_L2TBL_TTE_NG_MASK;
			*l2_tte |= (pg->ap << TTBL_L2TBL_TTE_S_SHIFT) &
			    TTBL_L2TBL_TTE_S_MASK;
			*l2_tte |= (pg->ap << (TTBL_L2TBL_TTE_AP2_SHIFT - 2)) &
			    TTBL_L2TBL_TTE_AP2_MASK;
			*l2_tte |= (pg->tex << TTBL_L2TBL_TTE_STEX_SHIFT) &
			    TTBL_L2TBL_TTE_STEX_MASK;
			*l2_tte |= (pg->ap << TTBL_L2TBL_TTE_AP_SHIFT) &
			    TTBL_L2TBL_TTE_AP_MASK;
			*l2_tte |= (pg->c << TTBL_L2TBL_TTE_C_SHIFT) &
			    TTBL_L2TBL_TTE_C_MASK;
			*l2_tte |= (pg->b << TTBL_L2TBL_TTE_B_SHIFT) &
			    TTBL_L2TBL_TTE_B_MASK;
			if (pg->xn) {
				*l2_tte |= TTBL_L2TBL_TTE_TYPE_SMALL_XN;
			} else {
				*l2_tte |= TTBL_L2TBL_TTE_TYPE_SMALL_X;
			}
			l2->tte_cnt++;
		} else {
			rc = VMM_EFAIL;
			goto mmu_map_return;
		}
		break;
	default:
		break;
	};

mmu_map_return:
	return rc;
}

int cpu_mmu_get_reserved_page(virtual_addr_t va, cpu_page_t * pg)
{
	return cpu_mmu_get_page(mmuctrl.defl1, va, pg);
}

int cpu_mmu_unmap_reserved_page(cpu_page_t * pg)
{
	int rc;
	struct dlist *le;
	cpu_l1tbl_t *l1;

	if (!pg) {
		return VMM_EFAIL;
	}
	if (pg->imp != 0x1) {
		return VMM_EFAIL;
	}

	if ((rc = cpu_mmu_unmap_page(mmuctrl.defl1, pg))) {
		return rc;
	}
	list_for_each(le, &mmuctrl.l1tbl_list) {
		l1 = list_entry(le, cpu_l1tbl_t, head);
		if ((rc = cpu_mmu_unmap_page(l1, pg))) {
			return rc;
		}
	}

	return VMM_OK;
}

int cpu_mmu_map_reserved_page(cpu_page_t * pg)
{
	int rc;
	struct dlist *le;
	cpu_l1tbl_t *l1;

	if (!pg) {
		return VMM_EFAIL;
	}
	if (pg->imp != 0x1) {
		return VMM_EFAIL;
	}

	if ((rc = cpu_mmu_map_page(mmuctrl.defl1, pg))) {
		return rc;
	}
	list_for_each(le, &mmuctrl.l1tbl_list) {
		l1 = list_entry(le, cpu_l1tbl_t, head);
		if ((rc = cpu_mmu_map_page(l1, pg))) {
			return rc;
		}
	}

	return VMM_OK;
}

cpu_l1tbl_t *cpu_mmu_l1tbl_alloc(void)
{
	u32 *nl1_tte;
	cpu_l1tbl_t *nl1 = NULL;
	virtual_addr_t nl1_va;
	physical_addr_t nl1_pa;
	struct dlist *le;
	cpu_l2tbl_t *l2, *nl2;

	if (cpu_mmu_pool_alloc(TTBL_L1TBL_SIZE, &nl1_pa, &nl1_va)) {
		goto l1tbl_alloc_fail;
	}

	nl1 = vmm_malloc(sizeof(cpu_l1tbl_t));

	INIT_LIST_HEAD(&nl1->l2tbl_list);
	nl1->tbl_pa = nl1_pa;
	nl1->tbl_va = nl1_va;
	nl1->tte_cnt = 0;
	nl1->l2tbl_cnt = 0;

	vmm_memcpy((void *)nl1->tbl_va,
		   (void *)mmuctrl.defl1->tbl_va, TTBL_L1TBL_SIZE);
	nl1->tte_cnt = mmuctrl.defl1->tte_cnt;

	list_for_each(le, &mmuctrl.defl1->l2tbl_list) {
		l2 = list_entry(le, cpu_l2tbl_t, head);
		nl1_tte = (u32 *) (nl1->tbl_va +
			  ((l2->map_va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
		*nl1_tte = 0x0;
		nl1->tte_cnt--;
		nl2 = cpu_mmu_l2tbl_alloc();
		if (!nl2) {
			goto l1tbl_alloc_fail;
		}
		vmm_memcpy((void *)nl2->tbl_va, (void *)l2->tbl_va,
			   TTBL_L2TBL_SIZE);
		nl2->tte_cnt = l2->tte_cnt;
		if (cpu_mmu_l2tbl_attach
		    (nl1, nl2, l2->imp, l2->domain, l2->map_va)) {
			goto l1tbl_alloc_fail;
		}
	}
	nl1->l2tbl_cnt = mmuctrl.defl1->l2tbl_cnt;

	list_add(&mmuctrl.l1tbl_list, &nl1->head);

	return nl1;

l1tbl_alloc_fail:
	if (nl1) {
		while (!list_empty(&nl1->l2tbl_list)) {
			le = list_pop(&nl1->l2tbl_list);
			nl2 = list_entry(le, cpu_l2tbl_t, head);
			cpu_mmu_l2tbl_free(nl2);
		}
		cpu_mmu_pool_free(nl1->tbl_va, TTBL_L1TBL_SIZE);
		vmm_free(nl1);
	}

	return NULL;
}

int cpu_mmu_l1tbl_free(cpu_l1tbl_t * l1)
{
	struct dlist *le;
	cpu_l2tbl_t *l2;

	if (!l1) {
		return VMM_EFAIL;
	}

	while (!list_empty(&l1->l2tbl_list)) {
		le = list_pop(&l1->l2tbl_list);
		l2 = list_entry(le, cpu_l2tbl_t, head);
		cpu_mmu_l2tbl_free(l2);
	}

	cpu_mmu_pool_free(l1->tbl_va, TTBL_L1TBL_SIZE);

	list_del(&l1->head);

	vmm_free(l1);

	return VMM_OK;
}

int cpu_mmu_chdacr(u32 new_dacr)
{
	u32 old_dacr;

	old_dacr = read_dacr();

	new_dacr &= ~0x3;
	new_dacr |= old_dacr & 0x3;

	write_dacr(new_dacr);

	return VMM_OK;
}

int cpu_mmu_chttbr(cpu_l1tbl_t * l1)
{
	u32 sctlr;

	if (!l1) {
		return VMM_EFAIL;
	}

	sctlr = read_sctlr();

	/* FIXME: Clean & Flush I-Cache if I-Cache enabled */
	if (sctlr & SCTLR_I_MASK) {
	}

	/* FIXME: Clean & Flush D-Cache if D-Cache enabled */
	if (sctlr & SCTLR_C_MASK) {
	}

	/* Invalidate all TLB enteries */
	invalid_tlb();

	/* Update TTBR0 to point to new L1 table */
	write_ttbr0(l1->tbl_pa);

	return VMM_OK;
}

int cpu_mmu_init(void)
{
	int rc = VMM_OK;
	u32 highvec_enable, dacr;
	vmm_devtree_node_t *node;
	virtual_addr_t defl1_va, va;
	virtual_size_t sz, tsz;
	physical_addr_t defl1_pa, pa;
	cpu_page_t respg;
	const char *attrval;
	extern u32 _code_start;
	extern u32 _code_end;

	/* Reset the memory of MMU control structure */
	vmm_memset(&mmuctrl, 0, sizeof(mmuctrl));
	mmuctrl.pool_bmap = NULL;
	mmuctrl.pool_pa = (physical_addr_t) & _code_end;
	if (mmuctrl.pool_pa & (TTBL_MAX_SIZE - 1)) {
		mmuctrl.pool_pa += TTBL_MAX_SIZE;
		mmuctrl.pool_pa &= ~(TTBL_MAX_SIZE - 1);
	}
	mmuctrl.pool_va = mmuctrl.pool_pa;
	mmuctrl.pool_sz = 0;
	INIT_LIST_HEAD(&mmuctrl.l1tbl_list);
	INIT_LIST_HEAD(&mmuctrl.l2tbl_list);
	mmuctrl.defl1 = NULL;

	/* Get the vmm information node */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!node) {
		rc = VMM_EFAIL;
		goto mmu_init_done;
	}

	/* Determine value of highvec_enable attribute */
	attrval = vmm_devtree_attrval(node, CPU_HIGHVEC_ENABLE_ATTR_NAME);
	if (!attrval) {
		rc = VMM_EFAIL;
		goto mmu_init_done;
	}
	highvec_enable = *((u32 *) attrval);

	/* Determine value of ttbl_pool_size attribute */
	attrval = vmm_devtree_attrval(node, MMU_POOL_SIZE_ATTR_NAME);
	if (!attrval) {
		rc = VMM_EFAIL;
		goto mmu_init_done;
	}
	mmuctrl.pool_sz = *((virtual_size_t *) attrval);
	mmuctrl.pool_bmap_len = (mmuctrl.pool_sz / (TTBL_MIN_SIZE * 32) + 1);
	mmuctrl.pool_bmap = vmm_malloc(sizeof(u32) * mmuctrl.pool_bmap_len);
	vmm_memset(mmuctrl.pool_bmap, 0, sizeof(u32) * mmuctrl.pool_bmap_len);

	/* Initialized domains (Dom0 for VMM) */
	dacr = TTBL_DOM_CLIENT;
	write_dacr(dacr);

	/* Handcraft default translation table */
	if (cpu_mmu_pool_alloc(TTBL_L1TBL_SIZE, &defl1_pa, &defl1_va)) {
		rc = VMM_EFAIL;
		goto mmu_init_done;
	}
	mmuctrl.defl1 = vmm_malloc(sizeof(cpu_l1tbl_t));
	if (!mmuctrl.defl1) {
		rc = VMM_EFAIL;
		goto mmu_init_done;
	}
	INIT_LIST_HEAD(&mmuctrl.defl1->l2tbl_list);
	mmuctrl.defl1->tbl_pa = defl1_pa;
	mmuctrl.defl1->tbl_va = defl1_va;
	vmm_memset((void *)mmuctrl.defl1->tbl_va, 0, TTBL_L1TBL_SIZE);
	mmuctrl.defl1->tte_cnt = 0;
	mmuctrl.defl1->l2tbl_cnt = 0;

	/* All MMU APIs available now */

	/* Reserve space for code/data + mmu pool */
	pa = (physical_addr_t) & _code_start;
	va = (virtual_addr_t) & _code_start;
	sz = (virtual_size_t) (&_code_end - &_code_start);
	sz = ((va + sz) < (mmuctrl.pool_va + mmuctrl.pool_sz)) ?
	    (mmuctrl.pool_va + mmuctrl.pool_sz) : (va + sz);
	pa = (pa < mmuctrl.pool_pa) ? pa : mmuctrl.pool_pa;
	va = (va < mmuctrl.pool_va) ? va : mmuctrl.pool_va;
	sz = sz - va;
	while (sz) {
		if (va & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) {
			tsz = va & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1);
			tsz = TTBL_L1TBL_SECTION_PAGE_SIZE - tsz;
			pa &= ~(TTBL_L1TBL_SECTION_PAGE_SIZE - 1);
			va &= ~(TTBL_L1TBL_SECTION_PAGE_SIZE - 1);
		} else {
			tsz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		}
		vmm_memset(&respg, 0, sizeof(respg));
		respg.pa = pa;
		respg.va = va;
		respg.sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		respg.imp = 1;
		respg.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
		respg.ap = TTBL_AP_SRW_U;
		respg.xn = 0;
		respg.c = 1;
		respg.b = 0;
		if ((rc = cpu_mmu_map_reserved_page(&respg))) {
			goto mmu_init_done;
		}
		sz = (sz < tsz) ? 0 : (sz - tsz);
		pa += TTBL_L1TBL_SECTION_PAGE_SIZE;
		va += TTBL_L1TBL_SECTION_PAGE_SIZE;
	}

	/* Reserve space for interrupt vectors */
	if (highvec_enable) {
		pa = (physical_addr_t) CPU_IRQ_HIGHVEC_BASE;
		va = (virtual_addr_t) CPU_IRQ_HIGHVEC_BASE;
		sz = (virtual_size_t) TTBL_L1TBL_SECTION_PAGE_SIZE;
	} else {
		pa = (physical_addr_t) CPU_IRQ_LOWVEC_BASE;
		va = (virtual_addr_t) CPU_IRQ_LOWVEC_BASE;
		sz = (virtual_size_t) TTBL_L1TBL_SECTION_PAGE_SIZE;
	}
	while (sz) {
		if (va & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) {
			tsz = va & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1);
			tsz = TTBL_L1TBL_SECTION_PAGE_SIZE - tsz;
			pa &= ~(TTBL_L1TBL_SECTION_PAGE_SIZE - 1);
			va &= ~(TTBL_L1TBL_SECTION_PAGE_SIZE - 1);
		} else {
			tsz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		}
		vmm_memset(&respg, 0, sizeof(respg));
		respg.pa = pa;
		respg.va = va;
		respg.sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		respg.imp = 1;
		respg.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
		respg.ap = TTBL_AP_SRW_U;
		respg.xn = 0;
		respg.c = 1;
		respg.b = 0;
		if ((rc = cpu_mmu_map_reserved_page(&respg))) {
			goto mmu_init_done;
		}
		sz = (sz < tsz) ? 0 : (sz - tsz);
		pa += TTBL_L1TBL_SECTION_PAGE_SIZE;
		va += TTBL_L1TBL_SECTION_PAGE_SIZE;
	}

	/* Change translation table base address to default L1 */
	if ((rc = cpu_mmu_chttbr(mmuctrl.defl1))) {
		goto mmu_init_done;
	}

	/* Enable MMU && Caches */
	write_sctlr(read_sctlr() |
		    (SCTLR_M_MASK | SCTLR_I_MASK | SCTLR_C_MASK));

mmu_init_done:
	return rc;
}
