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
 * @file drawfn.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic framebuffer format conversion routines.
 */

#ifndef __DRAWFN_H__
#define __DRAWFN_H__

enum drawfn_bppmode {
	DRAWFN_BPP_1,
	DRAWFN_BPP_2,
	DRAWFN_BPP_4,
	DRAWFN_BPP_8,
	DRAWFN_BPP_16,
	DRAWFN_BPP_32,
	DRAWFN_BPP_16_565,
	DRAWFN_BPP_12
};

#define DRAWFN_BPPMODE_MAX	(DRAWFN_BPP_12 + 1)

enum drawfn_order {
	/* little-endian bytes and little-endian pixels */
	DRAWFN_ORDER_LBLP,
	/* big-endian bytes and big-endian pixels */
	DRAWFN_ORDER_BBBP,
	/* big-endian bytes and little-endian pixels */
	DRAWFN_ORDER_BBLP
};

#define DRAWFN_ORDER_MAX	(DRAWFN_ORDER_BBLP + 1)

enum drawfn_format {
	/* blue-green-red color format */
	DRAWFN_FORMAT_BGR,
	/* red-green-blue color format */
	DRAWFN_FORMAT_RGB
};

#define DRAWFN_FORMAT_MAX	(DRAWFN_FORMAT_RGB + 1)

typedef void (*drawfn)(struct vmm_surface *,
			void *, u8 *, const u8 *, int, int);

#define DRAWFN_FNTABLE_INDEX(format, order, bppmode)	\
((format) * (DRAWFN_ORDER_MAX * DRAWFN_BPPMODE_MAX) + \
 (order) * DRAWFN_BPPMODE_MAX + \
 (bppmode))

#define DRAWFN_FNTABLE_SIZE	(DRAWFN_BPPMODE_MAX * \
				 DRAWFN_ORDER_MAX * \
				 DRAWFN_FORMAT_MAX)

drawfn drawfn_surface_fntable_8[DRAWFN_FNTABLE_SIZE];

drawfn drawfn_surface_fntable_15[DRAWFN_FNTABLE_SIZE];

drawfn drawfn_surface_fntable_16[DRAWFN_FNTABLE_SIZE];

drawfn drawfn_surface_fntable_24[DRAWFN_FNTABLE_SIZE];

drawfn drawfn_surface_fntable_32[DRAWFN_FNTABLE_SIZE];

#endif
