/**
 * Copyright (c) 2013 Sting Cheng.
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
 * @file cpu_vcpu_cp14.c
 * @author Sting Cheng (sting.cheng@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Source file for VCPU cp14 (Debug, Trace, and ThumbEE) emulation
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <arch_regs.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_cp14.h>

#include <arm_features.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

bool cpu_vcpu_cp14_read(struct vmm_vcpu *vcpu,
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm,
			u32 *data)
{
	struct arm_priv_cp14 *cp14 = &arm_priv(vcpu)->cp14;

	switch (opc1) {
	case 6: /* ThumbEE registers. */
		if (!arm_feature(vcpu, ARM_FEATURE_THUMB2EE))
			goto bad_reg;
		switch (CRn) {
		case 0: /* TEECR */
			if ((CRm == 0) && (opc2 == 0)) {
				cp14->teecr = read_teecr();
				 *data = cp14->teecr;
			} else {
				goto bad_reg;
			}
			DPRINTF("%s: TEECR: vcpu=%s data=0x%08x\n",
				__func__, vcpu->name, *data);
			break;
		case 1: /* TEEHBR */ 
			if ((CRm == 0) && (opc2 == 0)) {
				cp14->teehbr = read_teehbr();
				*data = cp14->teehbr;
			} else {
				goto bad_reg;
			}
			DPRINTF("%s: TEEHBR: vcpu=%s data=0x%08x\n",
				__func__, vcpu->name, *data);
			break;
		default:
			goto bad_reg;
		};
		break;
	case 0: /* Debug registers */
		if ((1 == CRn) && (4 == opc2)) {
			if (1 == CRm) {
				/* DBGOSLSR: No debug support */
				*data = 0;
				break;
			} else {
				/* DBGPRSR, Device Powerdown and Reset Status Register */
				*data = 0;
				break;
			}
		}
		vmm_printf("%s: Debug not supported yet!\n", __func__);
		goto bad_reg;
	case 1: /* Trace registers. */
		vmm_printf("%s: Trace not supported yet!\n", __func__);
		goto bad_reg;
	case 7: /* Jazelle registers. */
		vmm_printf("%s: Jazelle not supported yet!\n", __func__);
		goto bad_reg;
	default:
		goto bad_reg;
	}

	return TRUE;

bad_reg:
	vmm_printf("%s: vcpu=%s opc1=%x opc2=%x CRn=%x CRm=%x (invalid)\n",
		   __func__, vcpu->name, opc1, opc2, CRn, CRm);				
	return FALSE;
}

bool cpu_vcpu_cp14_write(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm,
			 u32 data)
{
	struct arm_priv_cp14 *cp14 = &arm_priv(vcpu)->cp14;

	switch (opc1) {
	case 6: /* ThumbEE registers. */
		if (!arm_feature(vcpu, ARM_FEATURE_THUMB2EE))
			goto bad_reg;			
		switch (CRn) {
		case 0: /* TEECR */
			DPRINTF("%s: TEECR: vcpu=%s data=0x%08x\n",
				__func__, vcpu->name, data);
			if ((CRm == 0) && (opc2 == 0)) {
				write_teecr(data);
				cp14->teecr = data;
			} else {
				goto bad_reg;
			}
			break;
		case 1: /* TEEHBR */ 
			DPRINTF("%s: TEEHBR: vcpu=%s data=0x%08x\n",
				__func__, vcpu->name, data);
			if ((CRm == 0) && (opc2 == 0)) {
				write_teehbr(data);
				cp14->teehbr = data;
			} else {
				goto bad_reg;
			}
			break;
		default:
			goto bad_reg;
		};
		break;
	case 0: /* Debug registers */
		if (0 == CRn) {
			if ((0 == opc2) && (7 == CRm)) {
				/* DBGVCR, Vector Catch Register */
				break;
			} else if (7 == opc2) {
				/* DBGBCR, Watchpoint Control Registers */
				/* CRm: C0-15 */
				break;
			} else if (6 == opc2) {
				/* DBGBVR, Watchpoint Value Registers */
				/* CRm: C0-15 */
				break;
			} else if (5 == opc2) {
				/* DBGBCR, Breakpoint Control Registers */
				/* CRm: C0-15 */
				break;
			} else if (4 == opc2) {
				/* DBGBVR, Breakpoint Value Registers */
				/* CRm: C0-15 */
				break;
			} else if ((2 == opc2) && (2 == CRm)) {
				/* DBGDSCR, Debug Status and Control Register */
				break;
			}
		}
		vmm_printf("%s: Debug not supported yet!\n", __func__);
		goto bad_reg;
	case 1: /* Trace registers. */
		vmm_printf("%s: Trace not supported yet!\n", __func__);
		goto bad_reg;
	case 7: /* Jazelle registers. */
		vmm_printf("%s: Jazelle not supported yet!\n", __func__);
		goto bad_reg;
	default:
		goto bad_reg;
	}

	return TRUE;

bad_reg:
	vmm_printf("%s: vcpu=%s opc1=%x opc2=%x CRn=%x CRm=%x (invalid)\n",
		   __func__, vcpu->name, opc1, opc2, CRn, CRm);
	return FALSE;
}

void cpu_vcpu_cp14_regs_save(struct vmm_vcpu *vcpu)
{
	/* All CP14 register access by VCPU always trap hence,
	 * we always have updated copy of CP14 registers.
	 */
}

void cpu_vcpu_cp14_regs_restore(struct vmm_vcpu *vcpu)
{
	struct arm_priv_cp14 *cp14 = &arm_priv(vcpu)->cp14;

	/* Do nothing if:
	 * 1. Host HW does not have ThumbEE feature
	 */
	if (!cpu_supports_thumbee()) {
		return;
	}

	/* Restore ThumbEE registers */
	write_teecr(cp14->teecr);
	write_teehbr(cp14->teehbr);
}

void cpu_vcpu_cp14_regs_dump(struct vmm_chardev *cdev,
			     struct vmm_vcpu *vcpu)
{
	struct arm_priv_cp14 *cp14 = &arm_priv(vcpu)->cp14;

	/* Do nothing if:
	 * 1. VCPU does not have ThumbEE feature
	 */
	if (!arm_feature(vcpu, ARM_FEATURE_THUMB2EE)) {
		return;
	}

	vmm_cprintf(cdev, "CP14 ThumbEE Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x\n",
		    "TEECR", cp14->teecr,
		    "TEEHBR", cp14->teehbr);
}

int cpu_vcpu_cp14_init(struct vmm_vcpu *vcpu)
{
	struct arm_priv_cp14 *cp14 = &arm_priv(vcpu)->cp14;

	/* Clear all CP14 registers */
	memset(cp14, 0, sizeof(*cp14));

	/* If host HW does not have ThumbEE then clear all
	 * ThumbEE feature flag so that VCPU always gets
	 * undefined exception when accessing ThumbEE registers.
	 */
	if (!cpu_supports_thumbee()) {
		arm_clear_feature(vcpu, ARM_FEATURE_THUMB2EE);
	}

	return VMM_OK;
}

int cpu_vcpu_cp14_deinit(struct vmm_vcpu *vcpu)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

