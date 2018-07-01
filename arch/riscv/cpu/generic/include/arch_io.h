/**
 * Copyright (c) 2018 Anup Patel.
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
#include <arch_barrier.h>

static inline u64 rev64(u64 v)
{
	return ((v & 0x00000000000000FFULL) << 56) | 
	       ((v & 0x000000000000FF00ULL) << 40) |
	       ((v & 0x0000000000FF0000ULL) << 24) |
	       ((v & 0x00000000FF000000ULL) << 8) |
	       ((v & 0x000000FF00000000ULL) >> 8) |
	       ((v & 0x0000FF0000000000ULL) >> 24) |
	       ((v & 0x00FF000000000000ULL) >> 40) |
	       ((v & 0xFF00000000000000ULL) >> 56);
}

static inline u32 rev32(u32 v)
{
	return ((v & 0x000000FF) << 24) | 
	       ((v & 0x0000FF00) << 8) |
	       ((v & 0x00FF0000) >> 8) |
	       ((v & 0xFF000000) >> 24);
}

static inline u16 rev16(u16 v)
{
	return ((v & 0x00FF) << 8) | 
	       ((v & 0xFF00) >> 8);
}

/* Generic IO read/write.  These perform native-endian accesses. */
static inline void __raw_write8(volatile void *addr, u8 val)
{
	asm volatile("sb %0, 0(%1)" : : "r" (val), "r" (addr));
}

static inline void __raw_write16(volatile void *addr, u16 val)
{
	asm volatile("sh %0, 0(%1)" : : "r" (val), "r" (addr));
}

static inline void __raw_write32(volatile void *addr, u32 val)
{
	asm volatile("sw %0, 0(%1)" : : "r" (val), "r" (addr));
}

static inline void __raw_write64(volatile void *addr, u64 val)
{
	asm volatile("sd %0, 0(%1)" : : "r" (val), "r" (addr));
}

static inline u8 __raw_read8(const volatile void *addr)
{
	u8 val;

	asm volatile("lb %0, 0(%1)" : "=r" (val) : "r" (addr));
	return val;
}

static inline u16 __raw_read16(const volatile void *addr)
{
	u16 val;

	asm volatile("lh %0, 0(%1)" : "=r" (val) : "r" (addr));
	return val;
}

static inline u32 __raw_read32(const volatile void *addr)
{
	u32 val;

	asm volatile("lw %0, 0(%1)" : "=r" (val) : "r" (addr));
	return val;
}

static inline u64 __raw_read64(const volatile void *addr)
{
	u64 val;

	asm volatile("ld %0, 0(%1)" : "=r" (val) : "r" (addr));
	return val;
}

#define __io_rbr()		do {} while (0)
#define __io_rar()		do {} while (0)
#define __io_rbw()		do {} while (0)
#define __io_raw()		do {} while (0)

#define __io_br()	do {} while (0)
#define __io_ar()	__asm__ __volatile__ ("fence i,r" : : : "memory");
#define __io_bw()	__asm__ __volatile__ ("fence w,o" : : : "memory");
#define __io_aw()	do {} while (0)

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
 * The RISC-V doesn't have special IO access instructions; all IO is memory
 * mapped.  Note that these are defined to perform little endian accesses
 * only. For RISC-V, IO port read/write operations translate to a read/write
 * operation to memory address. All IO port read/write operations are
 * assumed to be little-endian. 
 */
#define arch_outb(v, p)		{__io_bw(); __raw_write8(__io(p), v); __io_aw(); }
#define arch_outw(v, p)		{__io_bw(); __raw_write16(__io(p), v); __io_aw(); }
#define arch_outl(v, p)		{__io_bw(); __raw_write32(__io(p), v); __io_aw(); }
#define arch_inb(p)		({u8 v; __io_br(); v = __raw_read8(__io(p)); __io_ar(); v; })
#define arch_inw(p)		({u16 v; __io_br(); v = __raw_read16(__io(p)); __io_ar(); v; })
#define arch_inl(p)		({u32 v; __io_br(); v = __raw_read32(__io(p)); __io_ar(); v; })

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
#define arch_in_8(a)		({u8 v; __io_br(); v = __raw_read8(a); __io_ar(); v; })
#define arch_out_8(a, v)	{__io_bw(); __raw_write8(a, v); __io_aw(); }
#define arch_in_le16(a)		({u16 v; __io_br(); v = __raw_read16(a); __io_ar(); v; })
#define arch_out_le16(a, v)	{__io_bw(); __raw_write16(a, v); __io_aw(); }
#define arch_in_be16(a)		({u16 v; __io_br(); v = __raw_read16(a); __io_ar(); rev16(v); })
#define arch_out_be16(a, v)	{__io_bw(); __raw_write16(a, (rev16(v))); }
#define arch_in_le32(a)		({u32 v; __io_br(); v = __raw_read32(a); __io_ar(); v; })
#define arch_out_le32(a, v)	{__io_bw(); __raw_write32(a, v); __io_aw(); }
#define arch_in_be32(a)		({u32 v; __io_br(); v = __raw_read32(a); __io_ar(); rev32(v); })
#define arch_out_be32(a, v)	{__io_bw(); __raw_write32(a, rev32(v)); }
#define arch_in_le64(a)		({u32 v; __io_br(); v = __raw_read64(a); __io_ar(); v; })
#define arch_out_le64(a, v)	{__io_bw(); __raw_write64(a, v); __io_aw(); }
#define arch_in_be64(a)		({u32 v; __io_br(); v = __raw_read64(a); __io_ar(); rev64(v); })
#define arch_out_be64(a, v)	{__io_bw(); __raw_write64(a, rev64(v)); __io_aw(); }

#define arch_in_8_relax(a)		({u8 v; __io_rbr(); v = __raw_read8(a); __io_rar(); v; })
#define arch_out_8_relax(a, v)		{__io_rbw(); __raw_write8(a, v); __io_raw(); }
#define arch_in_le16_relax(a)		({u16 v; __io_rbr(); v = __raw_read16(a); __io_rar(); v; })
#define arch_out_le16_relax(a, v)	{__io_rbw(); __raw_write16(a, v); __io_raw(); }
#define arch_in_be16_relax(a)		({u16 v; __io_rbr(); v = __raw_read16(a); __io_rar(); rev16(v); })
#define arch_out_be16_relax(a, v)	{__io_rbw(); __raw_write16(a, (rev16(v))); __io_raw(); }
#define arch_in_le32_relax(a)		({u32 v; __io_rbr(); v = __raw_read32(a); __io_rar(); v; })
#define arch_out_le32_relax(a, v)	{__io_rbw(); __raw_write32(a, v); __io_raw(); }
#define arch_in_be32_relax(a)		({u32 v; __io_rbr(); v = __raw_read32(a); __io_rar(); rev32(v); })
#define arch_out_be32_relax(a, v)	{__io_rbw(); __raw_write32(a, rev32(v)); __io_raw(); }
#define arch_in_le64_relax(a)		({u32 v; __io_rbr(); v = __raw_read64(a); __io_rar(); v; })
#define arch_out_le64_relax(a, v)	{__io_rbw(); __raw_write64(a, v); __io_raw(); }
#define arch_in_be64_relax(a)		({u32 v; __io_rbr(); v = __raw_read64(a); __io_rar(); rev64(v); })
#define arch_out_be64_relax(a, v)	{__io_rbw(); __raw_write64(a, rev64(v)); __io_raw(); }

#endif
