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
 * @brief Implementation of ARM PSCI emulation for Guest
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_vcpu_irq.h>
#include <arch_barrier.h>

#include <cpu_defines.h>
#include <cpu_emulate_psci.h>
#include <emulate_psci.h>
#include <psci.h>

/*
 * This is an implementation of the Power State Coordination Interface
 * as described in ARM document number ARM DEN 0022A.
 */

#define AFFINITY_MASK(level)	~((0x1UL << ((level) * MPIDR_LEVEL_BITS)) - 1)

static unsigned long psci_affinity_mask(unsigned long affinity_level)
{
	if (affinity_level <= 3)
		return MPIDR_HWID_BITMASK & AFFINITY_MASK(affinity_level);

	return 0;
}

static unsigned long psci_vcpu_suspend(struct vmm_vcpu *vcpu,
				       arch_regs_t *regs)
{
	/*
	 * NOTE: For simplicity, we make VCPU suspend emulation to be
	 * same-as WFI (Wait-for-interrupt) emulation.
	 *
	 * This means for Xvisor the wakeup events are interrupts and
	 * this is consistent with intended use of StateID as described
	 * in section 5.4.1 of PSCI v0.2 specification (ARM DEN 0022A).
	 *
	 * Further, we also treat power-down request to be same as
	 * stand-by request as-per section 5.4.2 clause 3 of PSCI v0.2
	 * specification (ARM DEN 0022A). This means all suspend states
	 * for Xvisor will preserve the register state.
	 */
	vmm_vcpu_irq_wait_timeout(vcpu, 0);

	return PSCI_RET_SUCCESS;
}

static unsigned long psci_vcpu_off(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	if (vmm_manager_vcpu_reset(vcpu)) {
		return PSCI_RET_INTERNAL_FAILURE;
	}

	return PSCI_RET_SUCCESS;
}

static unsigned long psci_vcpu_on(struct vmm_vcpu *source_vcpu,
				  arch_regs_t *regs)
{
	u32 i;
	unsigned long cpu_id;
	unsigned long context_id;
	unsigned long mpidr;
	unsigned long target_pc;
	struct vmm_vcpu *vcpu = NULL, *tmp = NULL;

	cpu_id = emulate_psci_get_reg(source_vcpu, regs, 1);
	if (emulate_psci_is_32bit(source_vcpu, regs)) {
		cpu_id &= ~((u32) 0);
	}

	for_each_guest_vcpu(i, tmp, source_vcpu->guest) {
		mpidr = emulate_psci_get_mpidr(tmp);
		if ((mpidr & MPIDR_HWID_BITMASK) ==
				(cpu_id & MPIDR_HWID_BITMASK)) {
			vcpu = tmp;
			break;
		}
	}

	/* Make sure the caller requested a valid CPU and
	 * that the CPU is turned off.
	 */
	if (!vcpu || vcpu == source_vcpu) {
		return PSCI_RET_INVALID_PARAMS;
	}
	if (vmm_manager_vcpu_get_state(tmp) != VMM_VCPU_STATE_RESET) {
		if (emulate_psci_version(source_vcpu) != 1) {
			return PSCI_RET_ALREADY_ON;
		} else {
			return PSCI_RET_INVALID_PARAMS;
		}
	}

	target_pc = emulate_psci_get_reg(source_vcpu, regs, 2);
	context_id = emulate_psci_get_reg(source_vcpu, regs, 3);

	/* Gracefully handle Thumb2 entry point */
	if (emulate_psci_is_32bit(vcpu, regs) && (target_pc & 1)) {
		target_pc &= ~((physical_addr_t) 1);
		emulate_psci_set_thumb(vcpu, &vcpu->regs);
	}

	/* Propagate caller endianness */
	if (emulate_psci_is_be(source_vcpu, regs)) {
		emulate_psci_set_be(vcpu, &vcpu->regs);
	}

	emulate_psci_set_pc(vcpu, &vcpu->regs, target_pc);
	/*
	 * NOTE: We always update r0 (or x0) because for PSCI v0.1
	 * the general puspose registers are undefined upon CPU_ON.
	 */
	emulate_psci_set_reg(vcpu, &vcpu->regs, 0, context_id);

	arch_smp_mb();		/* Make sure the above is visible */

	if (vmm_manager_vcpu_kick(vcpu)) {
		return PSCI_RET_INTERNAL_FAILURE;
	}

	return PSCI_RET_SUCCESS;
}

static unsigned long psci_vcpu_affinity_info(struct vmm_vcpu *vcpu,
					     arch_regs_t *regs)
{
	u32 i;
	unsigned long mpidr;
	unsigned long target_affinity;
	unsigned long target_affinity_mask;
	unsigned long lowest_affinity_level;
	struct vmm_vcpu *tmp;

	target_affinity = emulate_psci_get_reg(vcpu, regs, 1);
	lowest_affinity_level = emulate_psci_get_reg(vcpu, regs, 2);

	/* Determine target affinity mask */
	target_affinity_mask = psci_affinity_mask(lowest_affinity_level);
	if (!target_affinity_mask)
		return PSCI_RET_INVALID_PARAMS;

	/* Ignore other bits of target affinity */
	target_affinity &= target_affinity_mask;

	/* If one or more VCPU matching target affinity are running
	 * then ON else OFF
	 */
	for_each_guest_vcpu(i, tmp, vcpu->guest) {
		mpidr = emulate_psci_get_mpidr(tmp);
		if (((mpidr & target_affinity_mask) == target_affinity) &&
		    (vmm_manager_vcpu_get_state(tmp) & VMM_VCPU_STATE_INTERRUPTIBLE)) {
			return PSCI_0_2_AFFINITY_LEVEL_ON;
		}
	}

	return PSCI_0_2_AFFINITY_LEVEL_OFF;
}

static void psci_system_off(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	int rc;

	rc = vmm_manager_guest_shutdown_request(vcpu->guest);
	if (rc) {
		vmm_printf("%s: guest=%s shutdown request failed (error %d)\n",
			   __func__, vcpu->guest->name, rc);
	}
}

static void psci_system_reset(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	int rc;

	rc = vmm_manager_guest_reboot_request(vcpu->guest);
	if (rc) {
		vmm_printf("%s: guest=%s reboot request failed (error %d)\n",
			   __func__, vcpu->guest->name, rc);
	}
}

static int emulate_psci_0_2_call(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	unsigned long psci_fn =
			emulate_psci_get_reg(vcpu, regs, 0) & ~((u32)0);
	unsigned long val;

	switch (psci_fn) {
	case PSCI_0_2_FN_PSCI_VERSION:
		/*
		 * Bits[31:16] = Major Version = 0
		 * Bits[15:0] = Minor Version = 2
		 */
		val = 2;
		break;
	case PSCI_0_2_FN_CPU_SUSPEND:
	case PSCI_0_2_FN64_CPU_SUSPEND:
		val = psci_vcpu_suspend(vcpu, regs);
		break;
	case PSCI_0_2_FN_CPU_OFF:
		psci_vcpu_off(vcpu, regs);
		val = PSCI_RET_SUCCESS;
		break;
	case PSCI_0_2_FN_CPU_ON:
	case PSCI_0_2_FN64_CPU_ON:
		val = psci_vcpu_on(vcpu, regs);
		break;
	case PSCI_0_2_FN_AFFINITY_INFO:
	case PSCI_0_2_FN64_AFFINITY_INFO:
		val = psci_vcpu_affinity_info(vcpu, regs);
		break;
	case PSCI_0_2_FN_MIGRATE:
	case PSCI_0_2_FN64_MIGRATE:
		val = PSCI_RET_NOT_SUPPORTED;
		break;
	case PSCI_0_2_FN_MIGRATE_INFO_TYPE:
		/*
		 * Trusted OS is MP hence does not require migration
	         * or
		 * Trusted OS is not present
		 */
		val = PSCI_0_2_TOS_MP;
		break;
	case PSCI_0_2_FN_MIGRATE_INFO_UP_CPU:
	case PSCI_0_2_FN64_MIGRATE_INFO_UP_CPU:
		val = PSCI_RET_NOT_SUPPORTED;
		break;
	case PSCI_0_2_FN_SYSTEM_OFF:
		psci_system_off(vcpu, regs);
		/*
		 * We should'nt be going back to guest VCPU after
		 * receiving SYSTEM_OFF request.
		 *
		 * If we accidently resume guest VCPU after SYSTEM_OFF
		 * request then guest VCPU should see internal failure
		 * from PSCI return value. To achieve this, we preload
		 * r0 (or x0) with PSCI return value INTERNAL_FAILURE.
		 */
		val = PSCI_RET_INTERNAL_FAILURE;
		break;
	case PSCI_0_2_FN_SYSTEM_RESET:
		psci_system_reset(vcpu, regs);
		/*
		 * Same reason as SYSTEM_OFF for preloading r0 (or x0)
		 * with PSCI return value INTERNAL_FAILURE.
		 */
		val = PSCI_RET_INTERNAL_FAILURE;
		break;
	default:
		return VMM_EINVALID;
	}

	emulate_psci_set_reg(vcpu, regs, 0, val);

	return VMM_OK;
}

/* PSCI v0.1 function numbers */
#define PSCI_FN_BASE		0x95c1ba5e
#define PSCI_FN(n)		(PSCI_FN_BASE + (n))

#define PSCI_FN_CPU_SUSPEND	PSCI_FN(0)
#define PSCI_FN_CPU_OFF		PSCI_FN(1)
#define PSCI_FN_CPU_ON		PSCI_FN(2)
#define PSCI_FN_MIGRATE		PSCI_FN(3)

static int emulate_psci_0_1_call(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	unsigned long psci_fn =
			emulate_psci_get_reg(vcpu, regs, 0) & ~((u32)0);
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

int emulate_psci_call(struct vmm_vcpu *vcpu, arch_regs_t *regs, bool is_smc)
{
	if (!vcpu || !regs) {
		return VMM_EINVALID;
	}

	switch (emulate_psci_version(vcpu)) {
	case 1: /* PSCI v0.1 */
		return emulate_psci_0_1_call(vcpu, regs);
	case 2: /* PSCI v0.2 */
		return emulate_psci_0_2_call(vcpu, regs);
	default:
		break;
	};

	return VMM_EINVALID;
}

