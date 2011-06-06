/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_cpu_io.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for CPU I/O or Memory read/write functions
 */
#ifndef _VMM_CPU_IO_H__
#define _VMM_CPU_IO_H__

#include <vmm_types.h>

static inline u16 cpu_bswap16(u16 data)
{
	return (((data & 0xFF) << 8) |
		((data & 0xFF00) >> 8));
}

static inline u32 cpu_bswap32(u32 data)
{
	return (((data & 0xFF) << 24) |
		((data & 0xFF00) << 8) |
		((data & 0xFF0000) >> 8) |
		((data & 0xFF000000) >> 24));
}

/** FIXME: */
static inline u8 vmm_cpu_ioreadb(volatile void *addr)
{
	return 0;
}

/** FIXME: */
static inline void vmm_cpu_iowriteb(volatile void *addr, u8 data)
{
}

/** FIXME: */
static inline u16 vmm_cpu_ioreadw(volatile void *addr)
{
	return 0;
}

/** FIXME: */
static inline void vmm_cpu_iowritew(volatile void *addr, u16 data)
{
}

/** FIXME: */
static inline u32 vmm_cpu_ioreadl(volatile void *addr)
{
	return 0;
}

/** FIXME: */
static inline void vmm_cpu_iowritel(volatile void *addr, u32 data)
{
}

/** FIXME: */
static inline u8 vmm_cpu_in_8(volatile u8 *addr)
{
	return *addr;
}

/** FIXME: */
static inline void vmm_cpu_out_8(volatile u8 *addr, u8 data)
{
	*addr = data;
}

/** FIXME: */
static inline u16 vmm_cpu_in_le16(volatile u16 *addr)
{
	return cpu_bswap16(*addr);
}

/** FIXME: */
static inline void vmm_cpu_out_le16(volatile u16* addr, u16 data)
{
	*addr = cpu_bswap16(data);
}

/** FIXME: */
static inline u16 vmm_cpu_in_be16(volatile u16 *addr)
{
	return *addr;
}

/** FIXME: */
static inline void vmm_cpu_out_be16(volatile u16* addr, u16 data)
{
	*addr = data;
}

/** FIXME: */
static inline u32 vmm_cpu_in_le32(volatile u32 *addr)
{
	return cpu_bswap32(*addr);
}

/** FIXME: */
static inline void vmm_cpu_out_le32(volatile u32* addr, u32 data)
{
	*addr = cpu_bswap32(data);
}

/** FIXME: */
static inline u32 vmm_cpu_in_be32(volatile u32 *addr)
{
	return *addr;
}

/** FIXME: */
static inline void vmm_cpu_out_be32(volatile u32* addr, u32 data)
{
	*addr = data;
}

#endif
