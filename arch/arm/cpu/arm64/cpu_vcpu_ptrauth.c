/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file cpu_vcpu_ptrauth.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Source file for VCPU Pointer Authentication
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <arch_regs.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_switch.h>
#include <cpu_vcpu_ptrauth.h>

#include <arm_features.h>

void cpu_vcpu_ptrauth_save(struct vmm_vcpu *vcpu)
{
	struct arm_priv_ptrauth *ptrauth = &arm_priv(vcpu)->ptrauth;

	if (!arm_feature(vcpu, ARM_FEATURE_PTRAUTH)) {
		return;
	}

	/* Low-level PTRAUTH register save */
	cpu_vcpu_ptrauth_regs_save(ptrauth);
}

void cpu_vcpu_ptrauth_restore(struct vmm_vcpu *vcpu)
{
	struct arm_priv_ptrauth *ptrauth = &arm_priv(vcpu)->ptrauth;

	if (!arm_feature(vcpu, ARM_FEATURE_PTRAUTH)) {
		return;
	}

	/* Low-level PTRAUTH register restore */
	cpu_vcpu_ptrauth_regs_restore(ptrauth);
}

void cpu_vcpu_ptrauth_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	struct arm_priv_ptrauth *ptrauth = &arm_priv(vcpu)->ptrauth;

	if (!arm_feature(vcpu, ARM_FEATURE_PTRAUTH)) {
		return;
	}

	vmm_cprintf(cdev, "Pointer Authentication EL1 Registers\n");
	vmm_cprintf(cdev, " %13s=0x%016"PRIx64" %13s=0x%016"PRIx64"\n",
		    "APIAKEYLO_EL1", ptrauth->apiakeylo_el1,
		    "APIAKEYHI_EL1", ptrauth->apiakeyhi_el1);
	vmm_cprintf(cdev, " %13s=0x%016"PRIx64" %13s=0x%016"PRIx64"\n",
		    "APIBKEYLO_EL1", ptrauth->apibkeylo_el1,
		    "APIBKEYHI_EL1", ptrauth->apibkeyhi_el1);
	vmm_cprintf(cdev, " %13s=0x%016"PRIx64" %13s=0x%016"PRIx64"\n",
		    "APDAKEYLO_EL1", ptrauth->apdakeylo_el1,
		    "APDAKEYHI_EL1", ptrauth->apdakeyhi_el1);
	vmm_cprintf(cdev, " %13s=0x%016"PRIx64" %13s=0x%016"PRIx64"\n",
		    "APDBKEYLO_EL1", ptrauth->apdbkeylo_el1,
		    "APDBKEYHI_EL1", ptrauth->apdbkeyhi_el1);
	vmm_cprintf(cdev, " %13s=0x%016"PRIx64" %13s=0x%016"PRIx64"\n",
		    "APGAKEYLO_EL1", ptrauth->apgakeylo_el1,
		    "APGAKEYHI_EL1", ptrauth->apgakeyhi_el1);
}

int cpu_vcpu_ptrauth_init(struct vmm_vcpu *vcpu)
{
	struct arm_priv_ptrauth *ptrauth = &arm_priv(vcpu)->ptrauth;

	/* Clear VCPU PTRAUTH context */
	memset(ptrauth, 0, sizeof(struct arm_priv_ptrauth));

	if (!arm_feature(vcpu, ARM_FEATURE_PTRAUTH)) {
		goto done;
	}

	if (!cpu_supports_address_auth_arch() &&
	    !cpu_supports_address_auth_imp()) {
		arm_clear_feature(vcpu, ARM_FEATURE_PTRAUTH);
		goto done;
	}

	arm_priv(vcpu)->hcr |= (HCR_APL_MASK | HCR_APK_MASK);

done:
	return VMM_OK;
}

int cpu_vcpu_ptrauth_deinit(struct vmm_vcpu *vcpu)
{
	/* For now nothing to do here. */
	return VMM_OK;
}
