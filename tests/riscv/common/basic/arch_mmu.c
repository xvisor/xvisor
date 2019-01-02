/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file arch_mmu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for MMU functions
 */

#include <arch_board.h>
#include <basic_stdio.h>

void arch_mmu_section_test(u32 * total, u32 * pass, u32 * fail)
{
	/* TODO: For now nothing to do here. */
	*total = 0;
	*pass = 0;
	*fail = 0;
	return;
}

void arch_mmu_page_test(u32 * total, u32 * pass, u32 * fail)
{
	/* TODO: For now nothing to do here. */
	*total = 0;
	*pass = 0;
	*fail = 0;
	return;
}

bool arch_mmu_is_enabled(void)
{
	/* TODO: For now nothing to do here. */
	return FALSE;
}

void arch_mmu_setup(void)
{
	/* TODO: For now nothing to do here. */
	return;
}

void arch_mmu_cleanup(void)
{
	/* TODO: For now nothing to do here. */
	return;
}
