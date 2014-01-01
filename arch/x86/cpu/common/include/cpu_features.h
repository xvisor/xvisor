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
 * @file cpu_features.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief CPU specific features gathering information.
 */

#ifndef __CPU_FEATURES_H__
#define __CPU_FEATURES_H__

#include <libs/stringlib.h>
#include <vmm_stdio.h>
#include <vmm_types.h>
#include <arch_cache.h>
#include <cpu_msr.h>

/* Vendor-strings. */
#define CPUID_VENDOR_AMD	"AuthenticAMD"
#define CPUID_VENDOR_INTEL	"GenuineIntel"

enum {
	CPUID_FEAT_ECX_SSE3         = 1 << 0,
	CPUID_FEAT_ECX_PCLMUL       = 1 << 1,
	CPUID_FEAT_ECX_DTES64       = 1 << 2,
	CPUID_FEAT_ECX_MONITOR      = 1 << 3,
	CPUID_FEAT_ECX_DS_CPL       = 1 << 4,
	CPUID_FEAT_ECX_VMX          = 1 << 5,
	CPUID_FEAT_ECX_SMX          = 1 << 6,
	CPUID_FEAT_ECX_EST          = 1 << 7,
	CPUID_FEAT_ECX_TM2          = 1 << 8,
	CPUID_FEAT_ECX_SSSE3        = 1 << 9,
	CPUID_FEAT_ECX_CID          = 1 << 10,
	CPUID_FEAT_ECX_FMA          = 1 << 12,
	CPUID_FEAT_ECX_CX16         = 1 << 13,
	CPUID_FEAT_ECX_ETPRD        = 1 << 14,
	CPUID_FEAT_ECX_PDCM         = 1 << 15,
	CPUID_FEAT_ECX_DCA          = 1 << 18,
	CPUID_FEAT_ECX_SSE4_1       = 1 << 19,
	CPUID_FEAT_ECX_SSE4_2       = 1 << 20,
	CPUID_FEAT_ECX_x2APIC       = 1 << 21,
	CPUID_FEAT_ECX_MOVBE        = 1 << 22,
	CPUID_FEAT_ECX_POPCNT       = 1 << 23,
	CPUID_FEAT_ECX_AES          = 1 << 25,
	CPUID_FEAT_ECX_XSAVE        = 1 << 26,
	CPUID_FEAT_ECX_OSXSAVE      = 1 << 27,
	CPUID_FEAT_ECX_AVX          = 1 << 28,

	CPUID_FEAT_EDX_FPU          = 1 << 0,
	CPUID_FEAT_EDX_VME          = 1 << 1,
	CPUID_FEAT_EDX_DE           = 1 << 2,
	CPUID_FEAT_EDX_PSE          = 1 << 3,
	CPUID_FEAT_EDX_TSC          = 1 << 4,
	CPUID_FEAT_EDX_MSR          = 1 << 5,
	CPUID_FEAT_EDX_PAE          = 1 << 6,
	CPUID_FEAT_EDX_MCE          = 1 << 7,
	CPUID_FEAT_EDX_CX8          = 1 << 8,
	CPUID_FEAT_EDX_APIC         = 1 << 9,
	CPUID_FEAT_EDX_SEP          = 1 << 11,
	CPUID_FEAT_EDX_MTRR         = 1 << 12,
	CPUID_FEAT_EDX_PGE          = 1 << 13,
	CPUID_FEAT_EDX_MCA          = 1 << 14,
	CPUID_FEAT_EDX_CMOV         = 1 << 15,
	CPUID_FEAT_EDX_PAT          = 1 << 16,
	CPUID_FEAT_EDX_PSE36        = 1 << 17,
	CPUID_FEAT_EDX_PSN          = 1 << 18,
	CPUID_FEAT_EDX_CLF          = 1 << 19,
	CPUID_FEAT_EDX_DTES         = 1 << 21,
	CPUID_FEAT_EDX_ACPI         = 1 << 22,
	CPUID_FEAT_EDX_MMX          = 1 << 23,
	CPUID_FEAT_EDX_FXSR         = 1 << 24,
	CPUID_FEAT_EDX_SSE          = 1 << 25,
	CPUID_FEAT_EDX_SSE2         = 1 << 26,
	CPUID_FEAT_EDX_SS           = 1 << 27,
	CPUID_FEAT_EDX_HTT          = 1 << 28,
	CPUID_FEAT_EDX_TM1          = 1 << 29,
	CPUID_FEAT_EDX_IA64         = 1 << 30,
	CPUID_FEAT_EDX_PBE          = 1 << 31
};

enum cpuid_requests {
	CPUID_BASE_VENDORSTRING,
	CPUID_BASE_FEATURES,
	CPUID_BASE_TLB,
	CPUID_BASE_SERIAL,
	CPUID_BASE_CACHE_CONF,
	CPUID_BASE_MON,
	CPUID_BASE_PWR_MNG,
	CPUID_BASE_FEAT_FLAGS,
	CPUID_BASE_FUNC_LIMIT,

	CPUID_EXTENDED_BASE=0x80000000,
	CPUID_EXTENDED_FEATURES, /* 800000001 */
	CPUID_EXTENDED_BRANDSTRING, /* 800000002 */
	CPUID_EXTENDED_BRANDSTRINGMORE, /* 80000003 */
	CPUID_EXTENDED_BRANDSTRINGEND, /* 800000004 */
	CPUID_EXTENDED_L1_CACHE_TLB_IDENTIFIER, /* 80000005 */
	CPUID_EXTENDED_L2_CACHE_TLB_IDENTIFIER, /* 80000006 */
	CPUID_EXTENDED_CAPABILITIES, /* 80000007 */
	CPUID_EXTENDED_ADDR_NR_PROC, /* 80000008 */
	CPUID_EXTENDED_RESVD_9, /* 80000009 */
	CPUID_EXTENDED_SVM_IDENTIFIER, /* 8000000A */
	CPUID_EXTENDED_FUNC_LIMIT
};

#define APIC_BASE(__msr)	(__msr >> 12)
#define APIC_ENABLED(__msr)	(__msr & (0x01UL << 11))

#define CPUID_BASE_FAMILY_SHIFT		8
#define CPUID_BASE_FAMILY_BITS		4
#define CPUID_BASE_FAMILY_MASK		((1<<CPUID_BASE_FAMILY_BITS)-1)
#define CPUID_EXTD_FAMILY_SHIFT		20
#define CPUID_EXTD_FAMILY_BITS		8
#define CPUID_EXTD_FAMILY_MASK		((1<<CPUID_EXTD_FAMILY_BITS)-1)

#define CPUID_BASE_MODEL_SHIFT		4
#define CPUID_BASE_MODEL_BITS		4
#define CPUID_BASE_MODEL_MASK		((1<<CPUID_BASE_MODEL_BITS)-1)
#define CPUID_EXTD_MODEL_SHIFT		16
#define CPUID_EXTD_MODEL_BITS		4
#define CPUID_EXTD_MODEL_MASK		((1<<CPUID_EXTD_MODEL_BITS)-1)

#define CPUID_STEPPING_SHIFT		0
#define CPUID_STEPPING_BITS		4
#define CPUID_STEPPING_MASK		((1<<CPUID_STEPPING_BITS)-1)

#define CPUID_L1_CACHE_SIZE_SHIFT	24
#define CPUID_L1_CACHE_SIZE_BITS	8
#define CPUID_L1_CACHE_SIZE_MASK	((1<<CPUID_L1_CACHE_SIZE_BITS)-1)
#define CPUID_L1_CACHE_LINE_SHIFT	0
#define CPUID_L1_CACHE_LINE_BITS	8
#define CPUID_L1_CACHE_LINE_MASK	((1<<CPUID_L1_CACHE_LINE_BITS)-1)

#define CPUID_L2_CACHE_SIZE_SHIFT	16
#define CPUID_L2_CACHE_SIZE_BITS	16
#define CPUID_L2_CACHE_SIZE_MASK	((1<<CPUID_L2_CACHE_SIZE_BITS)-1)
#define CPUID_L2_CACHE_LINE_SHIFT	0
#define CPUID_L2_CACHE_LINE_BITS	8
#define CPUID_L2_CACHE_LINE_MASK	((1<<CPUID_L2_CACHE_LINE_BITS)-1)

#define CPUID_L3_CACHE_SIZE_SHIFT	18
#define CPUID_L3_CACHE_SIZE_BITS	14
#define CPUID_L3_CACHE_SIZE_MASK	((1<<CPUID_L3_CACHE_SIZE_BITS)-1)
#define CPUID_L3_CACHE_LINE_SHIFT	0
#define CPUID_L3_CACHE_LINE_BITS	8
#define CPUID_L3_CACHE_LINE_MASK	((1<<CPUID_L3_CACHE_LINE_BITS)-1)

#define LVL_1_INST	1
#define LVL_1_DATA	2
#define LVL_2		3
#define LVL_3		4
#define LVL_TRACE	5

struct _cache_table {
	unsigned char descriptor;
	char cache_type;
	short size;
};

/* Intel-defined CPU features, CPUID level 0x00000001 (ecx), word 4 */
#define X86_FEATURE_XMM3        (4*32+ 0) /* "pni" SSE-3 */
#define X86_FEATURE_PCLMULQDQ   (4*32+ 1) /* PCLMULQDQ instruction */
#define X86_FEATURE_DTES64      (4*32+ 2) /* 64-bit Debug Store */
#define X86_FEATURE_MWAIT       (4*32+ 3) /* "monitor" Monitor/Mwait support */
#define X86_FEATURE_DSCPL       (4*32+ 4) /* "ds_cpl" CPL Qual. Debug Store */
#define X86_FEATURE_VMX         (4*32+ 5) /* Hardware virtualization */
#define X86_FEATURE_SMX         (4*32+ 6) /* Safer mode */
#define X86_FEATURE_EST         (4*32+ 7) /* Enhanced SpeedStep */
#define X86_FEATURE_TM2         (4*32+ 8) /* Thermal Monitor 2 */
#define X86_FEATURE_SSSE3       (4*32+ 9) /* Supplemental SSE-3 */
#define X86_FEATURE_CID         (4*32+10) /* Context ID */
#define X86_FEATURE_FMA         (4*32+12) /* Fused multiply-add */
#define X86_FEATURE_CX16        (4*32+13) /* CMPXCHG16B */
#define X86_FEATURE_XTPR        (4*32+14) /* Send Task Priority Messages */
#define X86_FEATURE_PDCM        (4*32+15) /* Performance Capabilities */
#define X86_FEATURE_DCA         (4*32+18) /* Direct Cache Access */
#define X86_FEATURE_XMM4_1      (4*32+19) /* "sse4_1" SSE-4.1 */
#define X86_FEATURE_XMM4_2      (4*32+20) /* "sse4_2" SSE-4.2 */
#define X86_FEATURE_X2APIC      (4*32+21) /* x2APIC */
#define X86_FEATURE_MOVBE       (4*32+22) /* MOVBE instruction */
#define X86_FEATURE_POPCNT      (4*32+23) /* POPCNT instruction */
#define X86_FEATURE_AES         (4*32+25) /* AES instructions */
#define X86_FEATURE_XSAVE       (4*32+26) /* XSAVE/XRSTOR/XSETBV/XGETBV */
#define X86_FEATURE_OSXSAVE     (4*32+27) /* "" XSAVE enabled in the OS */
#define X86_FEATURE_AVX         (4*32+28) /* Advanced Vector Extensions */
#define X86_FEATURE_HYPERVISOR  (4*32+31) /* Running on a hypervisor */

#define PROCESSOR_NAME_STRING_LEN	48
#define PROCESSOR_VENDOR_ID_LEN		12

enum x86_processor_generation {
	x86_CPU_AMD_K6, /* equivalent to intel 6th generation */
	x86_CPU_INTEL_PENTIUM, /* 6th generation */

	x86_NR_GENERATIONS,
};

enum x86_vendors {
	x86_VENDOR_UNKNOWN,
	x86_VENDOR_AMD,
	x86_VENDOR_INTEL,
	x86_NR_VENDORS,
};

struct cpuinfo_x86 {
	u8 vendor;
	u8 family;
	u8 model;
	u8 stepping;
	u8 vendor_string[PROCESSOR_VENDOR_ID_LEN];
	u8 name_string[PROCESSOR_NAME_STRING_LEN];
	u8 virt_bits;
	u8 phys_bits;
	u8 cpuid_level;
	u8 l1_dcache_size;
	u8 l1_dcache_line_size;
	u8 l1_icache_size;
	u8 l1_icache_line_size;
	u16 l2_cache_size;
	u16 l2_cache_line_size;
	u16 l3_cache_size;
	u8 hw_virt_available;
	u8 hw_nested_paging;
	u8 decode_assist;
	u32 hw_nr_asids;
}__aligned(ARCH_CACHE_LINE_SIZE);

/*
 * Issue a single request to CPUID. Fits 'intel features', for instance
 * note that even if only "eax" and "edx" are of interest, other registers
 * will be modified by the operation, so we need to tell the compiler about it.
 */
static inline void cpuid(int code, u32 *a, u32 *b, u32 *c, u32* d)
{
	asm volatile("cpuid\n\t"
		:"=a"(*a), "=d"(*d), "=b"(*b), "=c"(*c)
		:"0"(code));
}

static inline u8 cpu_has_msr(void)
{
	u32 a, b, c, d;

	cpuid(CPUID_BASE_FEATURES,
	      &a, &b, &c, &d);

	return (d & CPUID_FEAT_EDX_MSR);
}

static inline u64 cpu_read_msr(u32 msr)
{
	u32 a, d;

	asm volatile ("rdmsr\n\t"
		      :"=a"(a),"=d"(d)
		      :"c"(msr)
		      :"rbx");

	return (u64)(((u64)d << 32)
		     | (a & 0xFFFFFFFFUL));
}

static inline void cpu_read_msr32(u32 msr, u32 *high, u32 *low)
{
	u32 a, d;

	asm volatile ("rdmsr\n\t"
		      :"=a"(a),"=d"(d)
		      :"c"(msr)
		      :"rbx");

	*high = d;
	*low = a;
}

static inline void cpu_write_msr(u32 msr, u64 value)
{
	u32 a, d;

	/* RDX contains the high order 32-bits */
	d = value >> 32;

	/* RAX contains low order */
	a = value & 0xFFFFFFFFUL;

	asm volatile ("wrmsr\n\t"
		      ::"a"(a),"d"(d),"c"(msr));
}

static inline void cp_write_msr32(u32 msr, u32 high, u32 low)
{
	asm volatile ("wrmsr\n\t"
		      ::"a"(high),"d"(low),"c"(msr));
}

extern struct cpuinfo_x86 cpu_info;
extern void indentify_cpu(void);

#endif /* __CPU_FEATURES_H */
