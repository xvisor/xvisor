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
#include <vmm_cpumask.h>
#include <vmm_spinlocks.h>
#include <cpu_defines.h>
#include <generic_timer.h>

struct arch_regs {
	u32 cpsr; /* CPSR */
	u32 pc;	/* Program Counter */
	u32 gpr[CPU_GPR_COUNT];	/* R0 - R12 */
	u32 sp;	/* Stack Pointer */
	u32 lr;	/* Link Register */
} __attribute((packed));

typedef struct arch_regs arch_regs_t;

struct arm_priv_vfp {
	/* ID Registers */
	u32 fpsid;
	u32 mvfr0;
	u32 mvfr1;
	/* Control Registers */
	u32 fpexc;
	u32 fpscr;
	u32 fpinst;
	u32 fpinst2;
	/* General Purpose Registers */
	u64 fpregs1[16]; /* {d0-d15} 64bit floating point registers.*/
	u64 fpregs2[16]; /* {d16-d31} 64bit floating point registers.*/
};

struct arm_priv_cp14 {
	/* ThumbEE Registers */
	u32 teecr;
	u32 teehbr;
};

struct arm_priv_cp15 {
	/* Coprocessor Registers */
	u32 c0_midr;
	u32 c0_mpidr;
	u32 c0_cachetype;
	u32 c0_pfr0;
	u32 c0_pfr1;
	u32 c0_dfr0;
	u32 c0_afr0;
	u32 c0_mmfr0;
	u32 c0_mmfr1;
	u32 c0_mmfr2;
	u32 c0_mmfr3;
	u32 c0_isar0;
	u32 c0_isar1;
	u32 c0_isar2;
	u32 c0_isar3;
	u32 c0_isar4;
	u32 c0_isar5;
	u32 c0_ccsid[16]; /* Cache size. */
	u32 c0_clid; /* Cache level. */
	u32 c0_cssel; /* Cache size selection. */
	u32 c1_sctlr; /* System control register. */
	u32 c1_cpacr; /* Coprocessor access register.  */
	u32 c2_ttbr0; /* MMU translation table base 0. */
	u32 c2_ttbr1; /* MMU translation table base 1. */
	u32 c2_ttbcr; /* MMU translation table base control. */
	u32 c3_dacr; /* MMU domain access control register */
	u32 c5_ifsr; /* Fault status registers. */
	u32 c5_dfsr; /* Fault status registers. */
	u32 c5_aifsr; /* Auxillary Fault status registers. */
	u32 c5_adfsr; /* Auxillary Fault status registers. */
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
	/* D-cache clean-invalidate by set/way mask */
	vmm_cpumask_t dflush_needed;
};

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
	/* VFP & SMID registers (cp10 & cp11 coprocessors) */
	struct arm_priv_vfp vfp;
	/* Debug, Trace, and ThumbEE (cp14 coprocessor) */
	struct arm_priv_cp14 cp14;
	/* System control (cp15 coprocessor) */
	struct arm_priv_cp15 cp15;
	/* Last host CPU on which this VCPU ran */
	u32 last_hcpu;
	/* Generic timer context */
	struct generic_timer_context gentimer_context;
	/* VGIC context */
	bool vgic_avail;
	void (*vgic_save)(void *vcpu_ptr);
	void (*vgic_restore)(void *vcpu_ptr);
	void *vgic_priv;
} __attribute((packed));

struct arm_guest_priv {
	/* Stage2 table */
	struct cpu_ttbl *ttbl;
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
#define arm_gentimer_context(vcpu)	(&(arm_priv(vcpu)->gentimer_context))

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
