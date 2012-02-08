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

#endif
