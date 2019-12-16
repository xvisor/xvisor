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
 * @file cpu_entry_helper.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Boot-time helper funtions
 */

#include <vmm_types.h>
#include <arch_io.h>
#include <libs/libfdt.h>

void __attribute__ ((section(".entry")))
    _copy_fdt(virtual_addr_t fdt_src, virtual_addr_t fdt_dst)
{
	u32 i, fdt_size;
	u32 *src = (u32 *)fdt_src;
	u32 *dst = (u32 *)fdt_dst;

	if (rev32(src[0]) != FDT_MAGIC) {
		while (1); /* Hang !!! */
	}

	fdt_size = rev32(src[1]);
	if (CONFIG_RISCV_MAX_DTB_SIZE < fdt_size) {
		while (1); /* Hang !!! */
	}

	i = 0;
	while (i < fdt_size) {
		if (4 < (fdt_size - i)) {
			dst[i/4] = src[i/4];
			i += 4;
		} else {
			((u8 *)dst)[i] = ((u8 *)src)[i];
			i += 1;
		}
	}
}
