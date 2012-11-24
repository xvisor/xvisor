/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file cpu_cache.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Implementation of cache operations for ARMv5 family
 */
#include <cpu_cache.h>
#include <arch_cache.h>

void invalidate_icache(void)
{
	u32 tmp = 0;
	asm volatile(" mcr     p15, 0, %0, c7, c5, 0\n\t"
		     : : "r"(tmp) : );
}

void invalidate_icache_mva(virtual_addr_t mva)
{
	asm volatile(" mcr     p15, 0, %0, c7, c5, 1\n\t"
		     : : "r"(mva) : );
}

void invalidate_icache_line(u32 line)
{
	/* No such instruction so invalidate everything */
	asm volatile(" mcr     p15, 0, %0, c7, c5, 0\n\t"
		     : : "r"(line) : );
}

void invalidate_bpredictor(void)
{
	/* FIXME: */
}

void invalidate_bpredictor_mva(virtual_addr_t mva)
{
	/* FIXME: */
}

void invalidate_dcache(void)
{
	/* FIXME: invalidate data cache */
}

void invalidate_dcache_mva(virtual_addr_t mva)
{
	asm volatile(" mcr     p15, 0, %0, c7, c6, 1\n\t"
		     : : "r"(mva) : );
}

void invalidate_dcache_line(u32 line)
{
	asm volatile(" mcr     p15, 0, %0, c7, c6, 2\n\t"
		     : : "r"(line) : );
}

void invalidate_idcache(void)
{
	u32 tmp = 0;
	/* invalidate instruction cache */
	asm volatile(" mcr     p15, 0, %0, c7, c5, 0\n\t"
		     : : "r"(tmp) : );
	/* FIXME: invalidate data cache */
}

void invalidate_idcache_mva(virtual_addr_t mva)
{
	asm volatile(" mcr     p15, 0, %0, c7, c5, 1\n\t"
		     " mcr     p15, 0, %0, c7, c6, 1\n\t"
		     : : "r"(mva) : );
}

void invalidate_idcache_line(u32 line)
{
	asm volatile(" mcr     p15, 0, %0, c7, c5, 2\n\t"
		     " mcr     p15, 0, %0, c7, c6, 2\n\t"
		     : : "r"(line) : );
}

void clean_dcache(void)
{	
	/* FIXME: */
}

void clean_dcache_mva(virtual_addr_t mva)
{
	asm volatile(" mcr     p15, 0, %0, c7, c10, 1\n\t"
		     : : "r"(mva) : );
}

void clean_dcache_mva_range(virtual_addr_t start, virtual_addr_t end)
{
	virtual_addr_t addr = start;
	while(addr < end) {
		asm volatile(" mcr     p15, 0, %0, c7, c10, 1\n\t"
			     : : "r"(addr) : );
		addr += ARCH_CACHE_LINE_SIZE;
	}
}

void clean_dcache_line(u32 line)
{
	asm volatile(" mcr     p15, 0, %0, c7, c10, 2\n\t"
		     : : "r"(line) : );
}

void clean_idcache(void)
{
	/* FIXME: */
}

void clean_idcache_mva(virtual_addr_t mva)
{
	/* Instruction cache does not require cleaning so,
	 * this operation reduces to cleaning of data cache.
	 */
	asm volatile(" mcr     p15, 0, %0, c7, c10, 1\n\t"
		     : : "r"(mva) : );
}

void clean_idcache_line(u32 line)
{
	/* Instruction cache does not require cleaning so,
	 * this operation reduces to cleaning of data cache.
	 */
	asm volatile(" mcr     p15, 0, %0, c7, c10, 2\n\t"
		     : : "r"(line) : );
}

void clean_invalidate_dcache(void)
{
	/* FIXME: */
}

void clean_invalidate_dcache_mva(virtual_addr_t mva)
{
	asm volatile(" mcr     p15, 0, %0, c7, c14, 1\n\t"
		     : : "r"(mva) : );
}

void clean_invalidate_dcache_mva_range(virtual_addr_t start, virtual_addr_t end)
{
	virtual_addr_t addr = start;
	while(addr < end) {
		asm volatile(" mcr     p15, 0, %0, c7, c14, 1\n\t"
			     : : "r"(addr) : );
		addr += ARCH_CACHE_LINE_SIZE;
	}
}

void clean_invalidate_dcache_line(u32 line)
{
	asm volatile(" mcr     p15, 0, %0, c7, c14, 2\n\t"
		     : : "r"(line) : );
}

void clean_invalidate_idcache(void)
{
	/* FIXME: */
}

void clean_invalidate_idcache_mva(virtual_addr_t mva)
{
	/* Instruction cache does not require cleaning so,
	 * this operation reduces to following:
	 *   1. Flush instruction cache
	 *   2. Clean & invalidate data cache
	 */
	asm volatile(" mcr     p15, 0, %0, c7, c5, 1\n\t"
		     " mcr     p15, 0, %0, c7, c14, 1\n\t"
		     : : "r"(mva) : );
}

void clean_invalidate_idcache_line(u32 line)
{
	/* Instruction cache does not require cleaning so,
	 * this operation reduces to following:
	 *   1. Flush entire instruction cache
	 *   2. Clean & invalidate data cache
	 */
	asm volatile(" mcr     p15, 0, %0, c7, c5, 0\n\t"
		     " mcr     p15, 0, %0, c7, c14, 2\n\t"
		     : : "r"(line) : );
}
