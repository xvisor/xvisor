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
 * @brief Source file for VCPU VFP emulation
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
	    (mrs(cptr_el2) & CPTR_TFP_MASK)) {
		return;
	}

	/* Save floating point registers */
	asm volatile("stp	 q0,  q1, [%0, #0x00]\n\t"
		     "stp	 q2,  q3, [%0, #0x20]\n\t"
		     "stp	 q4,  q5, [%0, #0x40]\n\t"
		     "stp	 q6,  q7, [%0, #0x60]\n\t"
		     :: "r"((char *)(&vfp->fpregs) + 0x000));
	asm volatile("stp	 q8,  q9, [%0, #0x00]\n\t"
		     "stp	q10, q11, [%0, #0x20]\n\t"
		     "stp	q12, q13, [%0, #0x40]\n\t"
		     "stp	q14, q15, [%0, #0x60]\n\t"
		     :: "r"((char *)(&vfp->fpregs) + 0x080));
	asm volatile("stp	q16, q17, [%0, #0x00]\n\t"
		     "stp	q18, q19, [%0, #0x20]\n\t"
		     "stp	q20, q21, [%0, #0x40]\n\t"
		     "stp	q22, q23, [%0, #0x60]\n\t"
		     :: "r"((char *)(&vfp->fpregs) + 0x100));
	asm volatile("stp	q24, q25, [%0, #0x00]\n\t"
		     "stp	q26, q27, [%0, #0x20]\n\t"
		     "stp	q28, q29, [%0, #0x40]\n\t"
		     "stp	q30, q31, [%0, #0x60]\n\t"
		     :: "r"((char *)(&vfp->fpregs) + 0x180));
	vfp->fpsr = mrs(fpsr);
	vfp->fpcr = mrs(fpcr);

	/* Save 32bit floating point control */
	vfp->fpexc32 = mrs(fpexc32_el2);
}

void cpu_vcpu_vfp_regs_restore(struct vmm_vcpu *vcpu)
{
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

	/* Do nothing if:
	 * 1. VCPU does not have VFPv3 feature
	 * 2. Floating point access is disabled
	 */
	if (!arm_feature(vcpu, ARM_FEATURE_VFP3) ||
	    (mrs(cptr_el2) & CPTR_TFP_MASK)) {
		return;
	}

	/* Restore floating point registers */
	asm volatile("ldp	 q0,  q1, [%0, #0x00]\n\t"
		     "ldp	 q2,  q3, [%0, #0x20]\n\t"
		     "ldp	 q4,  q5, [%0, #0x40]\n\t"
		     "ldp	 q6,  q7, [%0, #0x60]\n\t"
		     :: "r"((char *)(&vfp->fpregs) + 0x000));
	asm volatile("ldp	 q8,  q9, [%0, #0x00]\n\t"
		     "ldp	q10, q11, [%0, #0x20]\n\t"
		     "ldp	q12, q13, [%0, #0x40]\n\t"
		     "ldp	q14, q15, [%0, #0x60]\n\t"
		     :: "r"((char *)(&vfp->fpregs) + 0x080));
	asm volatile("ldp	q16, q17, [%0, #0x00]\n\t"
		     "ldp	q18, q19, [%0, #0x20]\n\t"
		     "ldp	q20, q21, [%0, #0x40]\n\t"
		     "ldp	q22, q23, [%0, #0x60]\n\t"
		     :: "r"((char *)(&vfp->fpregs) + 0x100));
	asm volatile("ldp	q24, q25, [%0, #0x00]\n\t"
		     "ldp	q26, q27, [%0, #0x20]\n\t"
		     "ldp	q28, q29, [%0, #0x40]\n\t"
		     "ldp	q30, q31, [%0, #0x60]\n\t"
		     :: "r"((char *)(&vfp->fpregs) + 0x180));
	msr(fpsr, vfp->fpsr);
	msr(fpcr, vfp->fpcr);

	/* Restore 32bit floating point control */
	msr(fpexc32_el2, vfp->fpexc32);
}

int cpu_vcpu_vfp_init(struct vmm_vcpu *vcpu)
{
	struct arm_priv *p = arm_priv(vcpu);
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

	/* If Host HW does not support VFPv3 or higher then
	 * don't allow VFP access to VCPU using CPTR_EL2
	 */
	if (cpu_supports_fpu() &&
	    arm_feature(vcpu, ARM_FEATURE_VFP3)) {
		p->cptr &= ~CPTR_TFP_MASK;
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
