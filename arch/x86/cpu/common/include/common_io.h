/**
 * Copyright (c) 2012 Himanshu Chauhan.
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
 * @file common_io.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief All common I/O functions for x86.
 */

#ifndef __COMMON_IO_H
#define __COMMON_IO_H

#include <vmm_types.h>

#define BUILDIO(bwl, bw, type)						\
static inline void out##bwl(unsigned type value, int port)		\
{									\
	asm volatile("out" #bwl " %" #bw "0, %w1"			\
		     ::"a"(value), "Nd"(port));				\
}									\
static inline unsigned type in##bwl(int port)				\
{									\
	unsigned type value;						\
	asm volatile("in" #bwl " %w1, %" #bw "0"			\
		     : "=a"(value) : "Nd"(port));			\
	return value;							\
}									\
									\
static inline void outs##bwl(int port, const void *addr, unsigned long count) \
{									\
	asm volatile("rep; outs" #bwl					\
		     : "+S"(addr), "+c"(count) : "d"(port));		\
}									\
									\
static inline void ins##bwl(int port, void *addr, unsigned long count) \
{									\
	asm volatile("rep; ins" #bwl					\
		     : "+D"(addr), "+c"(count) : "d"(port));		\
}

BUILDIO(b, b, char)
BUILDIO(w, w, short)
BUILDIO(l, , int)

#endif /* __COMMON_IO_H */
