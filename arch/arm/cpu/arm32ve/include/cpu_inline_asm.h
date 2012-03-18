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
 * @file cpu_inline_asm.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief  Frequently required inline assembly macros
 */
#ifndef __CPU_INLINE_ASM_H__
#define __CPU_INLINE_ASM_H__

#include <vmm_types.h>

#define rev(val)		({ u32 rval; asm volatile(\
				" rev     %0, %1\n\t" : "=r" (rval) : \
				"r" (val) : "memory", "cc"); rval;})

#define rev16(val)		({ u16 rval; asm volatile(\
				" rev16   %0, %1\n\t" : "=r" (rval) : \
				"r" (val) : "memory", "cc"); rval;})

#define read_sctlr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c1, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_sctlr(val)	asm volatile(\
				" mcr     p15, 0, %0, c1, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_dacr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c3, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_dacr(val)		asm volatile(\
				" mcr     p15, 0, %0, c3, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_ttbr0()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c2, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_ttbr0(val)	asm volatile(\
				" mcr     p15, 0, %0, c2, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_ttbr1()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c2, c0, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_ttbr1(val)	asm volatile(\
				" mcr     p15, 0, %0, c2, c0, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_ttbcr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c2, c0, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_ttbcr(val)	asm volatile(\
				" mcr     p15, 0, %0, c2, c0, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_dfsr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c5, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_dfsr(val)		asm volatile(\
				" mcr     p15, 0, %0, c5, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_ifsr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c5, c0, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_ifsr(val)		asm volatile(\
				" mcr     p15, 0, %0, c5, c0, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_dfar()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c6, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_dfar(val)		asm volatile(\
				" mcr     p15, 0, %0, c6, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_ifar()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c6, c0, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_ifar(val)		asm volatile(\
				" mcr     p15, 0, %0, c6, c0, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define invalid_tlb()		({ u32 rval=0; asm volatile(\
				" mcr     p15, 0, %0, c8, c7, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define invalid_tlb_line(va)	asm volatile(\
				" mcr     p15, 0, %0, c8, c7, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define invalid_i_tlb()		({ u32 rval=0; asm volatile(\
				" mcr     p15, 0, %0, c8, c5, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define invalid_i_tlb_line(va)	asm volatile(\
				" mcr     p15, 0, %0, c8, c5, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define invalid_d_tlb()		({ u32 rval=0; asm volatile(\
				" mcr     p15, 0, %0, c8, c6, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define invalid_d_tlb_line(va)	asm volatile(\
				" mcr     p15, 0, %0, c8, c6, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define read_contextidr()	({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c13, c0, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_contextidr(val)	asm volatile(\
				" mcr     p15, 0, %0, c13, c0, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_tpidrurw()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c13, c0, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_tpidrurw(val)		asm volatile(\
				" mcr     p15, 0, %0, c13, c0, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_tpidruro()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c13, c0, 3\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_tpidruro(val)		asm volatile(\
				" mcr     p15, 0, %0, c13, c0, 3\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_tpidrprw()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c13, c0, 4\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_tpidrprw(val)		asm volatile(\
				" mcr     p15, 0, %0, c13, c0, 4\n\t" \
				:: "r" ((val)) : "memory", "cc")

#endif
