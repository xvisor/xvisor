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

typedef void (*drawfn)(struct vmm_surface *,
			void *, u8 *, const u8 *, int, int);

drawfn drawfn_surface_fntable_8[48];

drawfn drawfn_surface_fntable_15[48];

drawfn drawfn_surface_fntable_16[48];

drawfn drawfn_surface_fntable_24[48];

drawfn drawfn_surface_fntable_32[48];

#endif
