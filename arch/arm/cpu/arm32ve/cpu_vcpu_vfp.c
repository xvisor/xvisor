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
#include <arch_regs.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_vfp.h>
#include <arm_features.h>

void cpu_vcpu_vfp_regs_save(struct vmm_vcpu *vcpu)
{
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

	/* Do nothing if:
	 * 1. VCPU does not have VFPv3 feature
	 * 2. Floating point access is disabled
	 */
	if (!arm_feature(vcpu, ARM_FEATURE_VFP3) ||
	    (read_hcptr() & (HCPTR_TCP11_MASK|HCPTR_TCP10_MASK))) {
		return;
	}

	/* Save FPEXC */
	vfp->fpexc = read_fpexc();

	/* Force enable FPU */
	write_fpexc(vfp->fpexc | FPEXC_EN_MASK);

	/* Save FPSCR */
	vfp->fpscr = read_fpscr();

	/* Check for sub-architecture */
	if (vfp->fpexc & FPEXC_EX_MASK) {
		/* Save FPINST */
		vfp->fpinst = read_fpinst();

		/* Save FPINST2 */
		if (vfp->fpexc & FPEXC_FP2V_MASK) {
			vfp->fpinst2 = read_fpinst2();
		}

		/* Disable FPEXC_EX */
		write_fpexc((vfp->fpexc | FPEXC_EN_MASK) & ~FPEXC_EX_MASK);
	}

	/* Save {d0-d15} */
	asm volatile("stc p11, cr0, [%0], #32*4"
		     : : "r" (vfp->fpregs1));

	/* 32x 64 bits registers? */
	if ((read_mvfr0() & MVFR0_A_SIMD_MASK) == 2) {
		/* Save {d16-d31} */
		asm volatile("stcl p11, cr0, [%0], #32*4"
			     : : "r" (vfp->fpregs2));
	}

	/* Leave FPU in disabled state */
	write_fpexc(vfp->fpexc & ~(FPEXC_EN_MASK));
}

void cpu_vcpu_vfp_regs_restore(struct vmm_vcpu *vcpu)
{
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

	/* Do nothing if:
	 * 1. VCPU does not have VFPv3 feature
	 * 2. Floating point access is disabled
	 */
	if (!arm_feature(vcpu, ARM_FEATURE_VFP3) ||
	    (read_hcptr() & (HCPTR_TCP11_MASK|HCPTR_TCP10_MASK))) {
		return;
	}

	/* Force enable FPU */
	write_fpexc(read_fpexc() | FPEXC_EN_MASK);

	/* Restore {d0-d15} */
	asm volatile("ldc p11, cr0, [%0], #32*4"
		     : : "r" (vfp->fpregs1));

	/* 32x 64 bits registers? */
	if ((read_mvfr0() & MVFR0_A_SIMD_MASK) == 2) {
	        /* Restore {d16-d31} */
        	asm volatile("ldcl p11, cr0, [%0], #32*4"
			     : : "r" (vfp->fpregs2));
	}

	/* Check for sub-architecture */
	if (vfp->fpexc & FPEXC_EX_MASK) {
		/* Restore FPINST */
		write_fpinst(vfp->fpinst);

		/* Restore FPINST2 */
		if (vfp->fpexc & FPEXC_FP2V_MASK) {
			write_fpinst2(vfp->fpinst2);
		}
	}

	/* Restore FPSCR */
	write_fpscr(vfp->fpscr);

	/* Restore FPEXC */
	write_fpexc(vfp->fpexc);
}

int cpu_vcpu_vfp_init(struct vmm_vcpu *vcpu)
{
	u32 fpu;
	struct arm_priv *p = arm_priv(vcpu);
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

	/* If Host HW does not support VFPv3 or higher then
	 * don't allow CP10 & CP11 access to VCPU using HCPTR
	 */
	fpu = (read_fpsid() & FPSID_ARCH_MASK) >>  FPSID_ARCH_SHIFT;
	if (cpu_supports_fpu() && (fpu > 1) &&
	    arm_feature(vcpu, ARM_FEATURE_VFP3)) {
		p->hcptr &= ~(HCPTR_TCP11_MASK|HCPTR_TCP10_MASK);
	}

	/* Clear VCPU VFP context */
	memset(vfp, 0, sizeof(struct arm_priv_vfp));

	return VMM_OK;
}

int cpu_vcpu_vfp_deinit(struct vmm_vcpu *vcpu)
{
	/* For now nothing to do here. */
	return VMM_OK;
}
