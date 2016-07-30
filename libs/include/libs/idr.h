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
 * @file idr.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for Linux-like IDR library.
 */

#ifndef __IDR_H__
#define __IDR_H__

#include <libs/radix-tree.h>

struct idr {
	struct radix_tree_root root;
};

#define DEFINE_IDR(__name)	\
struct idr __name = { .root = RADIX_TREE_INIT(__name.root), }

#define DECLARE_IDR(__name)	\
struct idr __name

#define INIT_IDR(__idr)		\
do { \
	INIT_RADIX_TREE(&(__idr)->root)); \
} while (0)

/** Allocate id for given pointer */
int idr_alloc(struct idr *idr, void *ptr,
	      int start, int end, unsigned gfp_mask);

/** Find pointer associated with given id */
void *idr_find(struct idr *idr, int id);

/** Remove id to pointer mapping */
void idr_remove(struct idr *idr, int id);

#endif /* __IDR_H__ */
