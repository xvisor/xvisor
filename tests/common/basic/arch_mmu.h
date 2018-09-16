/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file arch_mmu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for MMU functions
 */

#ifndef __ARCH_MMU_H__
#define __ARCH_MMU_H__

#include <arch_types.h>

void arch_mmu_section_test(u32 *total, u32 *pass, u32 *fail);
void arch_mmu_page_test(u32 *total, u32 *pass, u32 *fail);
bool arch_mmu_is_enabled(void);
void arch_mmu_setup(void);
void arch_mmu_cleanup(void);

#endif /* __ARCH_MMU_H__ */
