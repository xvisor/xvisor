/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief common header file for CPU registers
 */
#ifndef _ARCH_REGS_H__
#define _ARCH_REGS_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <cpu_defines.h>

struct arch_regs {
	u64 gpr[CPU_GPR_COUNT];	/* X0 - X29 */
	u64 lr;
	u64 sp;		/* Stack Pointer */
	u64 pc;		/* Program Counter */
	u64 pstate; 	/* PState/SPSR */
} __attribute((packed));

typedef struct arch_regs arch_regs_t;

struct arm_priv_vfp {
	/* 64bit EL1/EL0 registers */
	u32 mvfr0;
	u32 mvfr1;
	u32 mvfr2;
	u32 fpcr;
	u32 fpsr;
	u64 fpregs[64]; /* 32x 128bit floating point registers. */
	/* 32bit only registers */
	u32 fpexc32;
};

struct arm_priv_sysregs {
	/* 64bit EL1/EL0 registers */
	u64 sp_el0;
	u64 sp_el1;
	u64 elr_el1;
	u64 spsr_el1;
	u64 midr_el1;	
	u64 mpidr_el1;
	u64 sctlr_el1; 	/* System control register. */
	u64 actlr_el1;	/* Auxillary control register. */
	u64 cpacr_el1; 	/* Coprocessor access register.  */
	u64 ttbr0_el1;	/* MMU translation table base 0. */
	u64 ttbr1_el1;	/* MMU translation table base 1. */
	u64 tcr_el1; 	/* MMU translation control register. */
	u64 esr_el1;	/* Exception status register. */
	u64 far_el1; 	/* Fault address register. */
	u64 par_el1;	/* Translation result. */
	u64 mair_el1;	/* Memory attribute Index Register */
	u64 vbar_el1; 	/* Vector base address register */
	u64 contextidr_el1; /* Context ID. */
	u64 tpidr_el0; /* User RW Thread register. */
	u64 tpidr_el1; /* Privileged Thread register. */
	u64 tpidrro_el0; 	/* User RO Thread register. */
	/* 32bit only registers */
	u32 spsr_abt;
	u32 spsr_und;
	u32 spsr_irq;
	u32 spsr_fiq;
	u32 dacr32_el2;	/* MMU domain access control register */
	u32 ifsr32_el2; 	/* Fault status registers. */
	/* 32bit only ThumbEE registers */
	u64 teecr32_el1;
	u32 teehbr32_el1;
};

struct arm_priv {
	/* Internal CPU feature flags. */
	u32 cpuid;
	u64 features;
	/* Hypervisor context */
	vmm_spinlock_t hcr_lock;
	u64 hcr;	/* Hypervisor Configuration */
	u64 cptr;	/* Coprocessor Trap Register */
	u64 hstr;	/* Hypervisor System Trap Register */
	/* EL1/EL0 system registers */
	struct arm_priv_sysregs sysregs;
	/* VFP & SMID registers */
	struct arm_priv_vfp vfp;
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
#define arm_cpsr(regs)		((u32)((regs)->pstate & 0xffffffff))
#define arm_pc(regs)		((regs)->pc)

/**
 *  Generic timers support macro
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
