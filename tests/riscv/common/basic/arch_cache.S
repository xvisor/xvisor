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
 * @file arch_cache.S
 * @author Anup Patel (anup@brainfault.org)
 * @brief Cache operations
 */

#include <riscv_asm.h>

	/* TODO: clean the entire data cache */	
	.globl arch_clean_dcache
arch_clean_dcache:
	ret

	/* TODO: clean & invalidate the entire data cache */	
	.globl arch_clean_invalidate_dcache
arch_clean_invalidate_dcache:
	ret

	/* TODO: clean by memory region by mva range 
	 * a0 - start address of region
	 * a1 - end address of region
	 */
	.globl arch_clean_dcache_mva_range
arch_clean_dcache_mva_range:
	ret

	/* TODO: clean and invalidate a memory region by mva
	 * a0 - start address of region
	 * a1 - end address of region
	 */
	.globl arch_clean_invalidate_dcache_mva_range
arch_clean_invalidate_dcache_mva_range:
	ret

	/* TODO: invalidate the entire i-cache */
	.globl arch_invalidate_icache
arch_invalidate_icache:
	ret
