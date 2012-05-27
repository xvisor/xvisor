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
 * @file cpu_private.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Any CPU specific headers.
 */

#ifndef __CPU_PRIVATE_H__
#define __CPU_PRIVATE_H__

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
	CPUID_GETVENDORSTRING,
	CPUID_GETFEATURES,
	CPUID_GETTLB,
	CPUID_GETSERIAL,

	CPUID_INTELEXTENDED=0x80000000,
	CPUID_INTELFEATURES,
	CPUID_INTELBRANDSTRING,
	CPUID_INTELBRANDSTRINGMORE,
	CPUID_INTELBRANDSTRINGEND,
};

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

/* Issue a complete request, storing general registers output as a string. */
static inline int cpuid_string(int code, u32 where[4])
{
	cpuid(code, &where[3], &where[0], &where[1], &where[2]);

	return where[3];
}

/* MSR ACCESS */
enum msr_registers {
	MSR_APIC = 0x1B
};

#define APIC_BASE(__msr)	(__msr >> 12)
#define APIC_ENABLED(__msr)	(__msr & (0x01UL << 11))

static inline u8 cpu_has_msr(void)
{
	u32 a, b, c, d;

	cpuid(CPUID_GETFEATURES,
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

#endif /* __CPU_PRIVATE_H__ */
