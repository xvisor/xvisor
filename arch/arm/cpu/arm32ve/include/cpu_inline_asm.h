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
#include <vmm_compiler.h>
#include <cpu_defines.h>

#define rev32(val)		({ u32 rval; asm volatile(\
				" rev     %0, %1\n\t" : "=r" (rval) : \
				"r" (val) : "memory", "cc"); rval;})

#define rev64(val)		({ u32 d1, d2; \
				d1 = (u32)((u64)val >> 32); d2 = (u32)val; \
				d1 = rev32(d1); d2 = rev32(d2); \
				(((u64)d2 << 32) | ((u64)d1));})

#define rev16(val)		({ u16 rval; asm volatile(\
				" rev16   %0, %1\n\t" : "=r" (rval) : \
				"r" (val) : "memory", "cc"); rval;})

#define ldrex(addr, data)	asm volatile("ldrex	%0, [%1]\n\t" \
				: "=r"(data) : "r"(addr))

#define strex(addr, data, res)	asm volatile("strex	%0, %1, [%2]\n\t" \
				: "=r"(res) : "r"(data), "r"(addr))

#define clrex()			asm volatile("clrex\n\t")

#define __ACCESS_CP15(CRn, Op1, CRm, Op2)	\
	"mrc", "mcr", stringify(p15, Op1, %0, CRn, CRm, Op2), u32
#define __ACCESS_CP15_64(Op1, CRm)		\
	"mrrc", "mcrr", stringify(p15, Op1, %Q0, %R0, CRm), u64

#define __read_sysreg(r, w, c, t) ({				\
	t __val;						\
	asm volatile(r " " c : "=r" (__val));			\
	__val;							\
})
#define read_sysreg(...)		__read_sysreg(__VA_ARGS__)

#define __write_sysreg(v, r, w, c, t)	asm volatile(w " " c : : "r" ((t)(v)))
#define write_sysreg(v, ...)		__write_sysreg(v, __VA_ARGS__)

/* General CP14 Register Read/Write */

#define read_teecr()		({ u32 rval; asm volatile(\
				" mrc     p14, 6, %0, c0, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_teecr(val)	asm volatile(\
				" mcr     p14, 6, %0, c0, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_teehbr()		({ u32 rval; asm volatile(\
				" mrc     p14, 6, %0, c1, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_teehbr(val)	asm volatile(\
				" mcr     p14, 6, %0, c1, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

/* General CP15 Register Read/Write */

#define read_ctr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c0, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_mpidr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c0, 5\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_midr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_ccsidr()		({ u32 rval; asm volatile(\
				" mrc     p15, 1, %0, c0, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_clidr()		({ u32 rval; asm volatile(\
				" mrc     p15, 1, %0, c0, c0, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_csselr()		({ u32 rval; asm volatile(\
				" mrc     p15, 2, %0, c0, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_csselr(val)	asm volatile(\
				" mcr     p15, 2, %0, c0, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_pfr0()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c1, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_pfr1()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c1, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_dfr0()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c1, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_afr0()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c1, 3\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_mmfr0()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c1, 4\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_mmfr1()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c1, 5\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_mmfr2()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c1, 6\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_mmfr3()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c1, 7\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_isar0()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c2, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_isar1()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c2, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_isar2()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c2, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_isar3()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c2, 3\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_isar4()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c2, 4\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_isar5()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c2, 5\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_sctlr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c1, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_sctlr(val)	asm volatile(\
				" mcr     p15, 0, %0, c1, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_cpacr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c1, c0, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_cpacr(val)	asm volatile(\
				" mcr     p15, 0, %0, c1, c0, 2\n\t" \
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

#define read_ttbr0_long()	({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 0, %0, %1, c2\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define write_ttbr0_long(val)	asm volatile(\
				" mcrr     p15, 0, %0, %1, c2\n\t" \
				:: "r" ((val) & 0xFFFFFFFF), "r" ((val) >> 32) \
				: "memory", "cc")

#define read_ttbr1()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c2, c0, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_ttbr1(val)	asm volatile(\
				" mcr     p15, 0, %0, c2, c0, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_ttbr1_long()	({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 1, %0, %1, c2\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define write_ttbr1_long(val)	asm volatile(\
				" mcrr     p15, 1, %0, %1, c2\n\t" \
				:: "r" ((val) & 0xFFFFFFFF), "r" ((val) >> 32) \
				: "memory", "cc")

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

#define read_adfsr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c5, c1, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_adfsr(val)	asm volatile(\
				" mcr     p15, 0, %0, c5, c1, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_aifsr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c5, c1, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_aifsr(val)	asm volatile(\
				" mcr     p15, 0, %0, c5, c1, 1\n\t" \
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

#define va2pa_c_pr(va)		asm volatile(\
				" mcr     p15, 0, %0, c7, c8, 0\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define va2pa_c_pw(va)		asm volatile(\
				" mcr     p15, 0, %0, c7, c8, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define va2pa_c_ur(va)		asm volatile(\
				" mcr     p15, 0, %0, c7, c8, 2\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define va2pa_c_uw(va)		asm volatile(\
				" mcr     p15, 0, %0, c7, c8, 3\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define va2pa_ns_pr(va)		asm volatile(\
				" mcr     p15, 0, %0, c7, c8, 4\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define va2pa_ns_pw(va)		asm volatile(\
				" mcr     p15, 0, %0, c7, c8, 5\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define va2pa_ns_ur(va)		asm volatile(\
				" mcr     p15, 0, %0, c7, c8, 6\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define va2pa_ns_uw(va)		asm volatile(\
				" mcr     p15, 0, %0, c7, c8, 7\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define read_par()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c7, c4, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_par(val)		asm volatile(\
				" mcr     p15, 0, %0, c7, c4, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_par64()		({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 0, %0, %1, c7\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define write_par64(val)	asm volatile(\
				" mcrr     p15, 0, %0, %1, c7\n\t" \
				:: "r" ((val) & 0xFFFFFFFF), "r" ((val) >> 32) \
				: "memory", "cc")

#define read_prrr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c10, c2, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_prrr(val)		asm volatile(\
				" mcr     p15, 0, %0, c10, c2, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_nmrr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c10, c2, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_nmrr(val)		asm volatile(\
				" mcr     p15, 0, %0, c10, c2, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_vbar()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c12, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_vbar(val)		asm volatile(\
				" mcr     p15, 0, %0, c12, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_fcseidr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c13, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_fcseidr(val)	asm volatile(\
				" mcr     p15, 0, %0, c13, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_contextidr()	({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c13, c0, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_contextidr(val)	asm volatile(\
				" mcr     p15, 0, %0, c13, c0, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_tpidrurw()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c13, c0, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_tpidrurw(val)	asm volatile(\
				" mcr     p15, 0, %0, c13, c0, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_tpidruro()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c13, c0, 3\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_tpidruro(val)	asm volatile(\
				" mcr     p15, 0, %0, c13, c0, 3\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_tpidrprw()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c13, c0, 4\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_tpidrprw(val)	asm volatile(\
				" mcr     p15, 0, %0, c13, c0, 4\n\t" \
				:: "r" ((val)) : "memory", "cc")

/* TLB maintainence */

#define inv_utlb_all()		({ u32 rval=0; asm volatile(\
				" mcr     p15, 0, %0, c8, c7, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define inv_utlb_line(va)	asm volatile(\
				" mcr     p15, 0, %0, c8, c7, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define inv_itlb_all()		({ u32 rval=0; asm volatile(\
				" mcr     p15, 0, %0, c8, c5, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define inv_itlb_line(va)	asm volatile(\
				" mcr     p15, 0, %0, c8, c5, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define inv_dtlb_all()		({ u32 rval=0; asm volatile(\
				" mcr     p15, 0, %0, c8, c6, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define inv_dtlb_line(va)	asm volatile(\
				" mcr     p15, 0, %0, c8, c6, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define inv_tlb_guest_all()	({ u32 rval=0; asm volatile(\
				" mcr     p15, 4, %0, c8, c7, 4\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define inv_tlb_guest_allis()	({ u32 rval=0; asm volatile(\
				" mcr     p15, 4, %0, c8, c3, 4\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define inv_tlb_hyp_all()	({ u32 rval=0; asm volatile(\
				" mcr     p15, 4, %0, c8, c7, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define inv_tlb_hyp_allis()	({ u32 rval=0; asm volatile(\
				" mcr     p15, 4, %0, c8, c3, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define inv_tlb_hyp_mva(va)	asm volatile(\
				" mcr     p15, 4, %0, c8, c7, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define inv_tlb_hyp_mvais(va)	asm volatile(\
				" mcr     p15, 4, %0, c8, c3, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define va2pa_hr(va)		asm volatile(\
				" mcr     p15, 4, %0, c7, c8, 0\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define va2pa_hw(va)		asm volatile(\
				" mcr     p15, 4, %0, c7, c8, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

/* VFP Control Register Read/Write */

#define read_fpexc()		({ u32 rval; asm volatile(\
				" mrc p10, 7, %0, c8, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_fpexc(val)	asm volatile(\
				" mcr p10, 7, %0, c8, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_fpscr()		({ u32 rval; asm volatile(\
				" mrc p10, 7, %0, c1, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_fpscr(val)	asm volatile(\
				" mcr p10, 7, %0, c1, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_fpsid()		({ u32 rval; asm volatile(\
				" mrc p10, 7, %0, c0, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_fpsid(val)	asm volatile(\
				" mcr p10, 7, %0, c0, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_fpinst()		({ u32 rval; asm volatile(\
				" mrc p10, 7, %0, c9, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_fpinst(val)	asm volatile(\
				" mcr p10, 7, %0, c9, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_fpinst2()		({ u32 rval; asm volatile(\
				" mrc p10, 7, %0, c10, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_fpinst2(val)	asm volatile(\
				" mcr p10, 7, %0, c10, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_mvfr0()		({ u32 rval; asm volatile(\
				" mrc p10, 7, %0, c7, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_mvfr0(val)	asm volatile(\
				" mcr p10, 7, %0, c7, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_mvfr1()		({ u32 rval; asm volatile(\
				" mrc p10, 7, %0, c6, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_mvfr1(val)	asm volatile(\
				" mcr p10, 7, %0, c6, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

/* Virtualization Extension Register Read/Write */

#define read_vpidr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c0, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_vpidr(val)	asm volatile(\
				" mcr     p15, 4, %0, c0, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_vmpidr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c0, c0, 5\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_vmpidr(val)	asm volatile(\
				" mcr     p15, 4, %0, c0, c0, 5\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hsctlr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c1, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hsctlr(val)	asm volatile(\
				" mcr     p15, 4, %0, c1, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hactlr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c1, c0, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hactlr(val)	asm volatile(\
				" mcr     p15, 4, %0, c1, c0, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hcr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c1, c1, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hcr(val)		asm volatile(\
				" mcr     p15, 4, %0, c1, c1, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hdctlr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c1, c1, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hdctlr(val)	asm volatile(\
				" mcr     p15, 4, %0, c1, c1, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hcptr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c1, c1, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hcptr(val)	asm volatile(\
				" mcr     p15, 4, %0, c1, c1, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hstr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c1, c1, 3\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hstr(val)		asm volatile(\
				" mcr     p15, 4, %0, c1, c1, 3\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hacr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c1, c1, 7\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hacr(val)		asm volatile(\
				" mcr     p15, 4, %0, c1, c1, 7\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_vttbr()		({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 6, %0, %1, c2\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define write_vttbr(val)	asm volatile(\
				" mcrr     p15, 6, %0, %1, c2\n\t" \
				:: "r" ((val) & 0xFFFFFFFF), "r" ((val) >> 32) \
				: "memory", "cc")

#define read_httbr()		({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 4, %0, %1, c2\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define write_httbr(val)	asm volatile(\
				" mcrr     p15, 4, %0, %1, c2\n\t" \
				:: "r" ((val) & 0xFFFFFFFF), "r" ((val) >> 32) \
				: "memory", "cc")

#define read_vtcr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c2, c1, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_vtcr(val)		asm volatile(\
				" mcr     p15, 4, %0, c2, c1, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_htcr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c2, c0, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_htcr(val)		asm volatile(\
				" mcr     p15, 4, %0, c2, c0, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hadfsr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c5, c1, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hadfsr(val)	asm volatile(\
				" mcr     p15, 4, %0, c5, c1, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_haifsr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c5, c1, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_haifsr(val)	asm volatile(\
				" mcr     p15, 4, %0, c5, c1, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hsr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c5, c2, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hsr(val)		asm volatile(\
				" mcr     p15, 4, %0, c5, c2, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hdfar()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c6, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hdfar(val)	asm volatile(\
				" mcr     p15, 4, %0, c6, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hifar()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c6, c0, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hifar(val)	asm volatile(\
				" mcr     p15, 4, %0, c6, c0, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hpfar()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c6, c0, 4\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hpfar(val)	asm volatile(\
				" mcr     p15, 4, %0, c6, c0, 4\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hmair0()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c10, c2, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hmair0(val)	asm volatile(\
				" mcr     p15, 4, %0, c10, c2, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hmair1()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c10, c2, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hmair1(val)	asm volatile(\
				" mcr     p15, 4, %0, c10, c2, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_hvbar()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c12, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_hvbar(val)	asm volatile(\
				" mcr     p15, 4, %0, c12, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_htpidr()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c13, c0, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_htpidr(val)	asm volatile(\
				" mcr     p15, 4, %0, c13, c0, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")

#if defined(CONFIG_ARM_GENERIC_TIMER)

#define read_cntfrq()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c14, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_cntfrq(val)	asm volatile(\
				" mcr     p15, 0, %0, c14, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_cnthctl()		({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c14, c1, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_cnthctl(val)	asm volatile(\
				" mcr     p15, 4, %0, c14, c1, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_cnthp_ctl()	({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c14, c2, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_cnthp_ctl(val)	asm volatile(\
				" mcr     p15, 4, %0, c14, c2, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_cnthp_cval()	({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 6, %0, %1, c14\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define write_cnthp_cval(val)	asm volatile(\
				" mcrr     p15, 6, %0, %1, c14\n\t" \
				:: "r" ((val) & 0xFFFFFFFF), "r" ((val) >> 32) \
				: "memory", "cc")

#define read_cnthp_tval()	({ u32 rval; asm volatile(\
				" mrc     p15, 4, %0, c14, c2, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_cnthp_tval(val)	asm volatile(\
				" mcr     p15, 4, %0, c14, c2, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_cntkctl()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c14, c1, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_cntkctl(val)	asm volatile(\
				" mcr     p15, 0, %0, c14, c1, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_cntp_ctl()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c14, c2, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_cntp_ctl(val)	asm volatile(\
				" mcr     p15, 0, %0, c14, c2, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_cntp_cval()	({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 2, %0, %1, c14\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define write_cntp_cval(val)	asm volatile(\
				" mcrr     p15, 2, %0, %1, c14\n\t" \
				:: "r" ((val) & 0xFFFFFFFF), "r" ((val) >> 32) \
				: "memory", "cc")

#define read_cntp_tval()	({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c14, c2, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_cntp_tval(val)	asm volatile(\
				" mcr     p15, 0, %0, c14, c2, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_cntpct()		({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 0, %0, %1, c14\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define read_cntv_ctl()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c14, c3, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_cntv_ctl(val)	asm volatile(\
				" mcr     p15, 0, %0, c14, c3, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_cntv_cval()	({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 3, %0, %1, c14\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define write_cntv_cval(val)	asm volatile(\
				" mcrr     p15, 3, %0, %1, c14\n\t" \
				:: "r" ((val) & 0xFFFFFFFF), "r" ((val) >> 32) \
				: "memory", "cc")

#define read_cntv_tval()	({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c14, c3, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_cntv_tval(val)	asm volatile(\
				" mcr     p15, 0, %0, c14, c3, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#define read_cntvct()		({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 1, %0, %1, c14\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define read_cntvoff()		({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 4, %0, %1, c14\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#define write_cntvoff(val)	asm volatile(\
				" mcrr     p15, 4, %0, %1, c14\n\t" \
				:: "r" ((val) & 0xFFFFFFFF), "r" ((val) >> 32) \
				: "memory", "cc")

#define read_cntvct()		({ u32 v1, v2; asm volatile(\
				" mrrc     p15, 1, %0, %1, c14\n\t" \
				: "=r" (v1), "=r" (v2) : : "memory", "cc"); \
				(((u64)v2 << 32) + (u64)v1);})

#endif

/* CPU feature checking macros */

#define cpu_supports_thumbee()	(((read_pfr0() & ID_PFR0_STATE3_MASK) \
					>> ID_PFR0_STATE3_SHIFT) == 0x1)

#define cpu_supports_securex()	(read_pfr1() & ID_PFR1_SECUREX_MASK)

#define cpu_supports_fpu()	(!(read_fpsid() & FPSID_SW_MASK))

#endif
