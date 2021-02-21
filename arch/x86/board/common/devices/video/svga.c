/**
 * Copyright (c) 2021 Himanshu Chauhan.
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
 * @file svga.c
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief SVGA initialization
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_completion.h>
#include <vmm_params.h>
#include <libs/vtemu.h>
#include <libs/fifo.h>
#include <drv/input.h>
#include <multiboot.h>
#include <brd_defterm.h>
#include <video/svga.h>

extern struct multiboot_info boot_info;
svga_mode_info_t svga_mode_info;

/*
 * Switches the SVGA mode to the specified mode number.
 */
void svga_change_mode(u16 mode)
{
	vmm_printf("Changing SVGA mode not supported (mode = 0x%X)\n", mode);
}

/*
 * Returns a pointer to the info struct about a certain SVGA mode.
 */
svga_mode_info_t* svga_mode_get_info(u16 mode)
{
	return &svga_mode_info;
}

/*
 * Requests the physical frame buffer address be mapped at the logical frame
 * buffer address.
 *
 * This function will map fb_length bytes.
 *
 * On success, it returns the virtual address where the framebuffer was mapped,
 * or 0 on failure.
 */
virtual_addr_t svga_map_fb(physical_addr_t real_addr, virtual_size_t fb_length)
{
	virtual_addr_t fb_base;

	/* Align framebuffer length to page boundaries */
	fb_length = VMM_ROUNDUP2_PAGE_SIZE(fb_length);

	vmm_printf("%s: physical: 0x%lx size: 0x%lx\n", __func__,
		   real_addr, fb_length);
	fb_base = vmm_host_memmap(real_addr, fb_length,
				  VMM_MEMORY_FLAGS_IO);

	return fb_base;
}
