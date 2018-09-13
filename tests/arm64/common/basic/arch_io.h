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
 * @brief Header file for common I/O functions
 */

#ifndef __ARCH_IO_H__
#define __ARCH_IO_H__

#include <arch_types.h>

static inline u32 arch_readl(void *addr)
{
	return *((u32 *)addr);
}

static inline void arch_writel(u32 data, void *addr)
{
	*((u32 *)addr) = data;
}

static inline u16 arch_readw(void * addr)
{
	return *((u16 *)addr);
}

static inline void arch_writew(u16 data, void * addr)
{
	*((u16 *)addr) = data;
}

static inline u8 arch_readb(void * addr)
{
	return *((u8 *)addr);
}

static inline void arch_writeb(u8 data, void * addr)
{
	*((u8 *)addr) = data;
}

#endif /* __ARCH_IO_H__ */
