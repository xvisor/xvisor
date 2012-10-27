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
 * @file arch_regs.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief common header file for CPU registers
 */
#ifndef _ARCH_REGS_H__
#define _ARCH_REGS_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <cpu_defines.h>
#include <cpu_mmu.h>
#include <generic_timer.h>

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

/* Feature related enumeration */
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
	ARM_FEATURE_GENTIMER,	/* ARM generic timer extension */
};

struct arch_regs {
	u32 cpsr; /* CPSR */
	u32 pc;	/* Program Counter */
	u32 gpr[CPU_GPR_COUNT];	/* R0 - R12 */
	u32 sp;	/* Stack Pointer */
	u32 lr;	/* Link Register */
} __attribute((packed));

typedef struct arch_regs arch_regs_t;

struct arm_priv {
	/* Banked Registers */
	u32 sp_usr;
	u32 sp_svc; /* Supervisor Mode */
	u32 lr_svc;
	u32 spsr_svc;
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
	/* Hypervisor mode stack */
	void * hyp_stack;
	/* Hypervisor Configuration */
	u32 hcr;
	/* Hypervisor Coprocessor Trap Register */
	u32 hcptr;
	/* Hypervisor System Trap Register */
	u32 hstr;
	/* Internal CPU feature flags. */
	u32 features;
	/* System control coprocessor (cp15) */
	struct {
		/* Coprocessor Registers */
		u32 c0_cpuid;
		u32 c0_cachetype;
		u32 c0_ccsid[16]; /* Cache size. */
		u32 c0_clid; /* Cache level. */
		u32 c0_cssel; /* Cache size selection. */
		u32 c0_c1[8]; /* Feature registers. */
		u32 c0_c2[8]; /* Instruction set registers. */
		u32 c1_sctlr; /* System control register. */
		u32 c1_cpacr; /* Coprocessor access register.  */
		u32 c2_ttbr0; /* MMU translation table base 0. */
		u32 c2_ttbr1; /* MMU translation table base 1. */
		u32 c2_ttbcr; /* MMU translation table base control. */
		u32 c3_dacr; /* MMU domain access control register */
		u32 c5_ifsr; /* Fault status registers. */
		u32 c5_dfsr; /* Fault status registers. */
		u32 c6_ifar; /* Fault address registers. */
		u32 c6_dfar; /* Fault address registers. */
		u32 c7_par;  /* VA2PA Translation result. */
		u64 c7_par64;  /* VA2PA Translation result. */
		u32 c9_insn; /* Cache lockdown registers. */
		u32 c9_data;
		u32 c9_pmcr; /* performance monitor control register */
		u32 c9_pmcnten; /* perf monitor counter enables */
		u32 c9_pmovsr; /* perf monitor overflow status */
		u32 c9_pmxevtyper; /* perf monitor event type */
		u32 c9_pmuserenr; /* perf monitor user enable */
		u32 c9_pminten; /* perf monitor interrupt enables */
		u32 c10_prrr;
		u32 c10_nmrr;
		u32 c12_vbar; /* Vector base address register */
		u32 c13_fcseidr; /* FCSE PID. */
		u32 c13_contextidr; /* Context ID. */
		u32 c13_tls1; /* User RW Thread register. */
		u32 c13_tls2; /* User RO Thread register. */
		u32 c13_tls3; /* Privileged Thread register. */
		u32 c15_i_max; /* Maximum D-cache dirty line index. */
		u32 c15_i_min; /* Minimum D-cache dirty line index. */
	} cp15;
	/* Generic timer context */
	struct generic_timer_context gentimer_context;
} __attribute((packed));

typedef struct arm_priv arm_priv_t;

struct arm_guest_priv {
	/* Stage2 table lock */
	vmm_spinlock_t ttbl_lock;
	/* Stage2 table */
	struct cpu_ttbl *ttbl;
};

typedef struct arm_guest_priv arm_guest_priv_t;

#define arm_regs(vcpu)		(&((vcpu)->regs))
#define arm_priv(vcpu)		((arm_priv_t *)((vcpu)->arch_priv))
#define arm_guest_priv(guest)	((arm_guest_priv_t *)((guest)->arch_priv))

#define arm_cpuid(vcpu) (arm_priv(vcpu)->cp15.c0_cpuid)
#define arm_set_feature(vcpu, feat) (arm_priv(vcpu)->features |= (0x1 << (feat)))
#define arm_feature(vcpu, feat) (arm_priv(vcpu)->features & (0x1 << (feat)))

/**
 *  Instruction emulation support macros
 */
#define arm_pc(regs)		((regs)->pc)
#define arm_cpsr(regs)		((regs)->cpsr)

/**
 *  Generic timers support macro
 */
#define arm_gentimer_context(vcpu)	(&(arm_priv(vcpu)->gentimer_context))

#endif
