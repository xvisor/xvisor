/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file arch_io.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @author Jim Huang (jserv@0xlab.org)
 * @brief header file for CPU I/O or Memory read/write functions
 */
#ifndef _ARCH_IO_H__
#define _ARCH_IO_H__

#include <vmm_types.h>

static inline u16 arch_bswap16(u16 data)
{
	register u16 tmp = data;
	__asm__ __volatile__("rev16 %0, %0"
			     :"+l"(tmp));
	return tmp;
}

static inline u32 arch_bswap32(u32 data)
{
	register u32 tmp = data;
	__asm__ __volatile__("rev %0, %0"
			     :"+l"(tmp));
	return tmp;
}

/** FIXME: */
static inline u8 arch_ioreadb(volatile void *addr)
{
	return 0;
}

/** FIXME: */
static inline void arch_iowriteb(volatile void *addr, u8 data)
{
}

/** FIXME: */
static inline u16 arch_ioreadw(volatile void *addr)
{
	return 0;
}

/** FIXME: */
static inline void arch_iowritew(volatile void *addr, u16 data)
{
}

/** FIXME: */
static inline u32 arch_ioreadl(volatile void *addr)
{
	return 0;
}

/** FIXME: */
static inline void arch_iowritel(volatile void *addr, u32 data)
{
}

/** FIXME: */
static inline u8 arch_in_8(volatile u8 * addr)
{
	return *addr;
}

/** FIXME: */
static inline void arch_out_8(volatile u8 * addr, u8 data)
{
	*addr = data;
}

/** FIXME: */
static inline u16 arch_in_le16(volatile u16 * addr)
{
	return *addr;
}

/** FIXME: */
static inline void arch_out_le16(volatile u16 * addr, u16 data)
{
	*addr = data;
}

/** FIXME: */
static inline u16 arch_in_be16(volatile u16 * addr)
{
	return arch_bswap16(*addr);
}

/** FIXME: */
static inline void arch_out_be16(volatile u16 * addr, u16 data)
{
	*addr = arch_bswap16(data);
}

/** FIXME: */
static inline u32 arch_in_le32(volatile u32 * addr)
{
	return *addr;
}

/** FIXME: */
static inline void arch_out_le32(volatile u32 * addr, u32 data)
{
	*addr = data;
}

/** FIXME: */
static inline u32 arch_in_be32(volatile u32 * addr)
{
	return arch_bswap32(*addr);
}

/** FIXME: */
static inline void arch_out_be32(volatile u32 * addr, u32 data)
{
	*addr = arch_bswap32(data);
}

#endif
