/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file arm_mmu.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Header file for MMU functions
 *
 * Adapted from tests/arm32/pb-a8/basic/arm_mmu.h
 */

#ifndef __ARM_MMU_H_
#define __ARM_MMU_H_

#include <arm_types.h>

void arm_mmu_syscall(struct pt_regs *regs);
void arm_mmu_prefetch_abort(struct pt_regs *regs);
void arm_mmu_data_abort(struct pt_regs *regs);
void arm_mmu_section_test(u32 * total, u32 * pass, u32 * fail);
void arm_mmu_page_test(u32 * total, u32 * pass, u32 * fail);
bool arm_mmu_is_enabled(void);
void arm_mmu_setup(void);
void arm_mmu_cleanup(void);

#endif /* __ARM_MMU_H_ */
