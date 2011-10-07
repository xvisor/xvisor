/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file vmm_regs.h
 * @version 1.0
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief common header file for CPU registers
 */
#ifndef _VMM_REGS_H__
#define _VMM_REGS_H__

#include <vmm_types.h>
#include <cpu_defines.h>
#include <cpu_mmu.h>

/* CPUID related macors & defines */
#define ARM_CPUID_ARM1026     0x4106a262
#define ARM_CPUID_ARM926      0x41069265
#define ARM_CPUID_ARM946      0x41059461
#define ARM_CPUID_TI915T      0x54029152
#define ARM_CPUID_TI925T      0x54029252
#define ARM_CPUID_SA1100      0x4401A11B
#define ARM_CPUID_SA1110      0x6901B119
#define ARM_CPUID_PXA250      0x69052100
#define ARM_CPUID_PXA255      0x69052d00
#define ARM_CPUID_PXA260      0x69052903
#define ARM_CPUID_PXA261      0x69052d05
#define ARM_CPUID_PXA262      0x69052d06
#define ARM_CPUID_PXA270      0x69054110
#define ARM_CPUID_PXA270_A0   0x69054110
#define ARM_CPUID_PXA270_A1   0x69054111
#define ARM_CPUID_PXA270_B0   0x69054112
#define ARM_CPUID_PXA270_B1   0x69054113
#define ARM_CPUID_PXA270_C0   0x69054114
#define ARM_CPUID_PXA270_C5   0x69054117
#define ARM_CPUID_ARM1136     0x4117b363
#define ARM_CPUID_ARM1136_R2  0x4107b362
#define ARM_CPUID_ARM11MPCORE 0x410fb022
#define ARM_CPUID_CORTEXA8    0x410fc080
#define ARM_CPUID_CORTEXA9    0x410fc090
#define ARM_CPUID_CORTEXM3    0x410fc231
#define ARM_CPUID_ANY         0xffffffff

enum arm_features {
	ARM_FEATURE_VFP,
	ARM_FEATURE_AUXCR,  /* ARM1026 Auxiliary control register.  */
	ARM_FEATURE_XSCALE, /* Intel XScale extensions.  */
	ARM_FEATURE_IWMMXT, /* Intel iwMMXt extension.  */
	ARM_FEATURE_V6,
	ARM_FEATURE_V6K,
	ARM_FEATURE_V7,
	ARM_FEATURE_THUMB2,
	ARM_FEATURE_MPU,    /* Only has Memory Protection Unit, not full MMU.  */
	ARM_FEATURE_VFP3,
	ARM_FEATURE_VFP_FP16,
	ARM_FEATURE_NEON,
	ARM_FEATURE_DIV,
	ARM_FEATURE_M, /* Microcontroller profile.  */
	ARM_FEATURE_OMAPCP, /* OMAP specific CP15 ops handling.  */
	ARM_FEATURE_THUMB2EE,
	ARM_FEATURE_V7MP,    /* v7 Multiprocessing Extensions */
	ARM_FEATURE_V4T,
	ARM_FEATURE_V5,
	ARM_FEATURE_STRONGARM,
	ARM_FEATURE_VAPA, /* cp15 VA to PA lookups */
};

struct vmm_user_regs {
	u32 cpsr; /* CPSR */
	u32 gpr[CPU_GPR_COUNT];	/* R0 - R12 */
	u32 sp;	/* Stack Pointer */
	u32 lr;	/* Link Register */
	u32 pc;	/* Program Counter */
} __attribute((packed));

typedef struct vmm_user_regs vmm_user_regs_t;

struct cpu_vtlb_entry {
	u8 valid;
	cpu_page_t page;
};

typedef struct cpu_vtlb_entry cpu_vtlb_entry_t;

struct cpu_vtlb {
	cpu_vtlb_entry_t * table;
	u32 victim;
	u32 count;
};

typedef struct cpu_vtlb cpu_vtlb_t;

struct vmm_super_regs {
	/* Priviledged CPSR */
	u32 cpsr;
	/* Banked Registers */
	u32 gpr_usr[CPU_FIQ_GPR_COUNT];	/* User Mode */
	u32 sp_usr;
	u32 lr_usr;
	u32 sp_svc; /* Supervisor Mode */
	u32 lr_svc;
	u32 spsr_svc;
	u32 sp_mon; /* Monitor Mode */
	u32 lr_mon;
	u32 spsr_mon;
	u32 sp_abt; /* Abort Mode */
	u32 lr_abt;
	u32 spsr_abt;
	u32 sp_und; /* Undefined Mode */
	u32 lr_und;
	u32 spsr_und;
	u32 sp_irq; /* IRQ Mode */
	u32 lr_irq;
	u32 spsr_irq;
	u32 gpr_fiq[CPU_FIQ_GPR_COUNT];	/* FIQ Mode */
	u32 sp_fiq;
	u32 lr_fiq;
	u32 spsr_fiq;
	/* Internal CPU feature flags. */
	u32 features;
	/* System control coprocessor (cp15) */
	struct {
		/* Shadow L1 */
		cpu_l1tbl_t *l1;
		/* Shadow DACR */
		u32 dacr;
		/* Virtual TLB */
		cpu_vtlb_t vtlb;
		/* Overlapping vectors */
		u32 ovect[CPU_IRQ_NR * 2];
		u32 ovect_base;
		/* Coprocessor Registers */
		u32 c0_cpuid;
		u32 c0_cachetype;
		u32 c0_ccsid[16]; /* Cache size. */
		u32 c0_clid; /* Cache level. */
		u32 c0_cssel; /* Cache size selection. */
		u32 c0_c1[8]; /* Feature registers. */
		u32 c0_c2[8]; /* Instruction set registers. */
		u32 c1_sctlr; /* System control register. */
		u32 c1_coproc; /* Coprocessor access register.  */
		u32 c2_base0; /* MMU translation table base 0. */
		u32 c2_base1; /* MMU translation table base 1. */
		u32 c2_control; /* MMU translation table base control. */
		u32 c2_mask; /* MMU translation table base selection mask. */
		u32 c2_base_mask; /* MMU translation table base 0 mask. */
		u32 c3; /* MMU domain access control register */
		u32 c5_ifsr; /* Fault status registers. */
		u32 c5_dfsr; /* Fault status registers. */
		u32 c6_ifar; /* Fault address registers. */
		u32 c6_dfar; /* Fault address registers. */
		u32 c7_par; /* Translation result. */
		u32 c9_insn; /* Cache lockdown registers. */
		u32 c9_data;
		u32 c9_pmcr; /* performance monitor control register */
		u32 c9_pmcnten; /* perf monitor counter enables */
		u32 c9_pmovsr; /* perf monitor overflow status */
		u32 c9_pmxevtyper; /* perf monitor event type */
		u32 c9_pmuserenr; /* perf monitor user enable */
		u32 c9_pminten; /* perf monitor interrupt enables */
		u32 c13_fcse; /* FCSE PID. */
		u32 c13_context; /* Context ID. */
		u32 c13_tls1; /* User RW Thread register. */
		u32 c13_tls2; /* User RO Thread register. */
		u32 c13_tls3; /* Privileged Thread register. */
		u32 c15_i_max; /* Maximum D-cache dirty line index. */
		u32 c15_i_min; /* Minimum D-cache dirty line index. */
	} cp15;
} __attribute((packed));

typedef struct vmm_super_regs vmm_super_regs_t;

#define arm_cpuid(vcpu) ((vcpu)->sregs->cp15.c0_cpuid)
#define arm_set_feature(vcpu, feat) ((vcpu)->sregs->features |= (0x1 << (feat)))
#define arm_feature(vcpu, feat) ((vcpu)->sregs->features & (0x1 << (feat)))

#endif
