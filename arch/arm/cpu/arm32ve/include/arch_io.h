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

#define arch_cpu_to_le64(v)	(v)

#define arch_le64_to_cpu(v)	(v)

#define arch_cpu_to_be64(v)	rev64(v)

#define arch_be64_to_cpu(v)	rev64(v)

#define __io(p)			((void *)p)

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
#define arch_outb(v, p)		{__iowmb(); __raw_write8(__io(p), v); }
#define arch_outw(v, p)		{__iowmb(); __raw_write16(__io(p), v); }
#define arch_outl(v, p)		{__iowmb(); __raw_write32(__io(p), v); }
#define arch_inb(p)		({u8 v = __raw_read8(__io(p)); __iormb(); v; })
#define arch_inw(p)		({u16 v = __raw_read16(__io(p)); __iormb(); v; })
#define arch_inl(p)		({u32 v = __raw_read32(__io(p)); __iormb(); v; })

#define arch_outb_p(v, p)	arch_outb((v), (p))
#define arch_outw_p(v, p)	arch_outw((v), (p))
#define arch_outl_p(v, p)	arch_outl((v), (p))
#define arch_inb_p(p)		arch_inb((p))
#define arch_inw_p(p)		arch_inw((p))
#define arch_inl_p(p)		arch_inl((p))

static inline void arch_insb(unsigned long p, void *b, int c)
{
	if (c) {
		u8 *buf = b;
		do {
			u8 x = arch_inb(p);
			*buf++ = x;
		} while (--c);
	}
}

static inline void arch_insw(unsigned long p, void *b, int c)
{
	if (c) {
		u16 *buf = b;
		do {
			u16 x = arch_inw(p);
			*buf++ = x;
		} while (--c);
	}
}

static inline void arch_insl(unsigned long p, void *b, int c)
{
	if (c) {
		u32 *buf = b;
		do {
			u32 x = arch_inl(p);
			*buf++ = x;
		} while (--c);
	}
}

static inline void arch_outsb(unsigned long p, const void *b, int c)
{
	if (c) {
		const u8 *buf = b;
		do {
			arch_outb(*buf++, p);
		} while (--c);
	}
}

static inline void arch_outsw(unsigned long p, const void *b, int c)
{
	if (c) {
		const u16 *buf = b;
		do {
			arch_outw(*buf++, p);
		} while (--c);
	}
}

static inline void arch_outsl(unsigned long p, const void *b, int c)
{
	if (c) {
		const u32 *buf = b;
		do {
			arch_outl(*buf++, p);
		} while (--c);
	}
}

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
