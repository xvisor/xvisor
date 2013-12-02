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
 * @file syscopyarea.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic copyarea (sys-to-sys)
 * 
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/video/syscopyarea.c
 *
 *  Generic Bit Block Transfer for frame buffers located in system RAM with
 *  packed pixels of any depth.
 *
 *  Based almost entirely from cfbcopyarea.c (which is based almost entirely
 *  on Geert Uytterhoeven's copyarea routine)
 *
 *      Copyright (C)  2007 Antonino Daplas <adaplas@pol.net>
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <libs/bitops.h>
#include <drv/fb.h>

#include "fb_draw.h"

    /*
     *  Generic bitwise copy algorithm
     */

static void
bitcpy(struct fb_info *p, unsigned long *dst, int dst_idx,
		const unsigned long *src, int src_idx, int bits, unsigned n)
{
	unsigned long first, last;
	int const shift = dst_idx-src_idx;
	int left, right;

	first = FB_SHIFT_HIGH(p, ~0UL, dst_idx);
	last = ~(FB_SHIFT_HIGH(p, ~0UL, umod32(dst_idx+n, bits)));

	if (!shift) {
		/* Same alignment for source and dest */
		if (dst_idx+n <= bits) {
			/* Single word */
			if (last)
				first &= last;
			*dst = comp(*src, *dst, first);
		} else {
			/* Multiple destination words */
			/* Leading bits */
 			if (first != ~0UL) {
				*dst = comp(*src, *dst, first);
				dst++;
				src++;
				n -= bits - dst_idx;
			}

			/* Main chunk */
			n = udiv32(n, bits);
			while (n >= 8) {
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				n -= 8;
			}
			while (n--)
				*dst++ = *src++;

			/* Trailing bits */
			if (last)
				*dst = comp(*src, *dst, last);
		}
	} else {
		unsigned long d0, d1;
		int m;

		/* Different alignment for source and dest */
		right = shift & (bits - 1);
		left = -shift & (bits - 1);

		if (dst_idx+n <= bits) {
			/* Single destination word */
			if (last)
				first &= last;
			if (shift > 0) {
				/* Single source word */
				*dst = comp(*src >> right, *dst, first);
			} else if (src_idx+n <= bits) {
				/* Single source word */
				*dst = comp(*src << left, *dst, first);
			} else {
				/* 2 source words */
				d0 = *src++;
				d1 = *src;
				*dst = comp(d0 << left | d1 >> right, *dst,
					    first);
			}
		} else {
			/* Multiple destination words */
			/** We must always remember the last value read,
			    because in case SRC and DST overlap bitwise (e.g.
			    when moving just one pixel in 1bpp), we always
			    collect one full long for DST and that might
			    overlap with the current long from SRC. We store
			    this value in 'd0'. */
			d0 = *src++;
			/* Leading bits */
			if (shift > 0) {
				/* Single source word */
				*dst = comp(d0 >> right, *dst, first);
				dst++;
				n -= bits - dst_idx;
			} else {
				/* 2 source words */
				d1 = *src++;
				*dst = comp(d0 << left | *dst >> right, *dst, first);
				d0 = d1;
				dst++;
				n -= bits - dst_idx;
			}

			/* Main chunk */
			m = umod32(n, bits);
			n = udiv32(n, bits);
			while (n >= 4) {
				d1 = *src++;
				*dst++ = d0 << left | d1 >> right;
				d0 = d1;
				d1 = *src++;
				*dst++ = d0 << left | d1 >> right;
				d0 = d1;
				d1 = *src++;
				*dst++ = d0 << left | d1 >> right;
				d0 = d1;
				d1 = *src++;
				*dst++ = d0 << left | d1 >> right;
				d0 = d1;
				n -= 4;
			}
			while (n--) {
				d1 = *src++;
				*dst++ = d0 << left | d1 >> right;
				d0 = d1;
			}

			/* Trailing bits */
			if (last) {
				if (m <= right) {
					/* Single source word */
					*dst = comp(d0 << left, *dst, last);
				} else {
					/* 2 source words */
 					d1 = *src;
					*dst = comp(d0 << left | d1 >> right,
						    *dst, last);
				}
			}
		}
	}
}

    /*
     *  Generic bitwise copy algorithm, operating backward
     */

static void
bitcpy_rev(struct fb_info *p, unsigned long *dst, int dst_idx,
		const unsigned long *src, int src_idx, int bits, unsigned n)
{
	unsigned long first, last;
	int shift;

	dst += udiv32((n-1), bits);
	src += udiv32((n-1), bits);
	if (umod32((n-1), bits)) {
		dst_idx += umod32((n-1), bits);
		dst += dst_idx >> (ffs(bits) - 1);
		dst_idx &= bits - 1;
		src_idx += umod32((n-1), bits);
		src += src_idx >> (ffs(bits) - 1);
		src_idx &= bits - 1;
	}

	shift = dst_idx-src_idx;

	first = FB_SHIFT_LOW(p, ~0UL, bits - 1 - dst_idx);
	last = ~(FB_SHIFT_LOW(p, ~0UL, bits - 1 - umod32((dst_idx-n), bits)));

	if (!shift) {
		/* Same alignment for source and dest */
		if ((unsigned long)dst_idx+1 >= n) {
			/* Single word */
			if (last)
				first &= last;
			*dst = comp(*src, *dst, first);
		} else {
			/* Multiple destination words */

			/* Leading bits */
			if (first != ~0UL) {
				*dst = comp(*src, *dst, first);
				dst--;
				src--;
				n -= dst_idx+1;
			}

			/* Main chunk */
			n = udiv32(n, bits);
			while (n >= 8) {
				*dst-- = *src--;
				*dst-- = *src--;
				*dst-- = *src--;
				*dst-- = *src--;
				*dst-- = *src--;
				*dst-- = *src--;
				*dst-- = *src--;
				*dst-- = *src--;
				n -= 8;
			}
			while (n--)
				*dst-- = *src--;
			/* Trailing bits */
			if (last)
				*dst = comp(*src, *dst, last);
		}
	} else {
		/* Different alignment for source and dest */

		int const left = -shift & (bits-1);
		int const right = shift & (bits-1);

		if ((unsigned long)dst_idx+1 >= n) {
			/* Single destination word */
			if (last)
				first &= last;
			if (shift < 0) {
				/* Single source word */
				*dst = comp(*src << left, *dst, first);
			} else if (1+(unsigned long)src_idx >= n) {
				/* Single source word */
				*dst = comp(*src >> right, *dst, first);
			} else {
				/* 2 source words */
				*dst = comp(*src >> right | *(src-1) << left,
					    *dst, first);
			}
		} else {
			/* Multiple destination words */
			/** We must always remember the last value read,
			    because in case SRC and DST overlap bitwise (e.g.
			    when moving just one pixel in 1bpp), we always
			    collect one full long for DST and that might
			    overlap with the current long from SRC. We store
			    this value in 'd0'. */
			unsigned long d0, d1;
			int m;

			d0 = *src--;
			/* Leading bits */
			if (shift < 0) {
				/* Single source word */
				*dst = comp(d0 << left, *dst, first);
			} else {
				/* 2 source words */
				d1 = *src--;
				*dst = comp(d0 >> right | d1 << left, *dst,
					    first);
				d0 = d1;
			}
			dst--;
			n -= dst_idx+1;

			/* Main chunk */
			m = umod32(n, bits);
			n = udiv32(n, bits);
			while (n >= 4) {
				d1 = *src--;
				*dst-- = d0 >> right | d1 << left;
				d0 = d1;
				d1 = *src--;
				*dst-- = d0 >> right | d1 << left;
				d0 = d1;
				d1 = *src--;
				*dst-- = d0 >> right | d1 << left;
				d0 = d1;
				d1 = *src--;
				*dst-- = d0 >> right | d1 << left;
				d0 = d1;
				n -= 4;
			}
			while (n--) {
				d1 = *src--;
				*dst-- = d0 >> right | d1 << left;
				d0 = d1;
			}

			/* Trailing bits */
			if (last) {
				if (m <= left) {
					/* Single source word */
					*dst = comp(d0 >> right, *dst, last);
				} else {
					/* 2 source words */
					d1 = *src;
					*dst = comp(d0 >> right | d1 << left,
						    *dst, last);
				}
			}
		}
	}
}

void sys_copyarea(struct fb_info *p, const struct fb_copyarea *area)
{
	u32 dx = area->dx, dy = area->dy, sx = area->sx, sy = area->sy;
	u32 height = area->height, width = area->width;
	unsigned long const bits_per_line = p->fix.line_length*8u;
	unsigned long *dst = NULL, *src = NULL;
	int bits = BITS_PER_LONG, bytes = bits >> 3;
	int dst_idx = 0, src_idx = 0, rev_copy = 0;

	if (p->state != FBINFO_STATE_RUNNING)
		return;

	/* if the beginning of the target area might overlap with the end of
	the source area, be have to copy the area reverse. */
	if ((dy == sy && dx > sx) || (dy > sy)) {
		dy += height;
		sy += height;
		rev_copy = 1;
	}

	/* split the base of the framebuffer into a long-aligned address and
	   the index of the first bit */
	dst = src = (unsigned long *)((unsigned long)p->screen_base &
				      ~(bytes-1));
	dst_idx = src_idx = 8*((unsigned long)p->screen_base & (bytes-1));
	/* add offset of source and target area */
	dst_idx += dy*bits_per_line + dx*p->var.bits_per_pixel;
	src_idx += sy*bits_per_line + sx*p->var.bits_per_pixel;

	if (p->fbops->fb_sync)
		p->fbops->fb_sync(p);

	if (rev_copy) {
		while (height--) {
			dst_idx -= bits_per_line;
			src_idx -= bits_per_line;
			dst += dst_idx >> (ffs(bits) - 1);
			dst_idx &= (bytes - 1);
			src += src_idx >> (ffs(bits) - 1);
			src_idx &= (bytes - 1);
			bitcpy_rev(p, dst, dst_idx, src, src_idx, bits,
				width*p->var.bits_per_pixel);
		}
	} else {
		while (height--) {
			dst += dst_idx >> (ffs(bits) - 1);
			dst_idx &= (bytes - 1);
			src += src_idx >> (ffs(bits) - 1);
			src_idx &= (bytes - 1);
			bitcpy(p, dst, dst_idx, src, src_idx, bits,
				width*p->var.bits_per_pixel);
			dst_idx += bits_per_line;
			src_idx += bits_per_line;
		}
	}
}
VMM_EXPORT_SYMBOL(sys_copyarea);

