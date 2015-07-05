/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file cpu_vcpu_vfp.c
 * @author Anup Patel (anup@brainfault.org)
 * @author Sting Cheng (sting.cheng@gmail.com)
 * @brief Source file for VCPU cp10 and cp11 emulation
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <arch_regs.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_inject.h>
#include <cpu_vcpu_switch.h>
#include <cpu_vcpu_vfp.h>
#include <arm_features.h>

void cpu_vcpu_vfp_save(struct vmm_vcpu *vcpu)
{
	struct arm_priv *p = arm_priv(vcpu);
	struct arm_priv_vfp *vfp = &p->vfp;

	/* Do nothing if:
	 * 1. VCPU does not have VFPv3 feature
	 */
	if (!arm_feature(vcpu, ARM_FEATURE_VFP3)) {
		return;
	}

	/* If VFP/ASIMD traps were disabled then:
	 * 1. Save VFP/ASIMD regs
	 * 2. Enable VFP/ASIMD traps
	 */
	if (!(p->hcptr & (HCPTR_TCP11_MASK|HCPTR_TCP10_MASK))) {
		cpu_vcpu_vfp_regs_save(vfp);
	}

	/* Force disable FPU
	 * Note: We don't use VFP in hypervisor so
	 * better disable it.
	 */
	write_fpexc(read_fpexc() & ~FPEXC_EN_MASK);
}

void cpu_vcpu_vfp_restore(struct vmm_vcpu *vcpu)
{
	struct arm_priv *p = arm_priv(vcpu);

	/* Make sure we trap VFP/ASIMD */
	p->hcptr |= (HCPTR_TASE_MASK);
	p->hcptr |= (HCPTR_TCP11_MASK|HCPTR_TCP10_MASK);

	/* Force enable FPU
	 * Note: If FPU is not enabled then we don't get
	 * VFP traps so we force enable FPU and setup
	 * traps using HCPTR register.
	 */
	write_fpexc(read_fpexc() | FPEXC_EN_MASK);
}

int cpu_vcpu_vfp_trap(struct vmm_vcpu *vcpu,
		      arch_regs_t *regs,
		      u32 il, u32 iss,
		      bool is_asimd)
{
	struct arm_priv *p = arm_priv(vcpu);
	struct arm_priv_vfp *vfp = &p->vfp;

	/* Inject undefined exception if:
	 * 1. VCPU does not have VFPv3 feature
	 */
	if (!arm_feature(vcpu, ARM_FEATURE_VFP3)) {
		/* Inject undefined exception */
		cpu_vcpu_inject_undef(vcpu, regs);
		return VMM_OK;
	}

	/* If VFP/ASIMD traps were enabled then:
	 * 1. Disable VFP/ASIMD traps
	 * 2. Restore VFP/ASIMD regs
	 */
	p->hcptr &= ~(HCPTR_TASE_MASK);
	p->hcptr &= ~(HCPTR_TCP11_MASK|HCPTR_TCP10_MASK);
	write_hcptr(p->hcptr);
	cpu_vcpu_vfp_regs_restore(vfp);

	return VMM_OK;
}

void cpu_vcpu_vfp_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	u32 i;
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

	/* Do nothing if:
	 * 1. VCPU does not have VFPv3 feature
	 */
	if (!arm_feature(vcpu, ARM_FEATURE_VFP3)) {
		return;
	}

	vmm_cprintf(cdev, "VFP Identification Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "FPSID", vfp->fpsid,
		    "MVFR0", vfp->mvfr0,
		    "MVFR1", vfp->mvfr1);
	vmm_cprintf(cdev, "VFP System Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "FPEXC", vfp->fpexc,
		    "FPSCR", vfp->fpscr,
		    "FPINST", vfp->fpinst);
	vmm_cprintf(cdev, " %7s=0x%08x\n",
		    "FPINST2", vfp->fpinst2);
	vmm_cprintf(cdev, "VFP Data Registers");
	for (i = 0; i < 32; i++) {
		if (i % 2 == 0) {
			vmm_cprintf(cdev, "\n");
		}
		if (i < 16) {
			vmm_cprintf(cdev, " %5s%02d=0x%016llx",
				   "D", (i), vfp->fpregs1[i]);
		} else {
			vmm_cprintf(cdev, " %5s%02d=0x%016llx",
				   "D", (i), vfp->fpregs2[i-16]);
		}
	}
	vmm_cprintf(cdev, "\n");
}

int cpu_vcpu_vfp_init(struct vmm_vcpu *vcpu)
{
	u32 fpu;
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

	/* Clear VCPU VFP context */
	memset(vfp, 0, sizeof(struct arm_priv_vfp));

	/* If host HW does not have VFP (i.e. software VFP) then
	 * clear all VFP feature flags so that VCPU always gets
	 * undefined exception when accessing VFP registers.
	 */
	if (!cpu_supports_fpu()) {
		goto no_vfp_for_vcpu;
	}

	/* If Host HW does not support VFPv3 or higher then
	 * clear all VFP feature flags so that VCPU always gets
	 * undefined exception when accessing VFP registers.
	 */
	fpu = (read_fpsid() & FPSID_ARCH_MASK) >>  FPSID_ARCH_SHIFT;
	if ((fpu <= 1) || !arm_feature(vcpu, ARM_FEATURE_VFP3)) {
		goto no_vfp_for_vcpu;
	}

	/* Current strategy is to show VFP identification registers
	 * same as underlying Host HW so that Guest sees same VFP
	 * capabilities as Host HW.
	 */
	vfp->fpsid = read_fpsid();
	vfp->mvfr0 = read_mvfr0();
	vfp->mvfr1 = read_mvfr1();

	return VMM_OK;

no_vfp_for_vcpu:
	arm_clear_feature(vcpu, ARM_FEATURE_MVFR);
	arm_clear_feature(vcpu, ARM_FEATURE_VFP);
	arm_clear_feature(vcpu, ARM_FEATURE_VFP3);
	arm_clear_feature(vcpu, ARM_FEATURE_VFP4);
	return VMM_OK;
}

int cpu_vcpu_vfp_deinit(struct vmm_vcpu *vcpu)
{
	/* For now nothing to do here. */
	return VMM_OK;
}
