/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file xref.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Simple reference counting wrapper APIs.
 */

#ifndef __XREF_H__
#define __XREF_H__

#include <vmm_types.h>
#include <arch_atomic.h>

struct xref {
	atomic_t ref;
};

static inline void xref_init(struct xref *__x)
{
	ARCH_ATOMIC_INIT(&__x->ref, 1);
}

static inline long xref_val(struct xref *__x)
{
	return arch_atomic_read(&__x->ref);
}

static inline long xref_get(struct xref *__x)
{
	return arch_atomic_add_return(&__x->ref, 1);
}

static inline void xref_put(struct xref *__x,
			    void (*__r)(struct xref *))
{
	if (arch_atomic_sub_return(&__x->ref, 1))
		return;
	__r(__x);
}

#endif
