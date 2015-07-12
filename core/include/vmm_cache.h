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
 * @file vmm_cache.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for cache operations.
 */

#ifndef __VMM_CACHE_H__
#define __VMM_CACHE_H__

#include <vmm_types.h>
#include <vmm_macros.h>
#include <arch_cache.h>

#ifndef ARCH_CACHE_LINE_SIZE
#error "Architecture support must provide ARCH_CACHE_LINE_SIZE."
#endif

#ifndef ARCH_CACHE_LINE_SHIFT
#error "Architecture support must provide ARCH_CACHE_LINE_SHIFT."
#endif

#define VMM_CACHE_LINE_SIZE			ARCH_CACHE_LINE_SIZE
#define VMM_CACHE_LINE_SHIFT			ARCH_CACHE_LINE_SHIFT

#ifndef VMM_CACHE_ALIGN
#define VMM_CACHE_ALIGN(x) 			align(x, VMM_CACHE_LINE_SIZE)
#endif

#ifndef __cacheline_aligned
#define __cacheline_aligned			__attribute__((aligned(VMM_CACHE_LINE_SIZE)))
#endif

#ifndef __cacheline_aligned_in_smp
#define __cacheline_aligned_in_smp		__attribute__((aligned(VMM_CACHE_LINE_SIZE)))
#endif

#ifndef ARCH_HAS_FLUSH_CACHE_ALL
#define vmm_flush_cache_all()			do { } while (0)
#else
#define vmm_flush_cache_all()			arch_flush_cache_all()
#endif

#ifndef ARCH_HAS_FLUSH_CACHE_RANGE
#define vmm_flush_cache_range(start, end)	do { } while (0)
#else
#define vmm_flush_cache_range(start, end)	arch_flush_cache_range(start, end)
#endif

#ifndef ARCH_HAS_FLUSH_CACHE_PAGE
#define vmm_flush_cache_page(page_va)		do { } while (0)
#else
#define vmm_flush_cache_page(page_va)		arch_flush_cache_page(page_va)
#endif

#ifndef ARCH_HAS_FLUSH_ICACHE_RANGE
#define vmm_flush_icache_range(start, end)	do { } while (0)
#else
#define vmm_flush_icache_range(start, end)	arch_flush_icache_range(start, end)
#endif

#ifndef ARCH_HAS_FLUSH_ICACHE_PAGE
#define vmm_flush_icache_page(page_va)		do { } while (0)
#else
#define vmm_flush_icache_page(page_va)		arch_flush_icache_page(page_va)
#endif

#ifndef ARCH_HAS_FLUSH_DCACHE_RANGE
#define vmm_flush_dcache_range(start, end)	do { } while (0)
#else
#define vmm_flush_dcache_range(start, end)	arch_flush_dcache_range(start, end)
#endif

#ifndef ARCH_HAS_FLUSH_DCACHE_PAGE
#define vmm_flush_dcache_page(page_va)		do { } while (0)
#else
#define vmm_flush_dcache_page(page_va)		arch_flush_dcache_page(page_va)
#endif

#ifndef ARCH_HAS_INV_DCACHE_RANGE
#define vmm_inv_dcache_range(start, end)	do { } while (0)
#else
#define vmm_inv_dcache_range(start, end)	arch_inv_dcache_range(start, end)
#endif

#ifndef ARCH_HAS_CLEAN_DCACHE_RANGE
#define vmm_clean_dcache_range(start, end)	do { } while (0)
#else
#define vmm_clean_dcache_range(start, end)	arch_clean_dcache_range(start, end)
#endif

#ifndef ARCH_HAS_OUTERCACHE
#define vmm_inv_outer_cache_range(start, end)	do { } while (0)
#define vmm_clean_outer_cache_range(start, end)	do { } while (0)
#define vmm_flush_outer_cache_range(start, end)	do { } while (0)
#else
#define vmm_inv_outer_cache_range(start, end)	arch_inv_outer_cache_range(start, end)
#define vmm_clean_outer_cache_range(start, end)	arch_inv_outer_cache_range(start, end)
#define vmm_flush_outer_cache_range(start, end)	arch_flush_outer_cache_range(start, end)
#endif

#endif /* __VMM_CACHE_H__ */
