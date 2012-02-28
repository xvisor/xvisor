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
 * @file cpu_cache.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM specific cache operations
 */
#ifndef __CPU_CACHE_H__
#define __CPU_CACHE_H__

#include <vmm_types.h>

/** Invalidate all instruction caches */
void flush_icache(void);

/** Invalidate instruction cache line by MVA to PoU */
void flush_icache_mva(virtual_addr_t mva);

/** Invalidate instruction cache line by set/way */
void flush_icache_line(u32 line);

/** Invalidate entire branch predictor array */
void flush_bpredictor(void);

/** Invalidate MVA from branch predictor array */
void flush_bpredictor_mva(virtual_addr_t mva);

/** Invalidate data caches */
void flush_dcache(void);

/** Invalidate data cache line by MVA */
void flush_dcache_mva(virtual_addr_t mva);

/** Invalidate data cache line by set/way */
void flush_dcache_line(u32 line);

/** Invalidate unified (instruction or data) cache */
void flush_idcache(void);

/** Invalidate unified cache line by MVA */
void flush_idcache_mva(virtual_addr_t mva);

/** Invalidate unified cache line by set/way */
void flush_idcache_line(u32 line);

/** Clean data cache */
void clean_dcache(void);

/** Clean data cache line by MVA */
void clean_dcache_mva(virtual_addr_t mva);

/** Clean data cache line by set/way */
void clean_dcache_line(u32 line);

/** Clean unified (instruction or data) cache */
void clean_idcache(void);

/** Clean unified cache line by MVA */
void clean_idcache_mva(virtual_addr_t mva);

/** Clean unified cache line by set/way */
void clean_idcache_line(u32 line);

/** Clean and invalidate data cache */
void clean_flush_dcache(void);

/** Clean and invalidate data cache line by MVA */
void clean_flush_dcache_mva(virtual_addr_t mva);

/** Clean and invalidate data cache line by set/way */
void clean_flush_dcache_line(u32 line);

/** Clean and invalidate unified (instruction or data) cache */
void clean_flush_idcache(void);

/** Clean and invalidate unified cache line by MVA */
void clean_flush_idcache_mva(virtual_addr_t mva);

/** Clean and invalidate unified cache line by set/way */
void clean_flush_idcache_line(u32 line);

#endif /* __CPU_CACHE_H__ */
