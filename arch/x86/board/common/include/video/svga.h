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
 * @file fb_console.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Framebuffer console header file.
 */

#ifndef SVGA_H
#define SVGA_H

#include <vmm_types.h>
#include <vmm_host_aspace.h>

#define SVGA_DEFAULT_MODE 0x117

/* RRRRR GGGGGG BBBBB */
#define SVGA_24TO16BPP(x) ((x & 0xF80000) >> 8) | ((x & 0xFC00) >> 5) | ((x & 0xF8) >> 3)

typedef struct svga_mode_info {
	u16 attributes;
	u8 windowA, windowB;
	u16 granularity;
	u16 windowSize;
	u16 segmentA, segmentB;
	u32 winFuncPtr; /* ptr to INT 0x10 Function 0x4F05 */
	u16 pitch; /* bytes per scan line */

	u16 screen_width, screen_height; /* resolution */
	u8 wChar, yChar, planes, bpp, banks; /* number of banks */
	u8 memoryModel, bankSize, imagePages;
	u8 reserved0;

	// color masks
	u8 readMask, redPosition;
	u8 greenMask, greenPosition;
	u8 blueMask, bluePosition;
	u8 reservedMask, reservedPosition;
	u8 directColorAttributes;

	u32 physbase; /* pointer to LFB in LFB modes */
	u32 offScreenMemOff;
	u16 offScreenMemSize;
	u8 reserved1[206];
} __attribute__((packed)) svga_mode_info_t;

void svga_change_mode(u16);
svga_mode_info_t* svga_mode_get_info(u16);
virtual_addr_t svga_map_fb(physical_addr_t, virtual_size_t);

#endif
