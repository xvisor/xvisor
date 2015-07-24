/**
 * Copyright (c) 2011 Jean-Christophe Dubois
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
 * @file libsort.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief header file for sorting related functions
 */

#ifndef __LIBSORT_H_
#define __LIBSORT_H_

#include <vmm_stdio.h>
#include <libs/list.h>

/* A fast, small, non-recursive O(nlog n) sort from Linux
 * kernel for portability */
void simple_sort(void *base, size_t num, size_t size,
                 int (*cmp)(const void *, const void *),
                 void (*swap)(void *, void *, int));

/** Sort array using smoothsort.
 *
 * Sort @a N elements from array @a base starting with index @a r with smoothsort.
 *
 * @param base  pointer to array
 * @param r     lowest index to sort
 * @param N     number of elements to sort
 * @param less  comparison function returning nonzero if m[a] < m[b]
 * @param swap  swapper function exchanging elements m[a] and m[b]
 */
int libsort_smoothsort(void *base, size_t r, size_t N,
		       int (*less) (void *m, size_t a, size_t b),
		       void (*swap) (void *m, size_t a, size_t b));

/**
 * list_sort - sort a list
 * @priv: private data, opaque to list_sort(), passed to @cmp
 * @head: the list to sort
 * @cmp: the elements comparison function
 *
 * This function implements "merge sort", which has O(nlog(n))
 * complexity.
 *
 * The comparison function @cmp must return a negative value if @a
 * should sort before @b, and a positive value if @a should sort after
 * @b. If @a and @b are equivalent, and their original relative
 * ordering is to be preserved, @cmp must return 0.
 */
void list_mergesort(void *priv, struct dlist *head,
		    int (*cmp)(void *priv, struct dlist *a,
			       struct dlist *b));
#endif
