/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file drawfn_template.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic framebuffer format conversion template.
 *
 * The header has been largely adapted from QEMU sources:
 * hw/display/pl110_template.h
 * 
 * Arm PrimeCell PL110 Color LCD Controller
 *
 * Copyright (c) 2005-2009 CodeSourcery.
 * Written by Paul Brook
 *
 * Framebuffer format conversion routines.
 *
 * The original code is licensed under the GPL.
 */

#ifndef bswap32
#ifdef CONFIG_CPU_LE
#define bswap32(x) vmm_cpu_to_be32(x)
#endif
#ifdef CONFIG_CPU_BE
#define bswap32(x) vmm_cpu_to_le32(x)
#endif
#endif

#ifndef glue
#define xglue(x, y) x ## y
#define glue(x, y) xglue(x, y)
#endif

#ifndef ORDER

#if SURFACE_BITS == 8
#define COPY_PIXEL(s, to, from) \
	vmm_surface_write8(s, to, from); to++;
#elif SURFACE_BITS == 15 || SURFACE_BITS == 16
#define COPY_PIXEL(s, to, from) \
	vmm_surface_write16(s, (u16 *)to, from); to += 2;
#elif SURFACE_BITS == 24
#define COPY_PIXEL(s, to, from) \
	vmm_surface_write8(s, to, from); to++; \
	vmm_surface_write8(s, to, (from) >> 8); to++; \
	vmm_surface_write8(s, to, (from) >> 16); to++;
#elif SURFACE_BITS == 32
#define COPY_PIXEL(s, to, from) \
	vmm_surface_write32(s, (u32 *)to, from); to += 4;
#else
#error unknown bit depth
#endif

#undef RGB
#define BORDER bgr
#define ORDER 0
#include "drawfn_template.h"
#define ORDER 1
#include "drawfn_template.h"
#define ORDER 2
#include "drawfn_template.h"
#undef BORDER
#define RGB
#define BORDER rgb
#define ORDER 0
#include "drawfn_template.h"
#define ORDER 1
#include "drawfn_template.h"
#define ORDER 2
#include "drawfn_template.h"
#undef BORDER

typedef void (*drawfn)(struct vmm_surface *,
			void *, u8 *, const u8 *, int, int);

drawfn glue(drawfn_surface_fntable_,SURFACE_BITS)[48] = {
	glue(drawfn_line1_lblp_bgr,SURFACE_BITS),
	glue(drawfn_line2_lblp_bgr,SURFACE_BITS),
	glue(drawfn_line4_lblp_bgr,SURFACE_BITS),
	glue(drawfn_line8_lblp_bgr,SURFACE_BITS),
	glue(drawfn_line16_555_lblp_bgr,SURFACE_BITS),
	glue(drawfn_line32_lblp_bgr,SURFACE_BITS),
	glue(drawfn_line16_lblp_bgr,SURFACE_BITS),
	glue(drawfn_line12_lblp_bgr,SURFACE_BITS),

	glue(drawfn_line1_bbbp_bgr,SURFACE_BITS),
	glue(drawfn_line2_bbbp_bgr,SURFACE_BITS),
	glue(drawfn_line4_bbbp_bgr,SURFACE_BITS),
	glue(drawfn_line8_bbbp_bgr,SURFACE_BITS),
	glue(drawfn_line16_555_bbbp_bgr,SURFACE_BITS),
	glue(drawfn_line32_bbbp_bgr,SURFACE_BITS),
	glue(drawfn_line16_bbbp_bgr,SURFACE_BITS),
	glue(drawfn_line12_bbbp_bgr,SURFACE_BITS),

	glue(drawfn_line1_lbbp_bgr,SURFACE_BITS),
	glue(drawfn_line2_lbbp_bgr,SURFACE_BITS),
	glue(drawfn_line4_lbbp_bgr,SURFACE_BITS),
	glue(drawfn_line8_lbbp_bgr,SURFACE_BITS),
	glue(drawfn_line16_555_lbbp_bgr,SURFACE_BITS),
	glue(drawfn_line32_lbbp_bgr,SURFACE_BITS),
	glue(drawfn_line16_lbbp_bgr,SURFACE_BITS),
	glue(drawfn_line12_lbbp_bgr,SURFACE_BITS),

	glue(drawfn_line1_lblp_rgb,SURFACE_BITS),
	glue(drawfn_line2_lblp_rgb,SURFACE_BITS),
	glue(drawfn_line4_lblp_rgb,SURFACE_BITS),
	glue(drawfn_line8_lblp_rgb,SURFACE_BITS),
	glue(drawfn_line16_555_lblp_rgb,SURFACE_BITS),
	glue(drawfn_line32_lblp_rgb,SURFACE_BITS),
	glue(drawfn_line16_lblp_rgb,SURFACE_BITS),
	glue(drawfn_line12_lblp_rgb,SURFACE_BITS),

	glue(drawfn_line1_bbbp_rgb,SURFACE_BITS),
	glue(drawfn_line2_bbbp_rgb,SURFACE_BITS),
	glue(drawfn_line4_bbbp_rgb,SURFACE_BITS),
	glue(drawfn_line8_bbbp_rgb,SURFACE_BITS),
	glue(drawfn_line16_555_bbbp_rgb,SURFACE_BITS),
	glue(drawfn_line32_bbbp_rgb,SURFACE_BITS),
	glue(drawfn_line16_bbbp_rgb,SURFACE_BITS),
	glue(drawfn_line12_bbbp_rgb,SURFACE_BITS),

	glue(drawfn_line1_lbbp_rgb,SURFACE_BITS),
	glue(drawfn_line2_lbbp_rgb,SURFACE_BITS),
	glue(drawfn_line4_lbbp_rgb,SURFACE_BITS),
	glue(drawfn_line8_lbbp_rgb,SURFACE_BITS),
	glue(drawfn_line16_555_lbbp_rgb,SURFACE_BITS),
	glue(drawfn_line32_lbbp_rgb,SURFACE_BITS),
	glue(drawfn_line16_lbbp_rgb,SURFACE_BITS),
	glue(drawfn_line12_lbbp_rgb,SURFACE_BITS),
};

#undef SURFACE_BITS
#undef COPY_PIXEL

#else

#if ORDER == 0
#define NAME glue(glue(lblp_, BORDER), SURFACE_BITS)
#ifdef CONFIG_CPU_BE
#define SWAP_WORDS 1
#endif
#elif ORDER == 1
#define NAME glue(glue(bbbp_, BORDER), SURFACE_BITS)
#ifndef CONFIG_CPU_BE
#define SWAP_WORDS 1
#endif
#else
#define SWAP_PIXELS 1
#define NAME glue(glue(lbbp_, BORDER), SURFACE_BITS)
#ifdef CONFIG_CPU_BE
#define SWAP_WORDS 1
#endif
#endif

#define FN_2(x, y) FN(x, y) FN(x+1, y)
#define FN_4(x, y) FN_2(x, y) FN_2(x+2, y)
#define FN_8(y) FN_4(0, y) FN_4(4, y)

static void glue(drawfn_line1_,NAME)(struct vmm_surface *s,
				     void *opaque, u8 *d, const u8 *src,
				     int width, int deststep)
{
	u32 *palette = opaque;
	u32 data;
	while (width > 0) {
		data = *(u32 *)src;
#ifdef SWAP_PIXELS
#define FN(x, y) COPY_PIXEL(s, d, palette[(data >> (y + 7 - (x))) & 1]);
#else
#define FN(x, y) COPY_PIXEL(s, d, palette[(data >> ((x) + y)) & 1]);
#endif
#ifdef SWAP_WORDS
		FN_8(24)
		FN_8(16)
		FN_8(8)
		FN_8(0)
#else
		FN_8(0)
		FN_8(8)
		FN_8(16)
		FN_8(24)
#endif
#undef FN
		width -= 32;
		src += 4;
	}
}

static void glue(drawfn_line2_,NAME)(struct vmm_surface *s,
				     void *opaque, u8 *d, const u8 *src,
				     int width, int deststep)
{
	u32 *palette = opaque;
	u32 data;
	while (width > 0) {
		data = *(u32 *)src;
#ifdef SWAP_PIXELS
#define FN(x, y) COPY_PIXEL(s, d, palette[(data >> (y + 6 - (x)*2)) & 3]);
#else
#define FN(x, y) COPY_PIXEL(s, d, palette[(data >> ((x)*2 + y)) & 3]);
#endif
#ifdef SWAP_WORDS
		FN_4(0, 24)
		FN_4(0, 16)
		FN_4(0, 8)
		FN_4(0, 0)
#else
		FN_4(0, 0)
		FN_4(0, 8)
		FN_4(0, 16)
		FN_4(0, 24)
#endif
#undef FN
		width -= 16;
		src += 4;
	}
}

static void glue(drawfn_line4_,NAME)(struct vmm_surface *s,
				     void *opaque, u8 *d, const u8 *src,
				     int width, int deststep)
{
	u32 *palette = opaque;
	u32 data;
	while (width > 0) {
		data = *(u32 *)src;
#ifdef SWAP_PIXELS
#define FN(x, y) COPY_PIXEL(s, d, palette[(data >> (y + 4 - (x)*4)) & 0xf]);
#else
#define FN(x, y) COPY_PIXEL(s, d, palette[(data >> ((x)*4 + y)) & 0xf]);
#endif
#ifdef SWAP_WORDS
		FN_2(0, 24)
		FN_2(0, 16)
		FN_2(0, 8)
		FN_2(0, 0)
#else
		FN_2(0, 0)
		FN_2(0, 8)
		FN_2(0, 16)
		FN_2(0, 24)
#endif
#undef FN
		width -= 8;
		src += 4;
	}
}

static void glue(drawfn_line8_,NAME)(struct vmm_surface *s,
				     void *opaque, u8 *d, const u8 *src,
				     int width, int deststep)
{
	u32 *palette = opaque;
	u32 data;
	while (width > 0) {
		data = *(u32 *)src;
#define FN(x) COPY_PIXEL(s, d, palette[(data >> (x)) & 0xff]);
#ifdef SWAP_WORDS
		FN(24)
		FN(16)
		FN(8)
		FN(0)
#else
		FN(0)
		FN(8)
		FN(16)
		FN(24)
#endif
#undef FN
		width -= 4;
		src += 4;
	}
}

static void glue(drawfn_line16_,NAME)(struct vmm_surface *s,
				      void *opaque, u8 *d, const u8 *src,
				      int width, int deststep)
{
	u32 data;
	unsigned int r, g, b;
	while (width > 0) {
		data = *(u32 *)src;
#ifdef SWAP_WORDS
		data = bswap32(data);
#endif
#ifdef RGB
#define LSB r
#define MSB b
#else
#define LSB b
#define MSB r
#endif
#if 0
		LSB = data & 0x1f;
		data >>= 5;
		g = data & 0x3f;
		data >>= 6;
		MSB = data & 0x1f;
		data >>= 5;
#else
		LSB = (data & 0x1f) << 3;
		data >>= 5;
		g = (data & 0x3f) << 2;
		data >>= 6;
		MSB = (data & 0x1f) << 3;
		data >>= 5;
#endif
		COPY_PIXEL(s, d, glue(rgb_to_pixel,SURFACE_BITS)(r, g, b));
		LSB = (data & 0x1f) << 3;
		data >>= 5;
		g = (data & 0x3f) << 2;
		data >>= 6;
		MSB = (data & 0x1f) << 3;
		data >>= 5;
		COPY_PIXEL(s, d, glue(rgb_to_pixel,SURFACE_BITS)(r, g, b));
#undef MSB
#undef LSB
		width -= 2;
		src += 4;
	}
}

static void glue(drawfn_line32_,NAME)(struct vmm_surface *s,
				      void *opaque, u8 *d, const u8 *src,
				      int width, int deststep)
{
	u32 data;
	unsigned int r, g, b;
	while (width > 0) {
		data = *(u32 *)src;
#ifdef RGB
#define LSB r
#define MSB b
#else
#define LSB b
#define MSB r
#endif
#ifndef SWAP_WORDS
		LSB = data & 0xff;
		g = (data >> 8) & 0xff;
		MSB = (data >> 16) & 0xff;
#else
		LSB = (data >> 24) & 0xff;
		g = (data >> 16) & 0xff;
		MSB = (data >> 8) & 0xff;
#endif
		COPY_PIXEL(s, d, glue(rgb_to_pixel,SURFACE_BITS)(r, g, b));
#undef MSB
#undef LSB
		width--;
		src += 4;
	}
}

static void glue(drawfn_line16_555_,NAME)(struct vmm_surface *s,
					  void *opaque, u8 *d, const u8 *src,
					  int width, int deststep)
{
	/* RGB 555 plus an intensity bit (which we ignore) */
	u32 data;
	unsigned int r, g, b;
	while (width > 0) {
		data = *(u32 *)src;
#ifdef SWAP_WORDS
		data = bswap32(data);
#endif
#ifdef RGB
#define LSB r
#define MSB b
#else
#define LSB b
#define MSB r
#endif
		LSB = (data & 0x1f) << 3;
		data >>= 5;
		g = (data & 0x1f) << 3;
		data >>= 5;
		MSB = (data & 0x1f) << 3;
		data >>= 5;
		COPY_PIXEL(s, d, glue(rgb_to_pixel,SURFACE_BITS)(r, g, b));
		LSB = (data & 0x1f) << 3;
		data >>= 5;
		g = (data & 0x1f) << 3;
		data >>= 5;
		MSB = (data & 0x1f) << 3;
		data >>= 6;
		COPY_PIXEL(s, d, glue(rgb_to_pixel,SURFACE_BITS)(r, g, b));
#undef MSB
#undef LSB
		width -= 2;
		src += 4;
	}
}

static void glue(drawfn_line12_,NAME)(struct vmm_surface *s,
				      void *opaque, u8 *d, const u8 *src,
				      int width, int deststep)
{
	/* RGB 444 with 4 bits of zeroes at the top of each halfword */
	u32 data;
	unsigned int r, g, b;
	while (width > 0) {
		data = *(u32 *)src;
#ifdef SWAP_WORDS
		data = bswap32(data);
#endif
#ifdef RGB
#define LSB r
#define MSB b
#else
#define LSB b
#define MSB r
#endif
		LSB = (data & 0xf) << 4;
		data >>= 4;
		g = (data & 0xf) << 4;
		data >>= 4;
		MSB = (data & 0xf) << 4;
		data >>= 8;
		COPY_PIXEL(s, d, glue(rgb_to_pixel,SURFACE_BITS)(r, g, b));
		LSB = (data & 0xf) << 4;
		data >>= 4;
		g = (data & 0xf) << 4;
		data >>= 4;
		MSB = (data & 0xf) << 4;
		data >>= 8;
		COPY_PIXEL(s, d, glue(rgb_to_pixel,SURFACE_BITS)(r, g, b));
#undef MSB
#undef LSB
		width -= 2;
		src += 4;
	}
}

#undef SWAP_PIXELS
#undef NAME
#undef SWAP_WORDS
#undef ORDER

#endif
