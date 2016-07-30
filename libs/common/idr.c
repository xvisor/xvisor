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
 * @file idr.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief implementation of Linux-like IDR library.
 */

#include <vmm_limits.h>
#include <libs/idr.h>

int idr_alloc(struct idr *idr, void *ptr,
	      int start, int end, unsigned gfp_mask)
{
	unsigned long id = 0;

	/* Ignore gfp_mask becasue that's only for linux compatibility */

	/* sanity checks */
	if (start < 0) {
		return VMM_EINVALID;
	}

	if (end <= 0) {
		end = INT_MAX;
	} else {
		end -= 1;
	}

	id = radix_tree_next_hole(&idr->root, start, end);
	if (id > end) {
		return VMM_ENOSPC;
	}
	if (0 != radix_tree_insert(&idr->root, id, ptr)) {
		return VMM_ENOMEM;
	}

	return id;
}

void *idr_find(struct idr *idr, int id)
{
	if (id < 0) {
		return NULL;
	}

	return radix_tree_lookup(&idr->root, id);
}

void idr_remove(struct idr *idr, int id)
{
	if (id < 0) {
		return;
	}

	radix_tree_delete(&idr->root, id);
}
