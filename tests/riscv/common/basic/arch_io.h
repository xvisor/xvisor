/**
 * Copyright (c) 2019 Anup Patel.
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
 * @brief Header file for common I/O functions
 */

#ifndef __ARCH_IO_H__
#define __ARCH_IO_H__

#include <arch_types.h>

#define __io_br()	do {} while (0)
#define __io_ar()	__asm__ __volatile__ ("fence i,r" : : : "memory");
#define __io_bw()	__asm__ __volatile__ ("fence w,o" : : : "memory");
#define __io_aw()	do {} while (0)

static inline u32 arch_readl(void *addr)
{
	u32 v;
	__io_br();
	v = *((u32 *)addr);
	__io_ar();
	return v;
}

static inline void arch_writel(u32 data, void *addr)
{
	__io_bw();
	*((u32 *)addr) = data;
	__io_aw();
}

static inline u16 arch_readw(void * addr)
{
	u16 v;
	__io_br();
	v = *((u16 *)addr);
	__io_ar();
	return v;
}

static inline void arch_writew(u16 data, void *addr)
{
	__io_bw();
	*((u16 *)addr) = data;
	__io_aw();
}

static inline u8 arch_readb(void *addr)
{
	u8 v;
	__io_br();
	v = *((u8 *)addr);
	__io_ar();
	return v;
}

static inline void arch_writeb(u8 data, void *addr)
{
	__io_bw();
	*((u8 *)addr) = data;
	__io_aw();
}

#endif /* __ARCH_IO_H__ */
