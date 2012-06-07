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
 * @author Anup Patel (anup@brainfault.org)
 * @author Jim Huang (jserv@0xlab.org)
 * @brief header file for CPU I/O or Memory read/write functions
 */
#ifndef _ARCH_IO_H__
#define _ARCH_IO_H__

#include <vmm_types.h>
#include <arch_barrier.h>
#include <cpu_inline_asm.h>

#define __raw_write8(a,v)	(*(volatile u8 *)(a) = (v))
#define __raw_write16(a,v)	(*(volatile u16 *)(a) = (v))
#define __raw_write32(a,v)	(*(volatile u32 *)(a) = (v))
#define __raw_write64(a,v)	(*(volatile u64 *)(a) = (v))

#define __raw_read8(a)		(*(volatile u8 *)(a))
#define __raw_read16(a)		(*(volatile u16 *)(a))
#define __raw_read32(a)		(*(volatile u32 *)(a))
#define __raw_read64(a)		(*(volatile u64 *)(a))

#define __iormb()		arch_rmb()
#define __iowmb()		arch_wmb()

/*
 * Endianness primitives
 * ------------------------
 */
#define arch_cpu_to_le16(v)	(v)

#define arch_le16_to_cpu(v)	(v)

#define arch_cpu_to_be16(v)	rev16(v)

#define arch_be16_to_cpu(v)	rev16(v)

#define arch_cpu_to_le32(v)	(v)

#define arch_le32_to_cpu(v)	(v)

#define arch_cpu_to_be32(v)	rev32(v)

#define arch_be32_to_cpu(v)	rev32(v)

/*
 * IO port access primitives
 * -------------------------
 *
 * The ARM doesn't have special IO access instructions; all IO is memory
 * mapped.  Note that these are defined to perform little endian accesses
 * only. For ARM, IO port read/write operations translate to a read/write
 * operation to memory address. All IO port read/write operations are
 * assumed to be little-endian. 
 */
#define arch_ioreadb(a)		({u8 v = __raw_read8(a); __iormb(); v; })

#define arch_iowriteb(a, v)	{__iowmb(); __raw_write8(a, v); }

#define arch_ioreadw(a)		({u16 v = __raw_read16(a); __iormb(); v; })

#define arch_iowritew(a, v)	{__iowmb(); __raw_write16(a, v); }

#define arch_ioreadl(a)		({u32 v = __raw_read32(a); __iormb(); v; })

#define arch_iowritel(a, v)	{__iowmb(); __raw_write32(a, v); }

/*
 * Memory access primitives
 * ------------------------
 */
#define arch_in_8(a)		({u8 v = __raw_read8(a); __iormb(); v; })

#define arch_out_8(a, v)	{__iowmb(); __raw_write8(a, v); }

#define arch_in_le16(a)		({u16 v = __raw_read16(a); __iormb(); v; })

#define arch_out_le16(a, v)	({__raw_write16(a, v); __iowmb(); })

#define arch_in_be16(a)		({u16 v = __raw_read16(a); __iormb(); rev16(v); })

#define arch_out_be16(a, v)	{__iowmb(); __raw_write16(a, (rev16(v))); }

#define arch_in_le32(a)		({u32 v = __raw_read32(a); __iormb(); v; })

#define arch_out_le32(a, v)	{__iowmb(); __raw_write32(a, v); }

#define arch_in_be32(a)		({u32 v = __raw_read32(a); __iormb(); rev32(v); })

#define arch_out_be32(a, v)	{__iowmb(); __raw_write32(a, rev32(v)); }

#define arch_in_le64(a)		({u32 v = __raw_read64(a); __iormb(); v; })

#define arch_out_le64(a, v)	{__iowmb(); __raw_write64(a, v); }

#define arch_in_be64(a)		({u32 v = __raw_read64(a); __iormb(); rev64(v); })

#define arch_out_be64(a, v)	{__iowmb(); __raw_write64(a, rev64(v)); }

#endif
