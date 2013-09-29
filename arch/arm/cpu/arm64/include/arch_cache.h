/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file arch_cache.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief arch specific cache operation
 */
#ifndef _ARCH_CACHE_H__
#define _ARCH_CACHE_H__

#include <cpu_cache.h>

#define ARCH_CACHE_LINE_SIZE		64
#define ARCH_CACHE_LINE_SHIFT		6

/* Prototype: 
 * void arch_flush_cache_all(void) 
 */
#define ARCH_HAS_FLUSH_CACHE_ALL
#define arch_flush_cache_all()			do { \
						clean_invalidate_dcache(); \
						} while (0)

/* Prototype: 
 * void arch_flush_cache_range(virtual_addr_t start, virtual_addr_t end) 
 */
#define ARCH_HAS_FLUSH_CACHE_RANGE
#define arch_flush_cache_range(start, end)	do { \
						clean_invalidate_dcache_mva_range((start),(end)); \
						} while (0)

/* Prototype: 
 * void arch_flush_dcache_range(virtual_addr_t start, virtual_addr_t end) 
 */
#define ARCH_HAS_FLUSH_DCACHE_RANGE
#define arch_flush_dcache_range(start, end)	do { \
						clean_invalidate_dcache_mva_range((start),(end)); \
						} while (0)


#endif /* _ARCH_CACHE_H__ */
