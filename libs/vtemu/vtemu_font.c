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
 * @file vtemu_font.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Video terminal emulation font database
 */

/*
 * linux/drivers/video/fonts.c -- `Soft' font definitions
 *
 *    Created 1995 by Geert Uytterhoeven
 *    Rewritten 1998 by Martin Mares <mj@ucw.cz>
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <vmm_macros.h>
#include <libs/stringlib.h>
#include <libs/vtemu_font.h>

#define VTEMU_NO_FONTS

static const struct vtemu_font *fonts[] = {
#ifdef CONFIG_VTEMU_FONT_8x8
#undef VTEMU_NO_FONTS
    &font_vga_8x8,
#endif
#ifdef CONFIG_VTEMU_FONT_8x16
#undef VTEMU_NO_FONTS
    &font_vga_8x16,
#endif
#ifdef CONFIG_VTEMU_FONT_6x11
#undef VTEMU_NO_FONTS
    &font_vga_6x11,
#endif
#ifdef CONFIG_VTEMU_FONT_7x14
#undef VTEMU_NO_FONTS
    &font_7x14,
#endif
#ifdef CONFIG_VTEMU_FONT_SUN8x16
#undef VTEMU_NO_FONTS
    &font_sun_8x16,
#endif
#ifdef CONFIG_VTEMU_FONT_SUN12x22
#undef VTEMU_NO_FONTS
    &font_sun_12x22,
#endif
#ifdef CONFIG_VTEMU_FONT_10x18
#undef VTEMU_NO_FONTS
    &font_10x18,
#endif
#ifdef CONFIG_VTEMU_FONT_ACORN_8x8
#undef VTEMU_NO_FONTS
    &font_acorn_8x8,
#endif
#ifdef CONFIG_VTEMU_FONT_PEARL_8x8
#undef VTEMU_NO_FONTS
    &font_pearl_8x8,
#endif
#ifdef CONFIG_VTEMU_FONT_MINI_4x6
#undef VTEMU_NO_FONTS
    &font_mini_4x6,
#endif
};

#define num_fonts array_size(fonts)

#ifdef VTEMU_NO_FONTS
#error No fonts configured for vtemu.
#endif

/**
 *	Find a font
 *	@name: string name of a font
 *
 *	Find a specified font with string name @name.
 *
 *	Returns %NULL if no font found, or a pointer to the
 *	specified font.
 *
 */

const struct vtemu_font *vtemu_find_font(const char *name)
{
   unsigned int i;

   for (i = 0; i < num_fonts; i++)
      if (!strcmp(fonts[i]->name, name))
	  return fonts[i];
   return NULL;
}


/**
 *	Get default font
 *	@xres: screen size of X
 *	@yres: screen size of Y
 *      @font_w: bit array of supported widths (1 - 32)
 *      @font_h: bit array of supported heights (1 - 32)
 *
 *	Get the default font for a specified screen size.
 *	Dimensions are in pixels.
 *
 *	Returns %NULL if no font is found, or a pointer to the
 *	chosen font.
 *
 */

const struct vtemu_font *vtemu_get_default_font(int xres, int yres, 
						u32 font_w, u32 font_h)
{
    int i, c, cc;
    const struct vtemu_font *f, *g;

    g = NULL;
    cc = -10000;
    for(i=0; i<num_fonts; i++) {
	f = fonts[i];
	c = f->pref;
#if defined(__mc68000__)
#ifdef CONFIG_FONT_PEARL_8x8
	if (MACH_IS_AMIGA && f->idx == PEARL8x8_IDX)
	    c = 100;
#endif
#ifdef CONFIG_FONT_6x11
	if (MACH_IS_MAC && xres < 640 && f->idx == VGA6x11_IDX)
	    c = 100;
#endif
#endif
	if ((yres < 400) == (f->height <= 8))
	    c += 1000;

	if ((font_w & (1 << (f->width - 1))) &&
	    (font_h & (1 << (f->height - 1))))
	    c += 1000;

	if (c > cc) {
	    cc = c;
	    g = f;
	}
    }
    return g;
}

