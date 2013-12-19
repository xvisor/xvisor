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
 * @file vmm_pixel_ops.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for pixel conversion helper APIs
 *
 * The header has been largely adapted from QEMU sources:
 * include/ui/pixel_ops.h
 *
 * The original source is licensed under GPL.
 */

#ifndef __VMM_PIXEL_OPS_H_
#define __VMM_PIXEL_OPS_H_

static inline unsigned int rgb_to_pixel8(unsigned int r, unsigned int g,
						unsigned int b)
{
	return ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
}

static inline unsigned int rgb_to_pixel15(unsigned int r, unsigned int g,
						unsigned int b)
{
	return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
}

static inline unsigned int rgb_to_pixel15bgr(unsigned int r, unsigned int g,
						unsigned int b)
{
	return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3);
}

static inline unsigned int rgb_to_pixel16(unsigned int r, unsigned int g,
						unsigned int b)
{
	return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static inline unsigned int rgb_to_pixel16bgr(unsigned int r, unsigned int g,
						unsigned int b)
{
	return ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3);
}

static inline unsigned int rgb_to_pixel24(unsigned int r, unsigned int g,
						unsigned int b)
{
	return (r << 16) | (g << 8) | b;
}

static inline unsigned int rgb_to_pixel24bgr(unsigned int r, unsigned int g,
						unsigned int b)
{
	return (b << 16) | (g << 8) | r;
}

static inline unsigned int rgb_to_pixel32(unsigned int r, unsigned int g,
						unsigned int b)
{
	return (r << 16) | (g << 8) | b;
}

static inline unsigned int rgb_to_pixel32bgr(unsigned int r, unsigned int g,
						unsigned int b)
{
	return (b << 16) | (g << 8) | r;
}

#endif /* __VMM_PIXEL_OPS_H_ */
