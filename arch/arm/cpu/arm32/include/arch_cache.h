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
 * @file arch_cache.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief arch specific cache operation
 */
#ifndef _ARCH_CACHE_H__
#define _ARCH_CACHE_H__

#include <cpu_cache.h>

#define ARCH_CACHE_LINE_SIZE		32
#define ARCH_CACHE_LINE_SHIFT		5

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

/* Prototype:
 * void arch_inv_cache_range(virtual_addr_t start, virtual_addr_t end)
 */
#define ARCH_HAS_INV_DCACHE_RANGE
#define arch_inv_dcache_range(start, end)	do { \
						invalidate_dcache_mva_range((start),(end)); \
						} while (0)

/* Prototype:
 * void arch_clean_cache_range(virtual_addr_t start, virtual_addr_t end)
 */
#define ARCH_HAS_CLEAN_DCACHE_RANGE
#define arch_clean_dcache_range(start, end)	do { \
						clean_dcache_mva_range((start),(end)); \
						} while (0)

/* Prefetching support.
 * Prototype:
 * void arch_prefetch(const void *ptr)
 */
#if (defined(CONFIG_ARMV5) || \
	defined(CONFIG_ARMV6) || \
	defined(CONFIG_ARMV6K) || \
	defined(CONFIG_ARMV7A))

#define ARCH_HAS_PREFETCH
static inline void arch_prefetch(const void *ptr)
{
	__asm__ __volatile__(
		"pld\t%a0"
		:: "p" (ptr));
}

#endif /* CONFIG_ARMV5 || CONFIG_ARMV6 || CONFIG_ARMV6K ||  CONFIG_ARMV7A */

#endif /* _ARCH_CACHE_H__ */
