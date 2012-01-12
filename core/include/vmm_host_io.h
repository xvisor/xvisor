/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_host_io.h
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for common io functions.
 */

#ifndef __VMM_HOST_IO_H_
#define __VMM_HOST_IO_H_

#include <vmm_types.h>
#include <arch_io.h>

/** I/O read/write legacy functions (Assumed to be Little Endian) */
static inline u8 vmm_ioreadb(volatile void *addr)
{
	return arch_ioreadb(addr);
}

static inline void vmm_iowriteb(volatile void *addr, u8 data)
{
	arch_iowriteb(addr, data);
}

static inline u16 vmm_ioreadw(volatile void *addr)
{
	return arch_ioreadw(addr);
}

static inline void vmm_iowritew(volatile void *addr, u16 data)
{
	arch_iowritew(addr, data);
}

static inline u32 vmm_ioreadl(volatile void *addr)
{
	return arch_ioreadw(addr);
}

static inline void vmm_iowritel(volatile void *addr, u32 data)
{
	arch_iowritew(addr, data);
}

/** Memory read/write legacy functions (Assumed to be Little Endian) */
static inline u8 vmm_readb(volatile void *addr)
{
	return arch_in_8((u8 *) addr);
}

static inline void vmm_writeb(u8 data, volatile void *addr)
{
	arch_out_8((u8 *) addr, data);
}

static inline u16 vmm_readw(volatile void *addr)
{
	return arch_in_le16((u16 *) addr);
}

static inline void vmm_writew(u16 data, volatile void *addr)
{
	arch_out_le16((u16 *) addr, data);
}

static inline u32 vmm_readl(volatile void *addr)
{
	return arch_in_le32((u32 *) addr);
}

static inline void vmm_writel(u32 data, volatile void *addr)
{
	arch_out_le32((u32 *) addr, data);
}

/** Memory read/write functions */
static inline u8 vmm_in_8(volatile u8 * addr)
{
	return arch_in_8(addr);
}

static inline void vmm_out_8(volatile u8 * addr, u8 data)
{
	arch_out_8(addr, data);
}

static inline u16 vmm_in_le16(volatile u16 * addr)
{
	return arch_in_le16(addr);
}

static inline void vmm_out_le16(volatile u16 * addr, u16 data)
{
	arch_out_le16(addr, data);
}

static inline u16 vmm_in_be16(volatile u16 * addr)
{
	return arch_in_be16(addr);
}

static inline void vmm_out_be16(volatile u16 * addr, u16 data)
{
	arch_out_be16(addr, data);
}

static inline u32 vmm_in_le32(volatile u32 * addr)
{
	return arch_in_le32(addr);
}

static inline void vmm_out_le32(volatile u32 * addr, u32 data)
{
	arch_out_le32(addr, data);
}

static inline u32 vmm_in_be32(volatile u32 * addr)
{
	return arch_in_be32(addr);
}

static inline void vmm_out_be32(volatile u32 * addr, u32 data)
{
	arch_out_be32(addr, data);
}

#endif /* __VMM_HOST_IO_H_ */
