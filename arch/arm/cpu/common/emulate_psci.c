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
 * @file psci.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of ARM PSCI emulation APIs
 */

#include <vmm_error.h>
#include <psci.h>
#include <cpu_emulate_psci.h>
#include <emulate_psci.h>

/* Emulate PSCI v0.1 interface */
#define PSCI_FN_BASE		0x95c1ba5e
#define PSCI_FN(n)		(PSCI_FN_BASE + (n))

#define PSCI_FN_CPU_SUSPEND	PSCI_FN(0)
#define PSCI_FN_CPU_OFF		PSCI_FN(1)
#define PSCI_FN_CPU_ON		PSCI_FN(2)
#define PSCI_FN_MIGRATE		PSCI_FN(3)

static unsigned long psci_vcpu_off(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	if (vmm_manager_vcpu_reset(vcpu)) {
		return PSCI_RET_INTERNAL_FAILURE;
	}

	return PSCI_RET_SUCCESS;
}

static unsigned long psci_vcpu_on(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	unsigned long cpuid, startpc;
	struct vmm_vcpu *target_vcpu;

	cpuid = emulate_psci_get_reg(vcpu, regs, 1);
	startpc = emulate_psci_get_reg(vcpu, regs, 2);

	if (vcpu->subid == (u32)cpuid) {
		return PSCI_RET_INVALID_PARAMS;
	}

	/* FIXME: The cpuid is actually value of mpidr so, 
	 * we need a better way of determining target VCPU.
	 */
	target_vcpu = vmm_manager_guest_vcpu(vcpu->guest, (u32)cpuid);
	if (!target_vcpu) {
		return PSCI_RET_NOT_PRESENT;
	}

	if (vmm_manager_vcpu_get_state(target_vcpu) != VMM_VCPU_STATE_RESET) {
		return PSCI_RET_ALREADY_ON;
	}

	emulate_psci_set_pc(target_vcpu, &target_vcpu->regs, startpc);

	if (vmm_manager_vcpu_kick(target_vcpu)) {
		return PSCI_RET_INTERNAL_FAILURE;
	}

	return PSCI_RET_SUCCESS;
}

int emulate_psci_call(struct vmm_vcpu *vcpu, arch_regs_t *regs, bool is_smc)
{
	unsigned long psci_fn = emulate_psci_get_reg(vcpu, regs, 0) & ~((u32) 0);
	unsigned long val;

	switch (psci_fn) {
	case PSCI_FN_CPU_OFF:
		val = psci_vcpu_off(vcpu, regs);
		break;
	case PSCI_FN_CPU_ON:
		val = psci_vcpu_on(vcpu, regs);
		break;
	case PSCI_FN_CPU_SUSPEND:
	case PSCI_FN_MIGRATE:
		val = PSCI_RET_NOT_SUPPORTED;
		break;
	default:
		return VMM_EINVALID;
	}

	emulate_psci_set_reg(vcpu, regs, 0, val);

	return VMM_OK;
}

