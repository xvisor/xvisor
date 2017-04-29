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

#define IDR_INITIALIZER(__name)	\
{ .root = RADIX_TREE_INIT(__name.root), }

#define DEFINE_IDR(__name)	\
struct idr __name = IDR_INITIALIZER(__name)

#define DECLARE_IDR(__name)	\
struct idr __name

#define INIT_IDR(__idr)		\
do { \
	INIT_RADIX_TREE(&(__idr)->root, 0); \
} while (0)

/** Allocate id for given pointer */
int idr_alloc(struct idr *idr, void *ptr,
	      int start, int end, unsigned gfp_mask);

/** Find pointer associated with given id */
void *idr_find(struct idr *idr, int id);

/** Remove id to pointer mapping */
void idr_remove(struct idr *idr, int id);

struct ida {
	struct idr idr;
};

#define IDA_INITIALIZER(__name)	\
{ .idr = IDR_INITIALIZER(__name.idr), }

#define DEFINE_IDA(__name)	\
struct ida __name = IDA_INITIALIZER(__name)

#define DECLARE_IDA(__name)	\
struct ida __name

#define INIT_IDA(__ida)	\
do { \
	INIT_IDR(&(__ida)->idr); \
} while (0)

/** Allocate new ID using ID allocator */
int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end,
		   unsigned gfp_mask);

/** Free-up a ID back to ID allocatore for reuse */
void ida_simple_remove(struct ida *ida, unsigned int id);

#endif /* __IDR_H__ */
