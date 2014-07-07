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
 * @file cpu_inline_asm.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief  Frequently required inline assembly macros
 */
#ifndef __CPU_INLINE_ASM_H__
#define __CPU_INLINE_ASM_H__

#include <vmm_types.h>
#include <cpu_defines.h>

#if defined(CONFIG_ARMV5)

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

#else

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

#endif

#if defined(CONFIG_ARMV5)

/* FIXME: */
#define ldrex(addr, data)	asm volatile("ldr	%0, [%1]\n\t" \
				: "=r"(data) : "r"(addr))

/* FIXME: */
#define strex(addr, data, res)	asm volatile("str	%0, [%1]\n\t" \
				: : "r"(data), "r"(addr))

/* FIXME: */
#define clrex()

#elif defined(CONFIG_ARMV6)

#define ldrex(addr, data)	asm volatile("ldrex	%0, [%1]\n\t" \
				: "=r"(data) : "r"(addr))

#define strex(addr, data, res)	asm volatile("strex	%0, %1, [%2]\n\t" \
				: "=&r"(res) : "r"(data), "r"(addr))

#define clrex()	

#else

#define ldrex(addr, data)	asm volatile("ldrex	%0, [%1]\n\t" \
				: "=r"(data) : "r"(addr))

#define strex(addr, data, res)	asm volatile("strex	%0, %1, [%2]\n\t" \
				: "=&r"(res) : "r"(data), "r"(addr))

#define clrex()			asm volatile("clrex\n\t")

#endif

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

#define read_midr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_ctr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c0, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define read_mpidr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c0, c0, 5\n\t" \
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

#define read_actlr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c1, c0, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_actlr(val)	asm volatile(\
				" mcr     p15, 0, %0, c1, c0, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#ifndef CONFIG_ARMV5

#define read_cpacr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c1, c0, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_cpacr(val)	asm volatile(\
				" mcr     p15, 0, %0, c1, c0, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")

#else

#define read_cpacr()		0

#define write_cpacr(val)

#endif

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

#if defined(CONFIG_ARMV7A)

#define read_vbar()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c12, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_vbar(val)		asm volatile(\
				" mcr     p15, 0, %0, c12, c0, 0\n\t" \
				:: "r" ((val)) : "memory", "cc")

#else

#define read_vbar()		0
#define write_vbar(val)

#endif /* CONFIG_ARMV7A */

#define read_ttbcr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c2, c0, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_ttbcr(val)	asm volatile(\
				" mcr     p15, 0, %0, c2, c0, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")

#if defined(CONFIG_ARMV5)
/*
 * On ARM V5, we don't know if a data abort was triggered by a read or
 * write operation. We need to read the instruction that triggered the 
 * abort and determine its type.
 */
extern unsigned int **_abort_inst;

static inline u32 read_dfsr(void)
{
	u32 rval, inst;

	asm volatile(" mrc     p15, 0, %0, c5, c0, 0" : "=r" (rval) : : "memory", "cc");

	inst = **_abort_inst;

	/*
	 * all STM/STR/LDM/LDR instructions have bit 20 to indicate 
	 * if it is a read or write operation. We test this bit
	 * to set or clear bit 11 on the DFSR result.
	 * SWP instruction is reading and writing to memory. So we 
	 * assume write. SWP has 0 on bit 20 (like STM or STR).
	 */
	if (inst & (1 << 20)) {
		/* LDM or LDR type instruction */
		rval &= ~(1 << 11);
	} else {
		/* STM or STR type instruction */
		/* SWP instruction is writing to memory */
		rval |= (1 << 11);
	}

	return rval;
}
#else // CONFIG_ARMV5
#define read_dfsr()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c5, c0, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})
#endif // CONFIG_ARMV5

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

#if defined(CONFIG_ARMV5)

extern unsigned int *_ifar;

/*
 * On ARM V5, there is no IFAR register. Therefore we need to emulate this
 * function
 */
#define read_ifar()		(*_ifar)
/* There is no need to write IFAR */
#else
#define read_ifar()		({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c6, c0, 2\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_ifar(val)		asm volatile(\
				" mcr     p15, 0, %0, c6, c0, 2\n\t" \
				:: "r" ((val)) : "memory", "cc")
#endif

#define invalid_i_tlb()		({ u32 rval=0; asm volatile(\
				" mcr     p15, 0, %0, c8, c5, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define invalid_i_tlb_mva(va)	asm volatile(\
				" mcr     p15, 0, %0, c8, c5, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define invalid_d_tlb()		({ u32 rval=0; asm volatile(\
				" mcr     p15, 0, %0, c8, c6, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define invalid_d_tlb_mva(va)	asm volatile(\
				" mcr     p15, 0, %0, c8, c6, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#define invalid_tlb()		({ u32 rval=0; asm volatile(\
				" mcr     p15, 0, %0, c8, c7, 0\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define invalid_tlb_mva(va)	asm volatile(\
				" mcr     p15, 0, %0, c8, c7, 1\n\t" \
				:: "r" ((va)) : "memory", "cc")

#if !defined(CONFIG_ARMV5)

#define invalid_tlb_asid(asid)	asm volatile(\
				" mcr     p15, 0, %0, c8, c7, 2\n\t" \
				:: "r" ((asid)) : "memory", "cc")

#endif

#define read_contextidr()	({ u32 rval; asm volatile(\
				" mrc     p15, 0, %0, c13, c0, 1\n\t" \
				: "=r" (rval) : : "memory", "cc"); rval;})

#define write_contextidr(val)	asm volatile(\
				" mcr     p15, 0, %0, c13, c0, 1\n\t" \
				:: "r" ((val)) : "memory", "cc")

#if defined(CONFIG_ARMV5)

#define read_tpidrurw()		0x0

#define write_tpidrurw(val)	

#define read_tpidruro()		0x0

#define write_tpidruro(val)	

#define read_tpidrprw()		0x0

#define write_tpidrprw(val)	

#else

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

/* CPU feature checking macros */

#ifdef CONFIG_ARMV7A

#define cpu_supports_thumbee()	(((read_pfr0() & ID_PFR0_STATE3_MASK) \
					>> ID_PFR0_STATE3_SHIFT) == 0x1)

#define cpu_supports_securex()	(read_pfr1() & ID_PFR1_SECUREX_MASK)

#else

#define cpu_supports_thumbee()	0

#define cpu_supports_securex()	0

#endif

#ifdef CONFIG_ARMV5
#define cpu_supports_fpu()	0
#else
#define cpu_supports_fpu()	(!(read_fpsid() & FPSID_SW_MASK))
#endif

#endif
