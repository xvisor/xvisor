/**
 * Copyright (c) 2018 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file ept.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Definitions and structres related to EPT
 */

#ifndef __EPT_H
#define __EPT_H

#include <vmm_types.h>
#include <cpu_vm.h>

#define EPT_PROT_READ		(0x1 << 0)
#define EPT_PROT_WRITE		(0x1 << 1)
#define EPT_PROT_EXEC_S		(0x1 << 2)
#define EPT_PROT_EXEC_U		(0x1 << 10)
#define EPT_PROT_MASK		(~(EPT_PROT_READ | EPT_PROT_WRITE	\
				   | EPT_PROT_EXEC_S | EPT_PROT_EXEC_U))

#define EPT_PAGE_SIZE_1G	(0x1ULL << 30)
#define EPT_PAGE_SIZE_2M	(0x1ULL << 21)
#define EPT_PAGE_SIZE_4K	(0x1ULL << 12)

extern struct cpuinfo_x86 cpu_info;

#define PHYS_ADDR_BIT_MASK	((0x1ul << cpu_info.phys_bits) - 1)
#define EPT_PAGE_MASK_2M	(PHYS_ADDR_BIT_MASK >> 21)
#define EPT_PAGE_MASK_4K	(PHYS_ADDR_BIT_MASK >> 12)
#define EPT_PAGE_MASK_1G	(PHYS_ADDR_BIT_MASK >> 30)

#define EPT_PHYS_FILTER(_p)	(_p & PHYS_ADDR_BIT_MASK)
#define EPT_PHYS_2MB_PFN(_p)	(EPT_PHYS_FILTER(_p) >> 21)
#define EPT_PHYS_1GB_PFN(_p)	(EPT_PHYS_FILTER(_p) >> 30)
#define EPT_PHYS_4KB_PFN(_p)	(EPT_PHYS_FILTER(_p) >> 12)

#define EPT_PHYS_2MB_PAGE(_p)   ((_p & EPT_PAGE_MASK_2M) << 21)
#define EPT_PHYS_1GB_PAGE(_p)	((_p & EPT_PAGE_MASK_1G) << 30)
#define EPT_PHYS_4KB_PAGE(_p)	((_p & EPT_PAGE_MASK_4K) << 12)

typedef union {
	u64 val;

	struct {
		u64 mt:3;        /* Memory type: 0 Uncacheable 6 Writeback */
		u64 pgwl:3;      /* Pagewalk length */
		u64 en_ad:1;     /* Enable accessed/dirty flags for EPT structures */
		u64 en_ssr:1;    /* Setting this control to 1 enables enforcement of
		                    access rights for supervisor shadow-stack pages */
		u64 res:4;       /* reserved */
		u64 pml4:52;     /* pml4 physical base, only bits N-1:12 are valid
				  * where N is the physical address width of the
				  * logical processor */
	} bits;
} eptp_t;

typedef union {
	u64 val;

	struct {
		u64 r:1;          /* Read access; Region 512 GiB */
		u64 w:1;          /* Write access */
		u64 x:1;          /* Execute access */
		u64 res:5;        /* Reserved */
		u64 accessed:1;   /* Depends on Bit 6 in EPTP. Currently not set */
		u64 ign:1;        /* Ignored */
		u64 mbe:1;        /* Mode based execution */
		u64 ign1:1;
		u64 pdpt_base:40; /* Physical address of 4-KByte aligned EPT
				   * page-directory-pointer table referenced by this entry */
		u64 ign2:12;      /* Ignored */
	} bits;
} ept_pml4e_t;

typedef union {
	u64 val;

	struct {
		u64 r:1;           /* Read access; Region 1 GiB */
		u64 w:1;           /* Write access */
		u64 x:1;           /* Execute */
		u64 mt:3;          /* EPT memory type */
		u64 ign_pat:1;     /* Ignore PAT memory type for this 1 GiB page */
		u64 is_page:1;     /* Ignore */
		u64 accessed:1;    /* Accessed (If bit 6 set in EPTP) */
		u64 dirty:1;       /* Dirty (If bit 6 set in EPTP) */
		u64 mbe:1;
		u64 ign1:1;        /* Ignored */
		u64 res:18;        /* Must be zero */
		u64 phys:22;       /* physicall address of PD */
		u64 ign2:8;
		u64 superv_ss:1;   /* supervisor shadow stack */
		u64 ign3:2;        /* ignored */
		u64 sup_ve:1;      /* suppress #VE exception */
	} pe;

	struct {
		u64 r:1;           /* Read access */
		u64 w:1;           /* Write */
		u64 x:1;           /* Execute */
		u64 res:5;         /* Reservd */
		u64 accessed:1;    /* Accessed by software (if Bit 6 in EPTP is set) */
		u64 ign:1;         /* Ignored */
		u64 mbe:1;         /* mode based exec */
		u64 ign1:1;
		u64 pd_base:40;    /* Page directory base */
		u64 ign2:12;       /* Ignored */
	} te;
} ept_pdpte_t;

typedef union {
	u64 val;

	struct {
		u64 r:1;           /* Read; Region 2 MiB */
		u64 w:1;           /* Write */
		u64 x:1;           /* Execute */
		u64 mt:3;          /* Memory Type */
		u64 ign_pat:1;     /* Ignore PAT type for this region */
		u64 is_page:1;     /* Must be set to 1 */
		u64 accessed:1;    /* Region was accessed by software */
		u64 dirty:1;       /* Region was written to by software */
		u64 mbe:1;
		u64 ign:1;         /* Ignored */
		u64 res:9;         /* Must be zero */
		u64 phys:31;       /* Physical address of 2MiB page */
		u64 ign1:8;        /* Ignored */
		u64 superv_ss:1;
		u64 ign2:2;
		u64 sup_ve:1;      /* Suppress #VE */
	} pe;

	struct {
		u64 r:1;           /* Read */
		u64 w:1;           /* Written */
		u64 x:1;           /* Execute */
		u64 res:4;         /* Reserved */
		u64 is_page:1;     /* Must be zero */
		u64 accessed:1;    /* Accessed by software (if bit 6 is set in EPTP) */
		u64 ign:1;         /* Ignore */
		u64 mbe:1;
		u64 ign1:1;
		u64 pt_base:40;    /* Physical address of the page table */
		u64 res1:12;       /* Reserved */
	} te;
} ept_pde_t;

typedef union {
	u64 val;

	struct {
		u64 r:1;           /* Read; Region 4KiB */
		u64 w:1;           /* Write */
		u64 x:1;           /* Execute */
		u64 mt:3;          /* Memory Type */
		u64 ign_pat:1;     /* Ignore PAT memory type */
		u64 ign:1;         /* Ignored */
		u64 accessed:1;    /* Accessed by software (if bit 6 in eptp set) */
		u64 dirty:1;       /* Written by software (if bit 6 in eptp set) */
		u64 mbe:1;
		u64 ign1:1;        /* Ignored */
		u64 phys:40;       /* Physical address of 4 KiB page mapped */
		u64 ign2:8;       /* Ignored */
		u64 superv_ss:1;
		u64 subpage_w:1;
		u64 ign3:1;
		u64 sup_ve:1;      /* Suppress #VE */
	} pe;
} ept_pte_t;

int setup_ept(struct vcpu_hw_context *context);
int ept_create_pte_map(struct vcpu_hw_context *context,
		       physical_addr_t gphys, physical_addr_t hphys,
		       size_t pg_size, u32 pg_prot);

#endif /* __EPT_H */
