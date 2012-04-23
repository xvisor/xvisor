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
 * @file cpu_cache.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of cache operations for ARMv7a family
 */
#include <cpu_cache.h>

void flush_icache(void)
{
	u32 tmp = 0;
	asm volatile(" mcr     p15, 0, %0, c7, c5, 0\n\t"
		     : : "r"(tmp) : );
}

void flush_icache_mva(virtual_addr_t mva)
{
	asm volatile(" mcr     p15, 0, %0, c7, c5, 1\n\t"
		     : : "r"(mva) : );
}

void flush_icache_line(u32 line)
{
	/* No such instruction so flush everything */
	asm volatile(" mcr     p15, 0, %0, c7, c5, 0\n\t"
		     : : "r"(line) : );
}

void flush_bpredictor(void)
{
	u32 tmp = 0;
	asm volatile(" mcr     p15, 0, %0, c7, c5, 6\n\t"
		     : : "r"(tmp) : );
}

void flush_bpredictor_mva(virtual_addr_t mva)
{
	asm volatile(" mcr     p15, 0, %0, c7, c5, 7\n\t"
		     : : "r"(mva) : );
}

void flush_dcache(void)
{
	/* FIXME: flush data cache */
}

void flush_dcache_mva(virtual_addr_t mva)
{
	asm volatile(" mcr     p15, 0, %0, c7, c6, 1\n\t"
		     : : "r"(mva) : );
}

void flush_dcache_line(u32 line)
{
	asm volatile(" mcr     p15, 0, %0, c7, c6, 2\n\t"
		     : : "r"(line) : );
}

void flush_idcache(void)
{
	u32 tmp = 0;
	/* flush instruction cache */
	asm volatile(" mcr     p15, 0, %0, c7, c5, 0\n\t"
		     : : "r"(tmp) : );
	/* FIXME: flush data cache */
}

void flush_idcache_mva(virtual_addr_t mva)
{
	asm volatile(" mcr     p15, 0, %0, c7, c5, 1\n\t"
		     " mcr     p15, 0, %0, c7, c6, 1\n\t"
		     : : "r"(mva) : );
}

void flush_idcache_line(u32 line)
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

void clean_flush_dcache(void)
{
	/* FIXME: */
}

void clean_flush_dcache_mva(virtual_addr_t mva)
{
	asm volatile(" mcr     p15, 0, %0, c7, c14, 1\n\t"
		     : : "r"(mva) : );
}

void clean_flush_dcache_line(u32 line)
{
	asm volatile(" mcr     p15, 0, %0, c7, c14, 2\n\t"
		     : : "r"(line) : );
}

void clean_flush_idcache(void)
{
	/* FIXME: */
}

void clean_flush_idcache_mva(virtual_addr_t mva)
{
	/* Instruction cache does not require cleaning so,
	 * this operation reduces to following:
	 *   1. Flush instruction cache
	 *   2. Clean & flush data cache
	 */
	asm volatile(" mcr     p15, 0, %0, c7, c5, 1\n\t"
		     " mcr     p15, 0, %0, c7, c11, 1\n\t"
		     : : "r"(mva) : );
}

void clean_flush_idcache_line(u32 line)
{
	/* Instruction cache does not require cleaning so,
	 * this operation reduces to following:
	 *   1. Flush entire instruction cache
	 *   2. Clean & flush data cache
	 */
	asm volatile(" mcr     p15, 0, %0, c7, c5, 0\n\t"
		     " mcr     p15, 0, %0, c7, c14, 2\n\t"
		     : : "r"(line) : );
}

