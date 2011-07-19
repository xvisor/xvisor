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

typedef union mips32_entryhi  {
	u32 _entryhi;
	struct {
		u32 asid:20;
		u32 global:1;
		u32 vpn2:11;
	}_s_entryhi;
} mips32_entryhi_t;

typedef union mips32_entrylo {
	u32 _entrylo;
	struct {
		u32 valid:1;
		u32 dirty:1;
		u32 pfn:30;
	}_s_entrylo;
} mips32_entrylo_t;

typedef struct mips32_tlb_entry {
	u32 page_mask;
	mips32_entrylo_t entrylo0;
	mips32_entrylo_t entrylo1;
	mips32_entryhi_t entryhi;
} mips32_tlb_entry_t;

#endif /* __CPU_MMU_H_ */
