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
#include <vmm_compiler.h>
#include <vmm_cpumask.h>
#include <vmm_spinlocks.h>
#include <cpu_defines.h>

struct arch_regs {
	/* CPSR */
	u32 cpsr;
	/* Program Counter */
	u32 pc;
	/* R0 - R12 */
	u32 gpr[CPU_GPR_COUNT];
	/* Stack Pointer */
	u32 sp;
	/* Link Register */
	u32 lr;
} __packed;

typedef struct arch_regs arch_regs_t;

/* Note: This structure is accessed from assembly code
 * hence any change in this structure should be reflected
 * in relevant defines in cpu_defines.h
 */
struct arm_priv_vfp {
	/* ID Registers */
	u32 fpsid;				/* 0x0 */
	u32 mvfr0;				/* 0x4 */
	u32 mvfr1;				/* 0x8 */
	/* Control Registers */
	u32 fpexc;				/* 0xC */
	u32 fpscr;				/* 0x10 */
	u32 fpinst;				/* 0x14 */
	u32 fpinst2;				/* 0x18 */
	/* General Purpose Registers */
	/* {d0-d15} 64bit floating point registers.*/
	u64 fpregs1[16];			/* 0x1C */
	/* {d16-d31} 64bit floating point registers.*/
	u64 fpregs2[16];			/* 0x9C */
} __packed;

struct arm_priv_cp14 {
	/* ThumbEE Registers */
	u32 teecr;
	u32 teehbr;
};

/* Note: This structure is accessed from assembly code
 * hence any change in this structure should be reflected
 * in relevant defines in cpu_defines.h
 */
struct arm_priv_cp15 {
	/* ID Registers */
	u32 c0_midr;				/* 0x0 */
	u32 c0_mpidr;				/* 0x4 */
	u32 c0_cachetype;			/* 0x8 */
	u32 c0_pfr0;				/* 0xC */
	u32 c0_pfr1;				/* 0x10 */
	u32 c0_dfr0;				/* 0x14 */
	u32 c0_afr0;				/* 0x18 */
	u32 c0_mmfr0;				/* 0x1C */
	u32 c0_mmfr1;				/* 0x20 */
	u32 c0_mmfr2;				/* 0x24 */
	u32 c0_mmfr3;				/* 0x28 */
	u32 c0_isar0;				/* 0x2C */
	u32 c0_isar1;				/* 0x30 */
	u32 c0_isar2;				/* 0x34 */
	u32 c0_isar3;				/* 0x38 */
	u32 c0_isar4;				/* 0x3C */
	u32 c0_isar5;				/* 0x40 */
	/* Cache id. */
	u32 c0_ccsid[16];			/* 0x44 */
	/* Cache level. */
	u32 c0_clid;				/* 0x84 */
	/* Cache size selection. */
	u32 c0_cssel;				/* 0x88 */
	/* System control register. */
	u32 c1_sctlr;				/* 0x8C */
	/* Coprocessor access register.  */
	u32 c1_cpacr;				/* 0x90 */
	/* MMU translation table base 0. */
	u64 c2_ttbr0;				/* 0x94 */
	/* MMU translation table base 1. */
	u64 c2_ttbr1;				/* 0x9C */
	/* MMU translation table base control. */
	u32 c2_ttbcr;				/* 0xA4 */
	/* MMU domain access control register */
	u32 c3_dacr;				/* 0xA8 */
	/* Fault status registers. */
	u32 c5_ifsr;				/* 0xAC */
	/* Fault status registers. */
	u32 c5_dfsr;				/* 0xB0 */
	/* Auxillary Fault status registers. */
	u32 c5_aifsr;				/* 0xB4 */
	/* Auxillary Fault status registers. */
	u32 c5_adfsr;				/* 0xB8 */
	/* Fault address registers. */
	u32 c6_ifar;				/* 0xBC */
	/* Fault address registers. */
	u32 c6_dfar;				/* 0xC0 */
	/* VA2PA Translation result. */
	u32 c7_par;				/* 0xC4 */
	/* VA2PA Translation result. */
	u64 c7_par64;				/* 0xC8 */
	/* Cache lockdown registers. */
	u32 c9_insn;				/* 0xD0 */
	u32 c9_data;				/* 0xD4 */
	/* Performance monitor control register */
	u32 c9_pmcr;				/* 0xD8 */
	/* Perf monitor counter enables */
	u32 c9_pmcnten;				/* 0xDC */
	/* Perf monitor overflow status */
	u32 c9_pmovsr;				/* 0xE0 */
	/* Perf monitor event type */
	u32 c9_pmxevtyper;			/* 0xE4 */
	/* Perf monitor user enable */
	u32 c9_pmuserenr;			/* 0xE8 */
	/* Perf monitor interrupt enables */
	u32 c9_pminten;				/* 0xEC */
	/* For Long-descriptor format this is MAIR0 */
	u32 c10_prrr;				/* 0xF0 */
	/* For Long-descriptor format this is MAIR1 */
	u32 c10_nmrr;				/* 0xF4 */
	/* Vector base address register */
	u32 c12_vbar;				/* 0xF8 */
	/* FCSE PID. */
	u32 c13_fcseidr;			/* 0xFC */
	/* Context ID. */
	u32 c13_contextidr;			/* 0x100 */
	/* User RW Thread register. */
	u32 c13_tls1;				/* 0x104 */
	/* User RO Thread register. */
	u32 c13_tls2;				/* 0x108 */
	/* Privileged Thread register. */
	u32 c13_tls3;				/* 0x10C */
	/* Maximum D-cache dirty line index. */
	u32 c15_i_max;				/* 0x110 */
	/* Minimum D-cache dirty line index. */
	u32 c15_i_min;				/* 0x114 */
} __packed;

/* Note: This structure is accessed from assembly code
 * hence any change in this structure should be reflected
 * in relevant defines in cpu_defines.h
 */
struct arm_priv_banked {
	u32 sp_usr;				/* 0x0 */
	/* Supervisor Mode Registers */
	u32 sp_svc;				/* 0x4 */
	u32 lr_svc;				/* 0x8 */
	u32 spsr_svc;				/* 0xC */
	/* Abort Mode Registers Registers */
	u32 sp_abt;				/* 0x10 */
	u32 lr_abt;				/* 0x14 */
	u32 spsr_abt;				/* 0x18 */
	/* Undefined Mode Registers */
	u32 sp_und;				/* 0x1C */
	u32 lr_und;				/* 0x20 */
	u32 spsr_und;				/* 0x24 */
	/* IRQ Mode Registers */
	u32 sp_irq;				/* 0x28 */
	u32 lr_irq;				/* 0x2C */
	u32 spsr_irq;				/* 0x30 */
	/* FIQ Mode Registers */
	u32 gpr_fiq[CPU_FIQ_GPR_COUNT];		/* 0x34 */
	u32 sp_fiq;				/* 0x48 */
	u32 lr_fiq;				/* 0x4C */
	u32 spsr_fiq;				/* 0x50 */
} __packed;

struct arm_priv {
	/* Internal CPU feature flags. */
	u32 cpuid;
	u64 features;
	/* Hypervisor Configuration */
	vmm_spinlock_t hcr_lock;
	u32 hcr;
	u32 hcptr;
	u32 hstr;
	/* Banked Registers */
	struct arm_priv_banked bnk;
	/* VFP & SMID registers (cp10 & cp11 coprocessors) */
	struct arm_priv_vfp vfp;
	/* Debug, Trace, and ThumbEE (cp14 coprocessor) */
	struct arm_priv_cp14 cp14;
	/* System control (cp15 coprocessor) */
	struct arm_priv_cp15 cp15;
	vmm_cpumask_t dflush_needed;
	/* Last host CPU on which this VCPU ran */
	u32 last_hcpu;
	/* Generic timer context */
	void *gentimer_priv;
	/* VGIC context */
	bool vgic_avail;
	void (*vgic_save)(void *vcpu_ptr);
	void (*vgic_restore)(void *vcpu_ptr);
	void *vgic_priv;
} __attribute((packed));

struct arm_guest_priv {
	/* Stage2 table */
	struct cpu_ttbl *ttbl;
	/* PSCI version
	 * Bits[31:16] = Major number
	 * Bits[15:0] = Minor number
	 */
	u32 psci_version;
};

#define arm_regs(vcpu)		(&((vcpu)->regs))
#define arm_priv(vcpu)		((struct arm_priv *)((vcpu)->arch_priv))
#define arm_guest_priv(guest)	((struct arm_guest_priv *)((guest)->arch_priv))

#define arm_cpuid(vcpu)		(arm_priv(vcpu)->cpuid)
#define arm_set_feature(vcpu, feat) \
			(arm_priv(vcpu)->features |= (0x1ULL << (feat)))
#define arm_clear_feature(vcpu, feat) \
			(arm_priv(vcpu)->features &= ~(0x1ULL << (feat)))
#define arm_feature(vcpu, feat)	\
			(arm_priv(vcpu)->features & (0x1ULL << (feat)))

/**
 *  Instruction emulation support macros
 */
#define arm_pc(regs)		((regs)->pc)
#define arm_cpsr(regs)		((regs)->cpsr)

/**
 *  Generic timers support macros
 */
#define arm_gentimer_context(vcpu)	(arm_priv(vcpu)->gentimer_priv)

/**
 *  VGIC support macros
 */
#define arm_vgic_setup(vcpu, __save_func, __restore_func, __priv) \
			do { \
				arm_priv(vcpu)->vgic_avail = TRUE; \
				arm_priv(vcpu)->vgic_save = __save_func; \
				arm_priv(vcpu)->vgic_restore = __restore_func;\
				arm_priv(vcpu)->vgic_priv = __priv; \
			} while (0)
#define arm_vgic_cleanup(vcpu)	do { \
					arm_priv(vcpu)->vgic_avail = FALSE; \
					arm_priv(vcpu)->vgic_save = NULL; \
					arm_priv(vcpu)->vgic_restore = NULL; \
					arm_priv(vcpu)->vgic_priv = NULL; \
				} while (0)
#define arm_vgic_avail(vcpu)	(arm_priv(vcpu)->vgic_avail)
#define arm_vgic_save(vcpu)	if (arm_vgic_avail(vcpu)) { \
					arm_priv(vcpu)->vgic_save(vcpu); \
				}
#define arm_vgic_restore(vcpu)	if (arm_vgic_avail(vcpu)) { \
					arm_priv(vcpu)->vgic_restore(vcpu); \
				}
#define arm_vgic_priv(vcpu)	(arm_priv(vcpu)->vgic_priv)

#endif
