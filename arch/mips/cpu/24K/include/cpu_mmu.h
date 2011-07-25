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

#include <vmm_types.h>

#define MAX_HOST_TLB_ENTRIES 6

#define PAGE_SHIFT	12
#define PAGE_SIZE	(0x01UL << PAGE_SHIFT)
#define PAGE_MASK	~(PAGE_SIZE - 1)
#define PFN_SHIFT	6
#define VPN2_SHIFT	13

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

struct host_tlb_entries_info {
	virtual_addr_t vaddr;
	physical_addr_t paddr;
	s32 free;
	s32 tlb_index;
} host_tlb_entries[MAX_HOST_TLB_ENTRIES];

void fill_tlb_entry(struct mips32_tlb_entry *tlb_entry, int index);

#endif /* __CPU_MMU_H_ */
