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
 * @file arch_io.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for CPU I/O or Memory read/write functions
 */
#ifndef _ARCH_IO_H__
#define _ARCH_IO_H__

#include <vmm_types.h>
#include <common_io.h>

static inline u16 bswap16(u16 data)
{
	return (((data & 0xFF) << 8) |
		((data & 0xFF00) >> 8));
}

static inline u32 bswap32(u32 data)
{
	return (((data & 0xFF) << 24) |
		((data & 0xFF00) << 8) |
		((data & 0xFF0000) >> 8) |
		((data & 0xFF000000) >> 24));
}

static inline u64 bswap64(u64 data)
{
	return (u64)(bswap32((u32)(data >> 32))
		     | (((u64)(bswap32((u32)(data
					     & 0xFFFFFFFFUL))))
			<< 32));
}

/*
 * Endianness primitives
 * ------------------------
 */
#define arch_cpu_to_le16(v)	(v)

#define arch_le16_to_cpu(v)	(v)

#define arch_cpu_to_be16(v)	bswap16(v)

#define arch_be16_to_cpu(v)	bswap16(v)

#define arch_cpu_to_le32(v)	(v)

#define arch_le32_to_cpu(v)	(v)

#define arch_cpu_to_be32(v)	bswap32(v)

#define arch_be32_to_cpu(v)	bswap32(v)

#define arch_cpu_to_le64(v)	(v)

#define arch_le64_to_cpu(v)	(v)

#define arch_cpu_to_be64(v)	bswap64(v)

#define arch_be64_to_cpu(v)	bswap64(v)

/*
 * IO port access primitives
 * -------------------------
 */

BUILDIO(b, b, char)
BUILDIO(w, w, short)
BUILDIO(l, , int)

/*
 * Memory access primitives
 * ------------------------
 */
static inline u8 arch_in_8(volatile u8 *addr)
{
	return *addr;
}

static inline void arch_out_8(volatile u8 *addr, u8 data)
{
	*addr = data;
}

static inline u16 arch_in_le16(volatile u16 *addr)
{
	return bswap16(*addr);
}

static inline void arch_out_le16(volatile u16* addr, u16 data)
{
	*addr = data;
}

static inline u16 arch_in_be16(volatile u16 *addr)
{
	return bswap16(*addr);
}

static inline void arch_out_be16(volatile u16* addr, u16 data)
{
	*addr = bswap16(data);
}

static inline u32 arch_in_le32(volatile u32 *addr)
{
	return (*addr);
}

static inline void arch_out_le32(volatile u32* addr, u32 data)
{
	*addr = (data);
}

static inline u32 arch_in_be32(volatile u32 *addr)
{
	return bswap32(*addr);
}

static inline void arch_out_be32(volatile u32* addr, u32 data)
{
	*addr = bswap32(data);
}

static inline u64 arch_in_le64(volatile u64 *addr)
{
	return (*addr);
}

static inline void arch_out_le64(volatile u64* addr, u64 data)
{
	*addr = (data);
}

static inline u64 arch_in_be64(volatile u64 *addr)
{
	return bswap64(*addr);
}

static inline void arch_out_be64(volatile u64* addr, u64 data)
{
	*addr = bswap64(data);
}

#define arch_in_8_relax(a)		arch_in_8(a)
#define arch_out_8_relax(a, v)		arch_out_8(a, v)
#define arch_in_le16_relax(a)		arch_in_le16(a)
#define arch_out_le16_relax(a, v)	arch_out_le16(a, v)
#define arch_in_be16_relax(a)		arch_in_be16(a)
#define arch_out_be16_relax(a, v)	arch_out_be16(a, v)
#define arch_in_le32_relax(a)		arch_in_le32(a)
#define arch_out_le32_relax(a, v)	arch_out_le32(a, v)
#define arch_in_be32_relax(a)		arch_in_be32(a)
#define arch_out_be32_relax(a, v)	arch_out_be32(a, v)
#define arch_in_le64_relax(a)		arch_in_le64(a)
#define arch_out_le64_relax(a, v)	arch_out_le64(a, v)
#define arch_in_be64_relax(a)		arch_in_be64(a)
#define arch_out_be64_relax(a, v)	arch_out_be64(a, v)

#endif
