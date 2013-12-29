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
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

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
			    (vfp->fpexc & FPEXC_EN_MASK)) {
				*data = read_fpscr();
				vfp->fpscr = *data;
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
				vfp->fpexc = *data;
			} else {
				goto bad_reg;
			}
			break;
		case 9: /* FPINST */
			if (arm_feature(vcpu, ARM_FEATURE_VFP) &&
			    (vfp->fpexc & FPEXC_EN_MASK)) {
				*data = read_fpinst();
				vfp->fpinst = *data;
			} else {
				goto bad_reg;
			}
			break;
		case 10: /* FPINST2 */
			if (arm_feature(vcpu, ARM_FEATURE_VFP) &&
			    (vfp->fpexc & FPEXC_EN_MASK)) {
				*data = read_fpinst2();
				vfp->fpinst2 = *data;
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
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

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
			    (vfp->fpexc & FPEXC_EN_MASK)) {
				write_fpscr(data);
				vfp->fpscr = data;
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
				vfp->fpexc = data;
			} else {
				goto bad_reg;
			}
			break;
		case 9: /* FPINST */
			if (arm_feature(vcpu, ARM_FEATURE_VFP) &&
			    (vfp->fpexc & FPEXC_EN_MASK)) {
				write_fpinst(data);
				vfp->fpinst = data;
			} else {
				goto bad_reg;
			}
			break;
		case 10: /* FPINST2 */
			if (arm_feature(vcpu, ARM_FEATURE_VFP) &&
			    (vfp->fpexc & FPEXC_EN_MASK)) {
				write_fpinst2(data);
				vfp->fpinst2 = data;
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

void cpu_vcpu_vfp_regs_save(struct vmm_vcpu *vcpu)
{
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

	/* VFP feature must be available */
	if (!arm_feature(vcpu, ARM_FEATURE_VFP)) {
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
	if (arm_feature(vcpu, ARM_FEATURE_VFP3)) {
		if ((read_mvfr0() & MVFR0_A_SIMD_MASK) == 2) {
			/* Save {d16-d31} */
			asm volatile("stcl p11, cr0, [%0], #32*4"
				     : : "r" (vfp->fpregs2));
		}
	}

	/* Leave FPU in disabled state */
	write_fpexc(vfp->fpexc & ~(FPEXC_EN_MASK));
}

void cpu_vcpu_vfp_regs_restore(struct vmm_vcpu *vcpu)
{
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

	/* VFP feature must be available */
	if (!arm_feature(vcpu, ARM_FEATURE_VFP)) {
		return;
	}

	/* Force enable FPU */
	write_fpexc(read_fpexc() | FPEXC_EN_MASK);

	/* Restore {d0-d15} */
	asm volatile("ldc p11, cr0, [%0], #32*4"
		     : : "r" (vfp->fpregs1));

	/* 32x 64 bits registers? */
	if (arm_feature(vcpu, ARM_FEATURE_VFP3)) {
		if ((read_mvfr0() & MVFR0_A_SIMD_MASK) == 2) {
			/* Restore {d16-d31} */
			asm volatile("ldcl p11, cr0, [%0], #32*4"
				     : : "r" (vfp->fpregs2));
		}
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

void cpu_vcpu_vfp_regs_dump(struct vmm_chardev *cdev,
			    struct vmm_vcpu *vcpu)
{
	u32 i;
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

	/* VFP feature must be available */
	if (!arm_feature(vcpu, ARM_FEATURE_VFP)) {
		return;
	}

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
		} else {
			vmm_cprintf(cdev, "   ");
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
	u32 vfp_arch = read_fpsid();
	struct arm_priv_vfp *vfp = &arm_priv(vcpu)->vfp;

	/* If host HW does not have VFP (i.e. software VFP) then 
	 * clear all VFP feature flags so that VCPU always gets
	 * undefined exception when accessing VFP registers.
	 */
	if (!cpu_supports_fpu()) {
		goto no_vfp_for_vcpu;
	}

	/* VCPU with VFP3 would requie host HW to have VFP3 or higher */
	vfp_arch = (read_fpsid() & FPSID_ARCH_MASK) >> FPSID_ARCH_SHIFT;
	if (arm_feature(vcpu, ARM_FEATURE_VFP3) && (vfp_arch < 2)) {
		goto no_vfp_for_vcpu;
	}

	/* Reset VFP control registers */
	vfp->fpexc = 0x0;
	vfp->fpscr = 0x0;
	vfp->fpinst = 0x0;
	vfp->fpinst2 = 0x0;
	memset(&vfp->fpregs1, 0, sizeof(vfp->fpregs1));
	memset(&vfp->fpregs2, 0, sizeof(vfp->fpregs2));

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

