/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file simplefb.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief SimpleFB driver source
 */

#include <arm_io.h>
#include <arm_math.h>
#include <libfdt/fdt_support.h>
#include <sys/vminfo.h>

#define SIMPLEFB_MAGIC_OFFSET		0x00
#define SIMPLEFB_VENDOR_OFFSET		0x04
#define SIMPLEFB_VERSION_OFFSET		0x08
#define SIMPLEFB_MODE_OFFSET		0x10
#define SIMPLEFB_WIDTH_OFFSET		0x50
#define SIMPLEFB_HEIGHT_OFFSET		0x54
#define SIMPLEFB_STRIDE_OFFSET		0x58
#define SIMPLEFB_FB_BASE_MS_OFFSET	0x5c
#define SIMPLEFB_FB_BASE_LS_OFFSET	0x60

u32 simplefb_magic(virtual_addr_t base)
{
	return arm_readl((void *)(base + SIMPLEFB_MAGIC_OFFSET));
}

u32 simplefb_vendor(virtual_addr_t base)
{
	return arm_readl((void *)(base + SIMPLEFB_VENDOR_OFFSET));
}

u32 simplefb_version(virtual_addr_t base)
{
	return arm_readl((void *)(base + SIMPLEFB_VERSION_OFFSET));
}

u32 simplefb_mode(virtual_addr_t base, char *mode, u32 mode_size)
{
	u32 len;
	virtual_addr_t mbase = base + SIMPLEFB_MODE_OFFSET;

	if (!mode || !mode_size)
		return 0;

	if (mode_size > 16)
		mode_size = 16;

	len = 0;
	while (len < mode_size) {
		mode[len] = arm_readl((void *)(mbase + len * 0x4)) & 0xff;
		len++;
	}

	mode[mode_size - 1] = '\0';

	return len;
}

u32 simplefb_width(virtual_addr_t base)
{
	return arm_readl((void *)(base + SIMPLEFB_WIDTH_OFFSET));
}

u32 simplefb_height(virtual_addr_t base)
{
	return arm_readl((void *)(base + SIMPLEFB_HEIGHT_OFFSET));
}

u32 simplefb_stride(virtual_addr_t base)
{
	return arm_readl((void *)(base + SIMPLEFB_STRIDE_OFFSET));
}

physical_addr_t simplefb_fb_base(virtual_addr_t base)
{
	u32 ms = arm_readl((void *)(base + SIMPLEFB_FB_BASE_MS_OFFSET));
	u32 ls = arm_readl((void *)(base + SIMPLEFB_FB_BASE_LS_OFFSET));

	return (physical_addr_t)(((u64)ms << 32) | ((u64)ls));
}

void simplefb_fdt_fixup(virtual_addr_t base, void *fdt_addr)
{
	char mode[16];
	u32 width, height, stride, mode_len;

	mode_len = simplefb_mode(base, mode, sizeof(mode));
	if (!mode_len)
		return;

	width = simplefb_width(base);
	height = simplefb_height(base);
	stride = simplefb_stride(base);

	do_fixup_by_compat(fdt_addr, "simple-framebuffer",
			   "format", mode, mode_len, 1);
	do_fixup_by_compat_u32(fdt_addr, "simple-framebuffer",
			       "width", width, 1);
	do_fixup_by_compat_u32(fdt_addr, "simple-framebuffer",
			       "height", height, 1);
	do_fixup_by_compat_u32(fdt_addr, "simple-framebuffer",
			       "stride", stride, 1);
}
