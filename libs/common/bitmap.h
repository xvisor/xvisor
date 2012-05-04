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
 * @file bitmap.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Abstraction and functions for bitmap handling.
 */

#ifndef __BITMAP_H__
#define __BITMAP_H__

#include <vmm_types.h>
#include <vmm_macros.h>

#define DEFINE_BITMAP(name, nbits)	u32 name[((nbits) >> 5) + 1]
#define DECLARE_BITMAP(name)		extern u32 name[]

static inline u32 bitmap_estimate_size(u32 nbits)
{
	return ((((nbits) >> 5) + 1) * sizeof(u32));
}

static inline void bitmap_clearall(u32 *bmap, u32 nbits)
{
	u32 i;
	for (i = 0; i < (((nbits)>>5)+1); i++) {
		bmap[i] = 0x0;
	}
}

static inline void bitmap_setall(u32 *bmap, u32 nbits)
{
	u32 i;
	for (i = 0; i < (((nbits)>>5)+1); i++) {
		bmap[i] = 0xFFFFFFFF;
	}
}

static inline bool bitmap_isset(u32 *bmap, u32 bit)
{
	return (bmap[bit >> 5] & (0x1 << (31 - (bit & 0x1F)))) ? TRUE : FALSE;
}

static inline void bitmap_setbit(u32 *bmap, u32 bit)
{
	bmap[bit >> 5] |= (0x1 << (31 - (bit & 0x1F)));
}

static inline void bitmap_clearbit(u32 *bmap, u32 bit)
{
	bmap[bit >> 5] &= ~(0x1 << (31 - (bit & 0x1F)));
}

static inline u32 bitmap_setcount(u32 *bmap, u32 nbits)
{
	u32 i, ret = 0;
	for (i = 0; i < nbits; i++) {
		if (bitmap_isset(bmap, i)) {
			ret++;
		}
	}
	return ret;
}

static inline u32 bitmap_clearcount(u32 *bmap, u32 nbits)
{
	u32 i, ret = 0;
	for (i = 0; i < nbits; i++) {
		if (!bitmap_isset(bmap, i)) {
			ret++;
		}
	}
	return ret;
}

#endif /* __BITMAP_H__ */
