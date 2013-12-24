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
#include <generic_timer.h>

struct arch_regs {
	u64 gpr[CPU_GPR_COUNT];	/* X0 - X29 */
	u64 lr;
	u64 sp;		/* Stack Pointer */
	u64 pc;		/* Program Counter */
	u64 pstate; 	/* PState/SPSR */
} __attribute((packed));

typedef struct arch_regs arch_regs_t;

struct arm_priv {
	/* Internal CPU feature flags. */
	u32 cpuid;
	u64 features;
	/* Hypervisor context */
	vmm_spinlock_t hcr_lock;
	u64 hcr;	/* Hypervisor Configuration */
	u64 cptr;	/* Coprocessor Trap Register */
	u64 hstr;	/* Hypervisor System Trap Register */
	/* EL1 Registers */
	u64 sp_el0;
	u64 sp_el1;
	u64 elr_el1;
	u64 spsr_el1;
	u32 spsr_abt;
	u32 spsr_und;
	u32 spsr_irq;
	u32 spsr_fiq;
	u64 midr;	
	u64 mpidr;
	u64 sctlr; 	/* System control register. */
	u64 actlr;	/* Auxillary control register. */
	u64 cpacr; 	/* Coprocessor access register.  */
	u64 ttbr0;	/* MMU translation table base 0. */
	u64 ttbr1;	/* MMU translation table base 1. */
	u64 tcr; 	/* MMU translation control register. */
	u64 esr;	/* Exception status register. */
	u64 far; 	/* Fault address register. */
	u64 par;	/* Translation result. */
	u64 mair;	/* Memory attribute Index Register */
	u64 vbar; 	/* Vector base address register */
	u64 contextidr; /* Context ID. */
	u64 tpidr_el0; /* User RW Thread register. */
	u64 tpidr_el1; /* Privileged Thread register. */
	u64 tpidrro; 	/* User RO Thread register. */
	/* ThumbEE registers */
	u64 teecr;
	u32 teehbr;
	/* VFP & SMID registers */
	u32 fpexc32;
	u32 fpcr;
	u32 fpsr;
	u64 fpregs[64]; /* 32x 128bit floating point registers. */
	/* 32bit only registers */
	u32 cp15_c0_c1[8];  
	u32 cp15_c0_c2[8];  
	u32 dacr;	/* MMU domain access control register */
	u32 ifsr; 	/* Fault status registers. */
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

typedef struct arm_priv arm_priv_t;

struct arm_guest_priv {
	/* Stage2 table */
	struct cpu_ttbl *ttbl;
};

typedef struct arm_guest_priv arm_guest_priv_t;

#define arm_regs(vcpu)		(&((vcpu)->regs))
#define arm_priv(vcpu)		((arm_priv_t *)((vcpu)->arch_priv))
#define arm_guest_priv(guest)	((arm_guest_priv_t *)((guest)->arch_priv))

#define arm_cpuid(vcpu)		(arm_priv(vcpu)->cpuid)
#define arm_set_feature(vcpu, feat) \
			(arm_priv(vcpu)->features |= (0x1ULL << (feat)))
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
