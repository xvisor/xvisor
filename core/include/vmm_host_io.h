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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for common io functions.
 */

#ifndef __VMM_HOST_IO_H_
#define __VMM_HOST_IO_H_

#include <vmm_types.h>
#include <arch_io.h>

/** Endianness related helper macros */
#define vmm_cpu_to_le16(data)	arch_cpu_to_le16(data)

#define vmm_le16_to_cpu(data)	arch_le16_to_cpu(data)

#define vmm_cpu_to_be16(data)	arch_cpu_to_be16(data)

#define vmm_be16_to_cpu(data)	arch_be16_to_cpu(data)

#define vmm_cpu_to_le32(data)	arch_cpu_to_le32(data)

#define vmm_le32_to_cpu(data)	arch_le32_to_cpu(data)

#define vmm_cpu_to_be32(data)	arch_cpu_to_be32(data)

#define vmm_be32_to_cpu(data)	arch_be32_to_cpu(data)

#define vmm_cpu_to_le64(data)	arch_cpu_to_le64(data)

#define vmm_le64_to_cpu(data)	arch_le64_to_cpu(data)

#define vmm_cpu_to_be64(data)	arch_cpu_to_be64(data)

#define vmm_be64_to_cpu(data)	arch_be64_to_cpu(data)

/** I/O space access functions (Assumed to be Little Endian) */
static inline u8 vmm_inb(unsigned long port)
{
	return arch_inb(port);
}

static inline u16 vmm_inw(unsigned long port)
{
	return arch_inw(port);
}

static inline u32 vmm_inl(unsigned long port)
{
	return arch_inl(port);
}

static inline void vmm_outb(u8 value, unsigned long port)
{
	arch_outb(value, port);
}

static inline void vmm_outw(u16 value, unsigned long port)
{
	arch_outw(value, port);
}

static inline void vmm_outl(u32 value, unsigned long port)
{
	arch_outl(value, port);
}

static inline u8 vmm_inb_p(unsigned long port)
{
	return arch_inb_p(port);
}

static inline u16 vmm_inw_p(unsigned long port)
{
	return arch_inw_p(port);
}

static inline u32 vmm_inl_p(unsigned long port)
{
	return arch_inl_p(port);
}

static inline void vmm_outb_p(u8 value, unsigned long port)
{
	arch_outb_p(value, port);
}

static inline void vmm_outw_p(u16 value, unsigned long port)
{
	arch_outw_p(value, port);
}

static inline void vmm_outl_p(u32 value, unsigned long port)
{
	arch_outl_p(value, port);
}

static inline void vmm_insb(unsigned long port, void *buffer, int count)
{
	arch_insb(port, buffer, count);
}

static inline void vmm_insw(unsigned long port, void *buffer, int count)
{
	arch_insw(port, buffer, count);
}

static inline void vmm_insl(unsigned long port, void *buffer, int count)
{
	arch_insl(port, buffer, count);
}

static inline void vmm_outsb(unsigned long port, const void *buffer, int count)
{
	arch_outsb(port, buffer, count);
}

static inline void vmm_outsw(unsigned long port, const void *buffer, int count)
{
	arch_outsw(port, buffer, count);
}

static inline void vmm_outsl(unsigned long port, const void *buffer, int count)
{
	arch_outsl(port, buffer, count);
}

/** Memory read/write legacy functions (Assumed to be Little Endian) */
static inline u8 vmm_readb(volatile void *addr)
{
	return arch_in_8(addr);
}

static inline void vmm_writeb(u8 data, volatile void *addr)
{
	arch_out_8(addr, data);
}

static inline u16 vmm_readw(volatile void *addr)
{
	return arch_in_le16(addr);
}

static inline void vmm_writew(u16 data, volatile void *addr)
{
	arch_out_le16(addr, data);
}

static inline u32 vmm_readl(volatile void *addr)
{
	return arch_in_le32(addr);
}

static inline void vmm_writel(u32 data, volatile void *addr)
{
	arch_out_le32(addr, data);
}

static inline u32 vmm_readq(volatile void *addr)
{
	return arch_in_le64(addr);
}

static inline void vmm_writeq(u32 data, volatile void *addr)
{
	arch_out_le64(addr, data);
}

static inline void vmm_readsb(volatile void *addr, void *buffer, int len)
{
	if (len) {
		u8 *buf = buffer;
		do {
			u8 x = vmm_readb(addr);
			*buf++ = x;
		} while (--len);
	}
}

static inline void vmm_readsw(volatile void *addr, void *buffer, int len)
{
	if (len) {
		u16 *buf = buffer;
		do {
			u16 x = vmm_readw(addr);
			*buf++ = x;
		} while (--len);
	}
}

static inline void vmm_readsl(volatile void *addr, void *buffer, int len)
{
	if (len) {
		u32 *buf = buffer;
		do {
			u32 x = vmm_readl(addr);
			*buf++ = x;
		} while (--len);
	}
}

static inline void vmm_writesb(volatile void *addr, const void *buffer, int len)
{
	if (len) {
		const u8 *buf = buffer;
		do {
			vmm_writeb(*buf++, addr);
		} while (--len);
	}
}

static inline void vmm_writesw(volatile void *addr, const void *buffer, int len)
{
	if (len) {
		const u16 *buf = buffer;
		do {
			vmm_writew(*buf++, addr);
		} while (--len);
	}
}

static inline void vmm_writesl(volatile void *addr, const void *buffer, int len)
{
	if (len) {
		const u32 *buf = buffer;
		do {
			vmm_writel(*buf++, addr);
		} while (--len);
	}
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

static inline u64 vmm_in_le64(volatile u64 *addr)
{
	return arch_in_le64(addr);
}

static inline void vmm_out_le64(volatile u64 *addr, u64 data)
{
	arch_out_le64(addr, data);
}

static inline u64 vmm_in_be64(volatile u64 *addr)
{
	return arch_in_be64(addr);
}

static inline void vmm_out_be64(volatile u64 *addr, u64 data)
{
	arch_out_be64(addr, data);
}

#endif /* __VMM_HOST_IO_H_ */
