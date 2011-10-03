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

#include <vmm_error.h>
#include <vmm_sections.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <vmm_cpu.h>
#include <cpu_defines.h>
#include <cpu_inline_asm.h>
#include <cpu_mmu.h>

#define TTBL_MAX_L1TBL_COUNT	(CONFIG_MAX_VCPU_COUNT + 1)

#define TTBL_MAX_L2TBL_COUNT	(CONFIG_ARMV7A_VTLB_ENTRY_COUNT * \
				 (CONFIG_MAX_VCPU_COUNT + 1))

u8 __attribute__((aligned(TTBL_L1TBL_SIZE))) defl1_mem[TTBL_L1TBL_SIZE];

struct cpu_mmu_ctrl {
	cpu_l1tbl_t defl1;
	virtual_addr_t l1_base_va;
	physical_addr_t l1_base_pa;
	cpu_l1tbl_t * l1_array;
	u8 * l1_bmap;
	u32 l1_alloc_count;
	virtual_addr_t l2_base_va;
	physical_addr_t l2_base_pa;
	cpu_l2tbl_t * l2_array;
	u8 * l2_bmap;
	u32 l2_alloc_count;
	struct dlist l1tbl_list;
	struct dlist l2tbl_list;
};

typedef struct cpu_mmu_ctrl cpu_mmu_ctrl_t;

cpu_mmu_ctrl_t mmuctrl;

/** Find L2 page table at given physical address from L1 page table */
cpu_l2tbl_t *cpu_mmu_l2tbl_find_tbl_pa(cpu_l1tbl_t * l1, 
				       physical_addr_t tbl_pa)
{
	u32 tmp;
	cpu_l2tbl_t *l2;

	if (!l1) {
		return NULL;
	}

	tmp = mmuctrl.l2_base_pa + TTBL_MAX_L2TBL_COUNT * TTBL_L2TBL_SIZE;
	if ((mmuctrl.l2_base_pa <= tbl_pa) && (tbl_pa < tmp)) {
		tmp = (tbl_pa - mmuctrl.l2_base_pa) / TTBL_L2TBL_SIZE;
		l2 = &mmuctrl.l2_array[tmp];
		if (l2->l1->l1_num == l1->l1_num) {
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
	u32 i;
	cpu_l2tbl_t *l2;

	if (!list_empty(&mmuctrl.l2tbl_list)) {
		return list_entry(mmuctrl.l2tbl_list.next, cpu_l2tbl_t, head);
	}

	if (mmuctrl.l2_alloc_count < TTBL_MAX_L2TBL_COUNT) {
		for (i = 0; i < TTBL_MAX_L2TBL_COUNT; i++) {
			if (!mmuctrl.l2_bmap[i]) {
				break;
			}
		}
		if (i == TTBL_MAX_L2TBL_COUNT) {
			return NULL;
		}
		mmuctrl.l2_bmap[i] = 1;
		mmuctrl.l2_alloc_count++;
		l2 = &mmuctrl.l2_array[i];
	} else {
		return NULL;
	}
	l2->l1 = NULL;
	l2->imp = 0;
	l2->domain = 0;
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

	list_del(&l2->head);

	mmuctrl.l2_bmap[l2->l2_num] = 0;
	mmuctrl.l2_alloc_count--;

	return VMM_OK;
}

/** Find L1 page table at given physical address */
cpu_l1tbl_t *cpu_mmu_l1tbl_find_tbl_pa(physical_addr_t tbl_pa)
{
	u32 tmp;

	if (tbl_pa == mmuctrl.defl1.tbl_pa) {
		return &mmuctrl.defl1;
	}

	tmp = mmuctrl.l1_base_pa + TTBL_MAX_L1TBL_COUNT * TTBL_L1TBL_SIZE;
	if ((mmuctrl.l1_base_pa <= tbl_pa) && (tbl_pa < tmp)) {
		tmp = (tbl_pa - mmuctrl.l1_base_pa) / TTBL_L1TBL_SIZE;
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
	u32 ite, *l1_tte, *l2_tte;
	u32 l1_tte_type, l1_sec_type, found = 0;
	physical_addr_t l2base, pgpa, chkpa;
	virtual_size_t chksz;
	cpu_l2tbl_t *l2 = NULL;

	if (!l1 || !pg) {
		return ret;
	}

	l1_tte = (u32 *) (l1->tbl_va +
			  ((pg->va >> TTBL_L1TBL_TTE_OFFSET_SHIFT) << 2));
	l1_tte_type = *l1_tte & TTBL_L1TBL_TTE_TYPE_MASK;
	l1_sec_type = (*l1_tte & TTBL_L1TBL_TTE_SECTYPE_MASK) >>
	    TTBL_L1TBL_TTE_SECTYPE_SHIFT;

	found = 0;
	switch (l1_tte_type) {
	case TTBL_L1TBL_TTE_TYPE_FAULT:
		break;
	case TTBL_L1TBL_TTE_TYPE_SECTION:
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
		l2 = cpu_mmu_l2tbl_find_tbl_pa(l1, l2base);
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
				l1->tte_cnt--;
			}
			ret = VMM_OK;
		}
		break;
	case 2: /* Section */
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
			*l1_tte = 0x0;
			l1->tte_cnt--;
			ret = VMM_OK;
		}
		break;
	case 3: /* Large Page */
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
			for (ite = 0; ite < 16; ite++) {
				l2_tte[ite] = 0x0;
				l2->tte_cnt--;				
			}
			if (!l2->tte_cnt) {
				cpu_mmu_l2tbl_detach(l2);
			}
			ret = VMM_OK;
		}
		break;
	case 4: /* Small Page */
		if ((pgpa == chkpa) && (pg->sz == chksz)) {
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
	u32 ite, l1_tte_type;
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
	if (l1_tte_type != TTBL_L1TBL_TTE_TYPE_FAULT) {
		if (l1_tte_type == TTBL_L1TBL_TTE_TYPE_L2TBL) {
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
			if (cpu_mmu_get_page(l1, pgva, &upg)) {
				pgva += minpgsz;
				pgsz = (pgsz < minpgsz) ? 0 : (pgsz - minpgsz);
			} else {
				rc = VMM_EFAIL;
				goto mmu_map_return;
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
		l1->tte_cnt++;
		if (pg->sz == TTBL_L1TBL_SUPSECTION_PAGE_SIZE) {
			for (ite = 1; ite < 16; ite++) {
				l1_tte[ite] = l1_tte[0];
				l1->tte_cnt++;
			}
		}
		break;
	case TTBL_L2TBL_LARGE_PAGE_SIZE:	/* 64K Large Page */
	case TTBL_L2TBL_SMALL_PAGE_SIZE:	/* 4K Small Page */
		l2base = *l1_tte & TTBL_L1TBL_TTE_BASE10_MASK;
		l2_tte = (u32 *) ((pg->va & ~TTBL_L1TBL_TTE_OFFSET_MASK) >>
				  TTBL_L2TBL_TTE_OFFSET_SHIFT);
		l2 = cpu_mmu_l2tbl_find_tbl_pa(l1, l2base);
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
			*l2_tte |= (pg->ap << TTBL_L2TBL_TTE_NG_SHIFT) &
			    TTBL_L2TBL_TTE_NG_MASK;
			*l2_tte |= (pg->ap << TTBL_L2TBL_TTE_S_SHIFT) &
			    TTBL_L2TBL_TTE_S_MASK;
			*l2_tte |= (pg->ap << (TTBL_L2TBL_TTE_AP2_SHIFT - 2)) &
			    TTBL_L2TBL_TTE_AP2_MASK;
			*l2_tte |= (pg->ap << TTBL_L2TBL_TTE_AP_SHIFT) &
			    TTBL_L2TBL_TTE_AP_MASK;
			*l2_tte |= (pg->c << TTBL_L2TBL_TTE_C_SHIFT) &
			    TTBL_L2TBL_TTE_C_MASK;
			*l2_tte |= (pg->b << TTBL_L2TBL_TTE_B_SHIFT) &
			    TTBL_L2TBL_TTE_B_MASK;
			l2->tte_cnt++;
			if (pg->sz == TTBL_L2TBL_LARGE_PAGE_SIZE) {
				for (ite = 1; ite < 16; ite++) {
					l2_tte[ite] = l2_tte[0];
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

mmu_map_return:
	return rc;
}

int cpu_mmu_get_reserved_page(virtual_addr_t va, cpu_page_t * pg)
{
	return cpu_mmu_get_page(&mmuctrl.defl1, va, pg);
}

int cpu_mmu_unmap_reserved_page(cpu_page_t * pg)
{
	int rc;

	if (!pg) {
		return VMM_EFAIL;
	}

	if ((rc = cpu_mmu_unmap_page(&mmuctrl.defl1, pg))) {
		return rc;
	}

	return VMM_OK;
}

int cpu_mmu_map_reserved_page(cpu_page_t * pg)
{
	int rc;

	if (!pg) {
		return VMM_EFAIL;
	}

	if ((rc = cpu_mmu_map_page(&mmuctrl.defl1, pg))) {
		return rc;
	}

	return VMM_OK;
}

cpu_l1tbl_t *cpu_mmu_l1tbl_alloc(void)
{
	u32 i, *nl1_tte;
	cpu_l1tbl_t *nl1 = NULL;
	struct dlist *le;
	cpu_l2tbl_t *l2, *nl2;

	if (mmuctrl.l1_alloc_count < TTBL_MAX_L1TBL_COUNT) {
		for (i = 0; i < TTBL_MAX_L1TBL_COUNT; i++) {
			if (!mmuctrl.l1_bmap[i]) {
				break;
			}
		}
		if (i == TTBL_MAX_L1TBL_COUNT) {
			return NULL;
		}
		mmuctrl.l1_bmap[i] = 1;
		mmuctrl.l1_alloc_count++;
		nl1 = &mmuctrl.l1_array[i];
	} else {
		return NULL;
	}

	INIT_LIST_HEAD(&nl1->l2tbl_list);
	nl1->tte_cnt = 0;
	nl1->l2tbl_cnt = 0;

	vmm_memcpy((void *)nl1->tbl_va,
		   (void *)mmuctrl.defl1.tbl_va, TTBL_L1TBL_SIZE);
	nl1->tte_cnt = mmuctrl.defl1.tte_cnt;

	list_for_each(le, &mmuctrl.defl1.l2tbl_list) {
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
	nl1->l2tbl_cnt = mmuctrl.defl1.l2tbl_cnt;

	list_add(&mmuctrl.l1tbl_list, &nl1->head);

	return nl1;

l1tbl_alloc_fail:
	if (nl1) {
		while (!list_empty(&nl1->l2tbl_list)) {
			le = list_pop(&nl1->l2tbl_list);
			nl2 = list_entry(le, cpu_l2tbl_t, head);
			cpu_mmu_l2tbl_free(nl2);
		}
		mmuctrl.l1_bmap[nl1->l1_num] = 0;
		mmuctrl.l1_alloc_count--;
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
	if (l1->tbl_pa == mmuctrl.defl1.tbl_pa) {
		return VMM_EFAIL;
	}

	while (!list_empty(&l1->l2tbl_list)) {
		le = list_pop(&l1->l2tbl_list);
		l2 = list_entry(le, cpu_l2tbl_t, head);
		cpu_mmu_l2tbl_free(l2);
	}

	list_del(&l1->head);

	mmuctrl.l1_bmap[l1->l1_num] = 0;
	mmuctrl.l1_alloc_count--;

	return VMM_OK;
}

cpu_l1tbl_t *cpu_mmu_l1tbl_default(void)
{
	return &mmuctrl.defl1;
}

cpu_l1tbl_t *cpu_mmu_l1tbl_current(void)
{
	u32 ttbr0;

	ttbr0 = read_ttbr0();

	return cpu_mmu_l1tbl_find_tbl_pa(ttbr0);
}

u32 cpu_mmu_physical_read32(physical_addr_t pa)
{
	u32 ret = 0x0, ite, found;
	u32 * l1_tte = NULL;
	cpu_l1tbl_t * l1 = NULL;
	virtual_addr_t va = 0x0;
	irq_flags_t flags;

	flags = vmm_cpu_irq_save();

	l1 = cpu_mmu_l1tbl_current();
	if (l1) {
		found = 0;
		l1_tte = (u32 *)(l1->tbl_va);
		for (ite = 0; ite < (TTBL_L1TBL_SIZE / 4); ite++) {
			if ((l1_tte[ite] & TTBL_L2TBL_TTE_TYPE_MASK) == 
			    TTBL_L2TBL_TTE_TYPE_FAULT) {
				found = 1;
				break;
			}
		}
		if (found) {
			l1_tte[ite] = 0x0;
			l1_tte[ite] |= (pa & TTBL_L1TBL_TTE_BASE20_MASK);
			l1_tte[ite] |= (TTBL_L1TBL_TTE_DOM_RESERVED << 
					TTBL_L1TBL_TTE_DOM_SHIFT) &
					TTBL_L1TBL_TTE_DOM_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_NS2_SHIFT) &
					TTBL_L1TBL_TTE_NS2_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_NG_SHIFT) &
					TTBL_L1TBL_TTE_NG_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_S_SHIFT) &
					TTBL_L1TBL_TTE_S_MASK;
			l1_tte[ite] |= (TTBL_AP_SRW_U << 
					(TTBL_L1TBL_TTE_AP2_SHIFT - 2)) &
					TTBL_L1TBL_TTE_AP2_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_TEX_SHIFT) &
					TTBL_L1TBL_TTE_TEX_MASK;
			l1_tte[ite] |= (TTBL_AP_SRW_U << 
					TTBL_L1TBL_TTE_AP_SHIFT) &
					TTBL_L1TBL_TTE_AP_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_IMP_SHIFT) &
					TTBL_L1TBL_TTE_IMP_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_XN_SHIFT) &
					TTBL_L1TBL_TTE_XN_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_C_SHIFT) &
					TTBL_L1TBL_TTE_C_MASK;
			l1_tte[ite] |= (0x0 << TTBL_L1TBL_TTE_B_SHIFT) &
					TTBL_L1TBL_TTE_B_MASK;
			l1_tte[ite] |= TTBL_L1TBL_TTE_TYPE_SECTION;
			va = (ite << TTBL_L1TBL_TTE_BASE20_SHIFT) + 
			     (pa & ~TTBL_L1TBL_TTE_BASE20_MASK);
			va &= ~0x3;
			ret = *(u32 *)(va);
			l1_tte[ite] = 0x0;
			invalid_tlb_line(va);
		}
	}

	vmm_cpu_irq_restore(flags);

	return ret;
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

int vmm_cpu_aspace_map(virtual_addr_t va, 
			virtual_size_t sz, 
			physical_addr_t pa,
			u32 mem_flags)
{
	cpu_page_t p;
	vmm_memset(&p, 0, sizeof(p));
	p.pa = pa;
	p.va = va;
	p.sz = sz;
	p.imp = 0;
	p.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
	if (mem_flags & (VMM_MEMORY_READABLE | VMM_MEMORY_WRITEABLE)) {
		p.ap = TTBL_AP_SRW_U;
	} else if (mem_flags & VMM_MEMORY_READABLE) {
		p.ap = TTBL_AP_SR_U;
	} else if (mem_flags & VMM_MEMORY_WRITEABLE) {
		p.ap = TTBL_AP_SRW_U;
	} else {
		p.ap = TTBL_AP_S_U;
	}
	p.xn = (mem_flags & VMM_MEMORY_EXECUTABLE) ? 0 : 1;
	p.c = (mem_flags & VMM_MEMORY_CACHEABLE) ? 1 : 0;
	p.b = 0;
	return cpu_mmu_map_reserved_page(&p);
}

int vmm_cpu_aspace_unmap(virtual_addr_t va, 
			 virtual_size_t sz)
{
	int rc;
	cpu_page_t p;
	rc = cpu_mmu_get_reserved_page(va, &p);
	if (rc) {
		return rc;
	}
	return cpu_mmu_unmap_reserved_page(&p);
}

int vmm_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t * pa)
{
	int rc = VMM_OK;
	cpu_page_t p;

	if ((rc = cpu_mmu_get_reserved_page(va, &p))) {
		return rc;
	}

	*pa = p.pa + (va & (p.sz - 1));

	return VMM_OK;
}

int vmm_cpu_aspace_init(physical_addr_t * resv_pa, 
			virtual_addr_t * resv_va,
			virtual_size_t * resv_sz)
{
	int rc = VMM_EFAIL;
	u32 i, val;
	virtual_addr_t va;
	virtual_size_t sz;
	physical_addr_t pa;
	cpu_page_t respg;

	/* Reset the memory of MMU control structure */
	vmm_memset(&mmuctrl, 0, sizeof(mmuctrl));

	/* Initialize list heads */
	INIT_LIST_HEAD(&mmuctrl.l1tbl_list);
	INIT_LIST_HEAD(&mmuctrl.l2tbl_list);

	/* Handcraft default translation table */
	INIT_LIST_HEAD(&mmuctrl.defl1.l2tbl_list);
	mmuctrl.defl1.tbl_va = (virtual_addr_t)&defl1_mem;
	mmuctrl.defl1.tbl_pa = vmm_code_paddr() + 
			       ((virtual_addr_t)&defl1_mem - vmm_code_vaddr());
	if (vmm_code_paddr() != vmm_code_vaddr()) {
		val = vmm_code_paddr() >> TTBL_L1TBL_TTE_OFFSET_SHIFT;
		val = val << 2;
		*((u32 *)(mmuctrl.defl1.tbl_va + val)) = 0x0;
		invalid_tlb();
	}
	mmuctrl.defl1.tte_cnt = 0;
	for (i = 0; i < TTBL_L1TBL_SIZE; i += 4) {
		val = *((u32 *)(mmuctrl.defl1.tbl_va + i));
		if ((val & TTBL_L1TBL_TTE_TYPE_MASK) != 
				TTBL_L1TBL_TTE_TYPE_FAULT) {
			mmuctrl.defl1.tte_cnt++;
		}
	}
	mmuctrl.defl1.l2tbl_cnt = 0;

	/* Compute additional reserved space required */
	pa = vmm_code_paddr();
	va = vmm_code_vaddr();
	sz = vmm_code_size();
	if ((va <= *resv_va) && (*resv_va < (va + sz))) {
		*resv_va = va + sz;
	} else if ((va <= (*resv_va + *resv_sz)) && 
		   ((*resv_va + *resv_sz) < (va + sz))) {
		*resv_va = va + sz;
	}
	if ((pa <= *resv_pa) && (*resv_pa < (pa + sz))) {
		*resv_pa = pa + sz;
	} else if ((pa <= (*resv_pa + *resv_sz)) && 
		   ((*resv_pa + *resv_sz) < (pa + sz))) {
		*resv_pa = pa + sz;
	}
	if (*resv_va & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) {
		*resv_va += TTBL_L1TBL_SECTION_PAGE_SIZE - 
			    (*resv_va & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1));
	}
	if (*resv_pa & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) {
		*resv_pa += TTBL_L1TBL_SECTION_PAGE_SIZE - 
			    (*resv_pa & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1));
	}
	*resv_sz = (*resv_sz & 0x3) ? (*resv_sz & ~0x3) + 0x4 : *resv_sz;
	mmuctrl.l1_bmap = (u8 *)(*resv_va + *resv_sz);
	*resv_sz += TTBL_MAX_L1TBL_COUNT;
	*resv_sz = (*resv_sz & 0x3) ? (*resv_sz & ~0x3) + 0x4 : *resv_sz;
	mmuctrl.l1_array = (cpu_l1tbl_t *)(*resv_va + *resv_sz);
	*resv_sz += sizeof(cpu_l1tbl_t) * TTBL_MAX_L1TBL_COUNT;
	*resv_sz = (*resv_sz & 0x3) ? (*resv_sz & ~0x3) + 0x4 : *resv_sz;
	mmuctrl.l2_bmap = (u8 *)(*resv_va + *resv_sz);
	*resv_sz += TTBL_MAX_L2TBL_COUNT;
	*resv_sz = (*resv_sz & 0x3) ? (*resv_sz & ~0x3) + 0x4 : *resv_sz;
	mmuctrl.l2_array = (cpu_l2tbl_t *)(*resv_va + *resv_sz);
	*resv_sz += sizeof(cpu_l2tbl_t) * TTBL_MAX_L2TBL_COUNT;
	*resv_sz = (*resv_sz & 0x3) ? (*resv_sz & ~0x3) + 0x4 : *resv_sz;
	if (*resv_sz & (TTBL_L1TBL_SIZE - 1)) {
		*resv_sz += TTBL_L1TBL_SIZE - 
			    (*resv_sz & (TTBL_L1TBL_SIZE - 1));
	}
	mmuctrl.l1_base_va = *resv_va + *resv_sz;
	mmuctrl.l1_base_pa = *resv_pa + *resv_sz;
	*resv_sz += TTBL_L1TBL_SIZE * TTBL_MAX_L1TBL_COUNT;
	mmuctrl.l2_base_va = *resv_va + *resv_sz;
	mmuctrl.l2_base_pa = *resv_pa + *resv_sz;
	*resv_sz += TTBL_L2TBL_SIZE * TTBL_MAX_L2TBL_COUNT;
	if (*resv_sz & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1)) {
		*resv_sz += TTBL_L1TBL_SECTION_PAGE_SIZE - 
			    (*resv_sz & (TTBL_L1TBL_SECTION_PAGE_SIZE - 1));
	}
	
	/* Map space for reserved area */
	pa = *resv_pa;
	va = *resv_va;
	sz = *resv_sz;
	while (sz) {
		vmm_memset(&respg, 0, sizeof(respg));
		respg.pa = pa;
		respg.va = va;
		respg.sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		respg.imp = 0;
		respg.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
		respg.ap = TTBL_AP_SRW_U;
		respg.xn = 0;
		respg.c = 1;
		respg.b = 0;
		if ((rc = cpu_mmu_map_reserved_page(&respg))) {
			goto mmu_init_error;
		}
		sz -= TTBL_L1TBL_SECTION_PAGE_SIZE;
		pa += TTBL_L1TBL_SECTION_PAGE_SIZE;
		va += TTBL_L1TBL_SECTION_PAGE_SIZE;
	}

	/* Setup up l1 array */
	vmm_memset(mmuctrl.l1_bmap, 0x0, TTBL_MAX_L1TBL_COUNT);
	vmm_memset(mmuctrl.l1_array, 0x0, 
		   sizeof(cpu_l1tbl_t) * TTBL_MAX_L1TBL_COUNT);
	for (i = 0; i < TTBL_MAX_L1TBL_COUNT; i++) {
		INIT_LIST_HEAD(&mmuctrl.l1_array[i].head);
		mmuctrl.l1_array[i].l1_num = i;
		mmuctrl.l1_array[i].tbl_pa = mmuctrl.l1_base_pa + 
						i * TTBL_L1TBL_SIZE;
		mmuctrl.l1_array[i].tbl_va = mmuctrl.l1_base_va + 
						i * TTBL_L1TBL_SIZE;
	}

	/* Setup up l2 array */
	vmm_memset(mmuctrl.l2_bmap, 0x0, TTBL_MAX_L2TBL_COUNT);
	vmm_memset(mmuctrl.l2_array, 0x0, 
		   sizeof(cpu_l2tbl_t) * TTBL_MAX_L2TBL_COUNT);
	for (i = 0; i < TTBL_MAX_L2TBL_COUNT; i++) {
		INIT_LIST_HEAD(&mmuctrl.l2_array[i].head);
		mmuctrl.l2_array[i].l2_num = i;
		mmuctrl.l2_array[i].tbl_pa = mmuctrl.l2_base_pa + 
						i * TTBL_L2TBL_SIZE;
		mmuctrl.l2_array[i].tbl_va = mmuctrl.l2_base_va + 
						i * TTBL_L2TBL_SIZE;
	}

	return VMM_OK;

mmu_init_error:
	return rc;
}
