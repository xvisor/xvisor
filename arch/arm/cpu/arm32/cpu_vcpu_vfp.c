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
 * @brief Source file for VCPU cp10 and cp11 emulation
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <arm_features.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_vfp.h>

bool cpu_vcpu_cp10_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u32 *data)
{
	switch (opc1) {
	case 7:
		switch (CRn) {
		case 0: /* FPSID */
			if (arm_feature(vcpu, ARM_FEATURE_VFP)) {
				*data = read_fpsid();
			} else {
				goto bad_reg;
			}
			break;
		case 1: /* FPSCR */
			if (arm_feature(vcpu, ARM_FEATURE_VFP) &&
			    (arm_priv(vcpu)->vfp.fpexc & FPEXC_EN_MASK)) {
				*data = read_fpscr();
				arm_priv(vcpu)->vfp.fpscr = *data;
			} else {
				goto bad_reg;
			}
			break;
		case 6: /* MVFR1 */
			if (arm_feature(vcpu, ARM_FEATURE_MVFR)) {
				*data = read_mvfr1();
			} else {
				goto bad_reg;
			}
			break;
		case 7: /* MVFR0 */
			if (arm_feature(vcpu, ARM_FEATURE_MVFR)) {
				*data = read_mvfr0();
			} else {
				goto bad_reg;
			}
			break;
		case 8: /* FPEXC */
			if (arm_feature(vcpu, ARM_FEATURE_VFP)) {
				*data = read_fpexc();
				arm_priv(vcpu)->vfp.fpexc = *data;
			} else {
				goto bad_reg;
			}
			break;
		case 9: /* FPINST */
			if (arm_feature(vcpu, ARM_FEATURE_VFP) &&
			    (arm_priv(vcpu)->vfp.fpexc & FPEXC_EN_MASK)) {
				*data = read_fpinst();
				arm_priv(vcpu)->vfp.fpinst = *data;
			} else {
				goto bad_reg;
			}
			break;
		case 10: /* FPINST2 */
			if (arm_feature(vcpu, ARM_FEATURE_VFP) &&
			    (arm_priv(vcpu)->vfp.fpexc & FPEXC_EN_MASK)) {
				*data = read_fpinst2();
				arm_priv(vcpu)->vfp.fpinst2 = *data;
			} else {
				goto bad_reg;
			}
			break;
		default:
			goto bad_reg;
		};
		break;
	default:
		goto bad_reg;
	};
	return TRUE;
bad_reg:
	vmm_printf("%s: vcpu=%d opc1=%x opc2=%x CRn=%x CRm=%x (invalid)\n", 
				__func__, vcpu->id, opc1, opc2, CRn, CRm);
	return FALSE;
}

bool cpu_vcpu_cp10_write(struct vmm_vcpu * vcpu, 
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			 u32 data)
{
	switch (opc1) {
	case 7:
		switch (CRn) {
		case 0: /* FPSID */
			if (arm_feature(vcpu, ARM_FEATURE_VFP)) {
				/* Ignore writes to FPSID */
			} else {
				goto bad_reg;
			}
			break;
		case 1: /* FPSCR */
			if (arm_feature(vcpu, ARM_FEATURE_VFP) &&
			    (arm_priv(vcpu)->vfp.fpexc & FPEXC_EN_MASK)) {
				write_fpscr(data);
				arm_priv(vcpu)->vfp.fpscr = data;
			} else {
				goto bad_reg;
			}
			break;
		case 6: /* MVFR1 */
			if (arm_feature(vcpu, ARM_FEATURE_MVFR)) {
				/* Ignore writes to MVFR1 */
			} else {
				goto bad_reg;
			}
			break;
		case 7: /* MVFR0 */
			if (arm_feature(vcpu, ARM_FEATURE_MVFR)) {
				/* Ignore writes to MVFR0 */
			} else {
				goto bad_reg;
			}
			break;
		case 8: /* FPEXC */
			if (arm_feature(vcpu, ARM_FEATURE_VFP)) {
				write_fpexc(data);
				arm_priv(vcpu)->vfp.fpexc = data;
			} else {
				goto bad_reg;
			}
			break;
		case 9: /* FPINST */
			if (arm_feature(vcpu, ARM_FEATURE_VFP) &&
			    (arm_priv(vcpu)->vfp.fpexc & FPEXC_EN_MASK)) {
				write_fpinst(data);
				arm_priv(vcpu)->vfp.fpinst = data;
			} else {
				goto bad_reg;
			}
			break;
		case 10: /* FPINST2 */
			if (arm_feature(vcpu, ARM_FEATURE_VFP) &&
			    (arm_priv(vcpu)->vfp.fpexc & FPEXC_EN_MASK)) {
				write_fpinst2(data);
				arm_priv(vcpu)->vfp.fpinst2 = data;
			} else {
				goto bad_reg;
			}
			break;
		default:
			goto bad_reg;
		};
		break;
	default:
		goto bad_reg;
	};
	return TRUE;
bad_reg:
	vmm_printf("%s: vcpu=%d opc1=%x opc2=%x CRn=%x CRm=%x (invalid)\n", 
				__func__, vcpu->id, opc1, opc2, CRn, CRm);
	return FALSE;
}

static void cpu_vcpu_vfp_regs_save(struct vmm_vcpu *vcpu)
{
	arm_priv_t *p = arm_priv(vcpu);

	/* Save FPEXC */
	p->vfp.fpexc = read_fpexc();

	/* Force enable FPU */
	write_fpexc(p->vfp.fpexc | FPEXC_EN_MASK);

	/* Save FPSCR */
	p->vfp.fpscr = read_fpscr();

	/* Check for sub-architecture */
	if (p->vfp.fpexc & FPEXC_EX_MASK) {
		/* Save FPINST */
		p->vfp.fpinst = read_fpinst();

		/* Save FPINST2 */
		if (p->vfp.fpexc & FPEXC_FP2V_MASK) {
			p->vfp.fpinst2 = read_fpinst2();
		}

		/* Disable FPEXC_EX */
		write_fpexc((p->vfp.fpexc | FPEXC_EN_MASK) & ~FPEXC_EX_MASK);
	}

	/* Save {d0-d15} */
	asm volatile("stc p11, cr0, [%0], #32*4"
		     : : "r" (p->vfp.fpregs1));

	/* 32x 64 bits registers? */
	if (arm_feature(vcpu, ARM_FEATURE_VFP3)) {
		if ((read_mvfr0() & MVFR0_A_SIMD_MASK) == 2) {
			/* Save {d16-d31} */
			asm volatile("stcl p11, cr0, [%0], #32*4"
				     : : "r" (p->vfp.fpregs2));
		}
	}

	/* Leave FPU in disabled state */
	write_fpexc(p->vfp.fpexc & ~(FPEXC_EN_MASK));
}

static void cpu_vcpu_vfp_regs_restore(struct vmm_vcpu *vcpu)
{
	arm_priv_t *p = arm_priv(vcpu);

	/* Force enable FPU */
	write_fpexc(read_fpexc() | FPEXC_EN_MASK);

	/* Restore {d0-d15} */
	asm volatile("ldc p11, cr0, [%0], #32*4"
		     : : "r" (p->vfp.fpregs1));

	/* 32x 64 bits registers? */
	if (arm_feature(vcpu, ARM_FEATURE_VFP3)) {
		if ((read_mvfr0() & MVFR0_A_SIMD_MASK) == 2) {
			/* Restore {d16-d31} */
			asm volatile("ldcl p11, cr0, [%0], #32*4"
				     : : "r" (p->vfp.fpregs2));
		}
	}

	/* Check for sub-architecture */
	if (p->vfp.fpexc & FPEXC_EX_MASK) {
		/* Restore FPINST */
		write_fpinst(p->vfp.fpinst);

		/* Restore FPINST2 */
		if (p->vfp.fpexc & FPEXC_FP2V_MASK) {
			write_fpinst2(p->vfp.fpinst2);
		}
	}

	/* Restore FPSCR */
	write_fpscr(p->vfp.fpscr);

	/* Restore FPEXC */
	write_fpexc(p->vfp.fpexc);
}

void cpu_vcpu_vfp_switch_context(struct vmm_vcpu *tvcpu, 
				  struct vmm_vcpu *vcpu)
{
	if (tvcpu && tvcpu->is_normal) {
		if (arm_feature(tvcpu, ARM_FEATURE_VFP)) {
			cpu_vcpu_vfp_regs_save(tvcpu);
		}
	}
	if (vcpu->is_normal) {
		if (arm_feature(vcpu, ARM_FEATURE_VFP)) {
			cpu_vcpu_vfp_regs_restore(vcpu);
		}
	}
}

int cpu_vcpu_vfp_init(struct vmm_vcpu *vcpu)
{
	u32 vfp_arch = read_fpsid();
	arm_priv_t *p = arm_priv(vcpu);

	/* If host HW does not have VFP (i.e. software VFP) then 
	 * clear all VFP feature flags so that VCPU always gets
	 * undefined exception when accessing VFP registers.
	 */
	if (!cpu_supports_fpu()) {
		goto no_vfp_for_vcpu;
	}

	/* VCPU with VFP3 would requie host HW to have VFP3 or higher */
	vfp_arch = (read_fpsid() & FPSID_ARCH_MASK) >> FPSID_ARCH_SHIFT;
	if (arm_feature(vcpu, ARM_FEATURE_VFP3) &&
	    (vfp_arch < 2)) {
		goto no_vfp_for_vcpu;
	}

	/* Reset VFP control registers */
	p->vfp.fpexc = 0x0;
	p->vfp.fpscr = 0x0;
	p->vfp.fpinst = 0x0;
	p->vfp.fpinst2 = 0x0;
	memset(&p->vfp.fpregs1, 0, sizeof(p->vfp.fpregs1));
	memset(&p->vfp.fpregs2, 0, sizeof(p->vfp.fpregs2));

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

