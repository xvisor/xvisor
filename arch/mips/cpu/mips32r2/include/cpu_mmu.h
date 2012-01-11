/*
 * Copyright (c) 2010-2020 Himanshu Chauhan.
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
 * @author: Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief: MMU related definition and structures.
 */

#ifndef __CPU_MMU_H_
#define __CPU_MMU_H_

#define PAGE_SHIFT		12
#define PAGE_SIZE		(0x01UL << PAGE_SHIFT)
#define PAGE_MASK		~(PAGE_SIZE - 1)
#define PFN_SHIFT		6
#define VPN2_SHIFT		13
#define ASID_SHIFT		6
#define ASID_MASK		~((0x01UL << 8) - 1)

#define TLB_PAGE_SIZE_1K	0x400
#define TLB_PAGE_SIZE_4K	0x1000
#define TLB_PAGE_SIZE_16K	0x4000
#define TLB_PAGE_SIZE_256K	0x40000
#define TLB_PAGE_SIZE_1M	0x100000
#define TLB_PAGE_SIZE_4M	0x400000
#define TLB_PAGE_SIZE_16M	0x1000000
#define TLB_PAGE_SIZE_64M	0x4000000
#define TLB_PAGE_SIZE_256M	0x10000000

#define VMM_ASID		1

#define is_vmm_asid(x)		((x >> ASID_SHIFT) == VMM_ASID)
#define is_guest_asid(x)	((x & 0xC0))
#define _ASID(x)		(x >> ASID_SHIFT)

#define TBE_VPN2(_tlb_entry)				\
  ({							\
    unsigned int _res;					\
    _res = (_tlb_entry)->entryhi._s_entryhi.vpn2;	\
    (_res << VPN2_SHIFT);				\
  })

#define TBE_PGMSKD_VPN2(_tlb_entry)			\
	({ unsigned int _res;				\
	  _res = TBE_VPN2(_tlb_entry) &			\
	    ~(_tlb_entry->page_mask);			\
	  _res;						\
	})

#define TBE_ASID(_tlb_entry)			\
	({ unsigned int _res;			\
		_res = _tlb_entry->entryhi.	\
			_s_entryhi.asid;	\
			_res;			\
	})

#define TBE_ELO_GLOBAL(_tlb_entry, _ELOT)	\
	({ unsigned int _res;			\
		_res = _tlb_entry->_ELOT.	\
			_s_entrylo.global;	\
		_res;				\
	})

#define TBE_ELO_VALID(_tlb_entry, _ELOT)	\
	({ unsigned int _res;			\
		_res = _tlb_entry->_ELOT.	\
			_s_entrylo.valid;	\
		_res;				\
	})

#define TBE_ELO_INVALIDATE(_tlb_entry, _ELOT)	(_tlb_entry-> \
						 _ELOT._s_entrylo.valid = 0)

#if !defined(__ASSEMBLY__)
#include <vmm_types.h>

struct arch_regs;

typedef union mips32_entryhi  {
	u32 _entryhi;
	struct {
		u32 vpn2:19;
		u32 vpn2x:2;
		u32 reserved:3; /* Must be written as zero */
		u32 asid:8;
	}_s_entryhi;
} mips32_entryhi_t;

typedef union mips32_entrylo {
	u32 _entrylo;
	struct {
		u32 pfn:26;
		u32 cacheable:3;
		u32 dirty:1;
		u32 valid:1;
		u32 global:1;
	}_s_entrylo;
} mips32_entrylo_t;

typedef struct mips32_tlb_entry {
	u32 page_mask;
	mips32_entrylo_t entrylo0;
	mips32_entrylo_t entrylo1;
	mips32_entryhi_t entryhi;
} mips32_tlb_entry_t;

#define BITS_PER_PGD		10
#define NUM_PGD_ENTRIES		(0x01UL << BITS_PER_PGD)
#define PGD_MASK		(NUM_PGD_ENTRIES - 1)
#define PGD_SHIFT		22

#define BITS_PER_PTAB		10
#define PTAB_SHIFT		12
#define NUM_PTAB_ENTRIES	(0x01UL << BITS_PER_PTAB)
#define PTAB_MASK		(NUM_PTAB_ENTRIES - 1)

/*
 * 8 MB of initial mapping. Rest will be dynamically
 * allocated after heap is initialized.
 */
#define PG_TABLE_INIT_MAP_SZ	(8 * 1024 * 1024)

typedef u32 pgd_t;
typedef u32 ptab_t;

/* Page flags */
#define PAGE_MAPPED		(0x01UL << 0)
#define PAGE_GLOBAL_MAPPED	(0x01UL << 1)

typedef struct pte {
	virtual_addr_t vaddr;
	physical_addr_t paddr;
	u32 flags;
	u32 reserved;
} __attribute__((packed)) pte_t;

pte_t *vmm_cpu_va2pte(virtual_addr_t vaddr);

void mips_fill_tlb_entry(mips32_tlb_entry_t *tlb_entry, int index);
u32 do_tlbmiss(struct arch_regs *uregs);
int vmm_cpu_aspace_va2pa(virtual_addr_t va, physical_addr_t * pa);
pte_t *vmm_cpu_va2pte(virtual_addr_t vaddr);
void set_current_asid(u32 cur_asid);

#endif /* ! __ASSEMBLY__ */

#endif /* __CPU_MMU_H_ */
