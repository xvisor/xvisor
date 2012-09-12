/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file vtemu_font.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Video terminal emulation font database interface
 */

/*
 *  font.h -- `Soft' font definitions
 *
 *  Created 1995 by Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 */


#ifndef __VTEMU_FONT_H_
#define __VTEMU_FONT_H_

#include <vmm_types.h>

struct vtemu_font {
    int idx;
    const char *name;
    int width, height;
    const void *data;
    int pref;
};

#define VGA8x8_IDX	0
#define VGA8x16_IDX	1
#define PEARL8x8_IDX	2
#define VGA6x11_IDX	3
#define FONT7x14_IDX	4
#define	FONT10x18_IDX	5
#define SUN8x16_IDX	6
#define SUN12x22_IDX	7
#define ACORN8x8_IDX	8
#define	MINI4x6_IDX	9

extern const struct vtemu_font	font_vga_8x8,
				font_vga_8x16,
				font_pearl_8x8,
				font_vga_6x11,
				font_7x14,
				font_10x18,
				font_sun_8x16,
				font_sun_12x22,
				font_acorn_8x8,
				font_mini_4x6;

/* Find a font with a specific name */

extern const struct vtemu_font *vtemu_find_font(const char *name);

/* Get the default font for a specific screen size */

const struct vtemu_font *vtemu_get_default_font(int xres, int yres, 
						u32 font_w, u32 font_h);

/* Max. length for the name of a predefined font */
#define VTEMU_MAX_FONT_NAME	32


#endif /* __VTEMU_FONT_H_ */
