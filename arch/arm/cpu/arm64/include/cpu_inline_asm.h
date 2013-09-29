/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief  Frequently required inline assembly macros
 */
#ifndef __CPU_INLINE_ASM_H__
#define __CPU_INLINE_ASM_H__

#include <vmm_types.h>

#define rev16(val)		({ u16 rval; asm volatile(\
				" rev16   %0, %1\n\t" : "=r" (rval) : \
				"r" (val) : "memory", "cc"); rval;})

#define rev32(val)		({ u32 rval; asm volatile(\
				" rev32    %0, %1\n\t" : "=r" (rval) : \
				"r" (val) : "memory", "cc"); rval;})

#define rev64(val)		({ u32 d1, d2; \
				d1 = (u32)((u64)val >> 32); d2 = (u32)val; \
				d1 = rev32(d1); d2 = rev32(d2); \
				(((u64)d2 << 32) | ((u64)d1));})

#define ldxr(addr, data)	asm volatile("ldxr	%0, [%1]\n\t" \
				: "=r"(data) : "r"(addr))

#define stxr(addr, data, res)	asm volatile("stxr	%0, %1, [%2]\n\t" \
				: "=r"(res) : "r"(data), "r"(addr))

#define clrex()			asm volatile("clrex\n\t")

#define mrs(spr)		({ u64 rval; asm volatile(\
				"mrs %0," #spr :"=r"(rval)); rval; })

#define msr(spr, val)		asm volatile("msr " #spr ", %0" ::"r"(val))

#define msr_sync(spr, val)	asm volatile("msr " #spr ", %0\n\t" \
					     "dsb sy\n\t" \
					     "isb\n\t" ::"r"(val))

/* TLB maintainence */

#define inv_tlb_hyp_all()	asm volatile("tlbi alle2is\n\t" \
					     "dsb sy\n\t" \
					     "isb\n\t" \
					     ::: "memory", "cc")

#define inv_tlb_guest_allis()	asm volatile("tlbi alle1is\n\t" \
					     "dsb sy\n\t" \
					     "isb\n\t" \
					     ::: "memory", "cc")

#define inv_tlb_guest_cur()	asm volatile("tlbi vmalls12e1is\n\t" \
					     "dsb sy\n\t" \
					     "isb\n\t" \
					     ::: "memory", "cc")

#define inv_tlb_hyp_vais(va)	asm volatile("tlbi vae2is, %0\n\t" \
					     "dsb sy\n\t" \
					     "isb\n\t" \
					     ::"r"((va)>>12): "memory", "cc")

#define inv_tlb_guest_ipa(va)	asm volatile("tlbi ipas2e1is, %0\n\t" \
					     "dsb sy\n\t" \
					     "isb\n\t" \
					     ::"r"((va)>>12): "memory", "cc")

#define inv_tlb_guest_va(va)	asm volatile("tlbi vaae1is, %0\n\t" \
					     "dsb sy\n\t" \
					     "isb\n\t" \
					     ::"r"((va)>>12): "memory", "cc")

/* VA to PA Address Translation */

#define VA2PA_STAGE1		"s1"
#define VA2PA_STAGE12		"s12"
#define VA2PA_EL0		"e0"
#define VA2PA_EL1		"e1"
#define VA2PA_EL2		"e2"
#define VA2PA_EL3		"e3"
#define VA2PA_RD		"r"
#define VA2PA_WR		"w"
#define va2pa_at(stage, el, rw, va)	asm volatile(	\
					"at " stage el rw ", %0" \
					: : "r"(va) : "memory", "cc");


/* Features */
#define cpu_supports_thumbee()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_pfr0_el1" \
						: "=r"(pfr0)); \
				   (pfr0 & ID_PFR0_THUMBEE_MASK); \
				})

#define cpu_supports_thumb()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_pfr0_el1" \
						: "=r"(pfr0)); \
				   (pfr0 & ID_PFR0_THUMBEE_MASK); \
				})

#define cpu_supports_thumb2()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_pfr0_el1" \
						: "=r"(pfr0)); \
				   ((pfr0 & ID_PFR0_THUMB_MASK) == ID_PFR0_THUMB2_MASK); \
				})

#define cpu_supports_jazelle()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_pfr0_el1" \
						: "=r"(pfr0)); \
				   (pfr0 & ID_PFR0_JAZELLE_MASK); \
				})

#define cpu_supports_arm()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_pfr0_el1" \
						: "=r"(pfr0)); \
				   (pfr0 & ID_PFR0_ARM_MASK); \
				})

#define cpu_supports_asimd()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_aa64pfr0_el1" \
						: "=r"(pfr0)); \
				   ((pfr0 & ID_AA64PFR0_ASIMD_MASK) == 0); \
				})

#define cpu_supports_fpu()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_aa64pfr0_el1" \
						: "=r"(pfr0)); \
				   ((pfr0 & ID_AA64PFR0_FPU_MASK) == 0); \
				})

#define cpu_supports_el0_a32()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_aa64pfr0_el1" \
						: "=r"(pfr0)); \
				   ((pfr0 & ID_AA64PFR0_EL0_MASK) == ID_AA64PFR0_EL0_A32); \
				})

#define cpu_supports_el1_a32()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_aa64pfr0_el1" \
						: "=r"(pfr0)); \
				   ((pfr0 & ID_AA64PFR0_EL1_MASK) == ID_AA64PFR0_EL1_A32); \
				})

#define cpu_supports_el2_a32()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_aa64pfr0_el1" \
						: "=r"(pfr0)); \
				   ((pfr0 & ID_AA64PFR0_EL2_MASK) == ID_AA64PFR0_EL2_A32); \
				})

#define cpu_supports_el3_a32()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_aa64pfr0_el1" \
						: "=r"(pfr0)); \
				   ((pfr0 & ID_AA64PFR0_EL3_MASK) == ID_AA64PFR0_EL3_A32); \
				})

#define cpu_supports_el0()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_aa64pfr0_el1" \
						: "=r"(pfr0)); \
				   (pfr0 & ID_AA64PFR0_EL0_MASK); \
				})

#define cpu_supports_el1()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_aa64pfr0_el1" \
						: "=r"(pfr0)); \
				   (pfr0 & ID_AA64PFR0_EL1_MASK); \
				})

#define cpu_supports_el2()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_aa64pfr0_el1" \
						: "=r"(pfr0)); \
				   (pfr0 & ID_AA64PFR0_EL2_MASK); \
				})

#define cpu_supports_el3()	({ u64 pfr0; \
				   asm volatile("mrs %0, id_aa64pfr0_el1" \
						: "=r"(pfr0)); \
				   (pfr0 & ID_AA64PFR0_EL3_MASK); \
				})

#define vfp_simd_save_regs(addr)	{				\
		asm volatile("stnp	 Q0,  Q1, [%0, #0x00]\n\t"	\
			     "stnp	 Q2,  Q3, [%0, #0x20]\n\t"	\
			     "stnp	 Q4,  Q5, [%0, #0x40]\n\t"	\
			     "stnp	 Q6,  Q7, [%0, #0x60]\n\t"	\
			     :: "r"((char *)(addr) + 0x000));		\
		asm volatile("stnp	 Q8,  Q9, [%0, #0x00]\n\t"	\
			     "stnp	Q10, Q11, [%0, #0x20]\n\t"	\
			     "stnp	Q12, Q13, [%0, #0x40]\n\t"	\
			     "stnp	Q14, Q15, [%0, #0x60]\n\t"	\
			     :: "r"((char *)(addr) + 0x080));		\
		asm volatile("stnp	Q16, Q17, [%0, #0x00]\n\t"	\
			     "stnp	Q18, Q19, [%0, #0x20]\n\t"	\
			     "stnp	Q20, Q21, [%0, #0x40]\n\t"	\
			     "stnp	Q22, Q23, [%0, #0x60]\n\t"	\
			     :: "r"((char *)(addr) + 0x100));		\
		asm volatile("stnp	Q24, Q25, [%0, #0x00]\n\t"	\
			     "stnp	Q26, Q27, [%0, #0x20]\n\t"	\
			     "stnp	Q28, Q29, [%0, #0x40]\n\t"	\
			     "stnp	Q30, Q31, [%0, #0x60]\n\t"	\
			     :: "r"((char *)(addr) + 0x180));		\
		}


#define vfp_simd_restore_regs(addr)	{				\
		asm volatile("ldnp	 Q0,  Q1, [%0, #0x00]\n\t"	\
			     "ldnp	 Q2,  Q3, [%0, #0x20]\n\t"	\
			     "ldnp	 Q4,  Q5, [%0, #0x40]\n\t"	\
			     "ldnp	 Q6,  Q7, [%0, #0x60]\n\t"	\
			     :: "r"((char *)(addr) + 0x000));		\
		asm volatile("ldnp	 Q8,  Q9, [%0, #0x00]\n\t"	\
			     "ldnp	Q10, Q11, [%0, #0x20]\n\t"	\
			     "ldnp	Q12, Q13, [%0, #0x40]\n\t"	\
			     "ldnp	Q14, Q15, [%0, #0x60]\n\t"	\
			     :: "r"((char *)(addr) + 0x080));		\
		asm volatile("ldnp	Q16, Q17, [%0, #0x00]\n\t"	\
			     "ldnp	Q18, Q19, [%0, #0x20]\n\t"	\
			     "ldnp	Q20, Q21, [%0, #0x40]\n\t"	\
			     "ldnp	Q22, Q23, [%0, #0x60]\n\t"	\
			     :: "r"((char *)(addr) + 0x100));		\
		asm volatile("ldnp	Q24, Q25, [%0, #0x00]\n\t"	\
			     "ldnp	Q26, Q27, [%0, #0x20]\n\t"	\
			     "ldnp	Q28, Q29, [%0, #0x40]\n\t"	\
			     "ldnp	Q30, Q31, [%0, #0x60]\n\t"	\
			     :: "r"((char *)(addr) + 0x180));		\
		}

#endif
