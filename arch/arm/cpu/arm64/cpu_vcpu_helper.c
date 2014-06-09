/**
 * Copyright (c) 2013 Sukanto Ghosh.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modifycpu_vcpu_helper.c
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
 * @file cpu_vcpu_helper.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source of VCPU helper functions
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <arch_barrier.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_sysregs.h>
#include <cpu_vcpu_vfp.h>
#include <cpu_vcpu_helper.h>

#include <generic_timer.h>
#include <arm_features.h>
#include <mmu_lpae.h>

void cpu_vcpu_halt(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	if (vmm_manager_vcpu_get_state(vcpu) != VMM_VCPU_STATE_HALTED) {
		vmm_printf("\n");
		cpu_vcpu_dump_user_reg(regs);
		vmm_manager_vcpu_halt(vcpu);
	}
}

void cpu_vcpu_spsr32_update(struct vmm_vcpu *vcpu, u32 mode, u32 new_spsr)
{
	struct arm_priv_sysregs *s = &arm_priv(vcpu)->sysregs;

	switch (mode) {
	case CPSR_MODE_ABORT:
		msr(spsr_abt, new_spsr);
		s->spsr_abt = new_spsr;
		break;
	case CPSR_MODE_UNDEFINED:
		msr(spsr_und, new_spsr);
		s->spsr_und = new_spsr;
		break;
	case CPSR_MODE_SUPERVISOR:
		msr(spsr_el1, new_spsr);
		s->spsr_el1 = new_spsr;
		break;
	case CPSR_MODE_IRQ:
		msr(spsr_irq, new_spsr);
		s->spsr_irq = new_spsr;
		break;
	case CPSR_MODE_FIQ:
		msr(spsr_fiq, new_spsr);
		s->spsr_fiq = new_spsr;
		break;
	case CPSR_MODE_HYPERVISOR:
		msr(spsr_el2, new_spsr);
		break;
	default:
		break;
	};
}

u32 cpu_vcpu_regmode32_read(arch_regs_t *regs, u32 mode, u32 reg)
{
	switch (reg) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		return regs->gpr[reg];
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
		if (mode == CPSR_MODE_FIQ) {
			return regs->gpr[16 + reg];
		} else {
			return regs->gpr[reg];
		}
	case 13:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			return regs->gpr[13];
		case CPSR_MODE_FIQ:
			return regs->gpr[29];
		case CPSR_MODE_IRQ:
			return regs->gpr[17];
		case CPSR_MODE_SUPERVISOR:
			return regs->gpr[19];
		case CPSR_MODE_ABORT:
			return regs->gpr[21];
		case CPSR_MODE_UNDEFINED:
			return regs->gpr[23];
		case CPSR_MODE_HYPERVISOR:
			return regs->gpr[15];
		default:
			break;
		};
		break;
	case 14:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			return regs->gpr[14];
		case CPSR_MODE_FIQ:
			return regs->lr;
		case CPSR_MODE_IRQ:
			return regs->gpr[16];
		case CPSR_MODE_SUPERVISOR:
			return regs->gpr[18];
		case CPSR_MODE_ABORT:
			return regs->gpr[20];
		case CPSR_MODE_UNDEFINED:
			return regs->gpr[22];
		case CPSR_MODE_HYPERVISOR:
			return regs->gpr[14];
		default:
			break;
		};
		break;
	case 15:
		return regs->pc;
	default:
		break;
	};

	return 0x0;
}

void cpu_vcpu_regmode32_write(arch_regs_t *regs, 
			      u32 mode, u32 reg, u32 val)
{
	switch (reg) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		regs->gpr[reg] = val;
		break;
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
		if (mode == CPSR_MODE_FIQ) {
			regs->gpr[16 + reg] = val;
		} else {
			regs->gpr[reg] = val;
		}
		break;
	case 13:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			regs->gpr[13] = val;
			break;
		case CPSR_MODE_FIQ:
			regs->gpr[29] = val;
			break;
		case CPSR_MODE_IRQ:
			regs->gpr[17] = val;
			break;
		case CPSR_MODE_SUPERVISOR:
			regs->gpr[19] = val;
			break;
		case CPSR_MODE_ABORT:
			regs->gpr[21] = val;
			break;
		case CPSR_MODE_UNDEFINED:
			regs->gpr[23] = val;
			break;
		case CPSR_MODE_HYPERVISOR:
			regs->gpr[15] = val;
			break;
		default:
			break;
		};
		break;
	case 14:
		switch (mode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			regs->gpr[14] = val;
		case CPSR_MODE_FIQ:
			regs->lr = val;
		case CPSR_MODE_IRQ:
			regs->gpr[16] = val;
		case CPSR_MODE_SUPERVISOR:
			regs->gpr[18] = val;
		case CPSR_MODE_ABORT:
			regs->gpr[20] = val;
		case CPSR_MODE_UNDEFINED:
			regs->gpr[22] = val;
		case CPSR_MODE_HYPERVISOR:
			regs->gpr[14] = val;
		default:
			break;
		};
		break;
	case 15:
		regs->pc = val;
		break;
	default:
		break;
	};
}

u64 cpu_vcpu_reg64_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs, 
			u32 reg) 
{
	u64 ret;

	if (reg < CPU_GPR_COUNT) {
		ret = regs->gpr[reg];
	} else if (reg == 30) {
		ret = regs->lr;
	} else {
		/* No such GPR */
		ret = 0;
	}

	/* Truncate bits[63:32] for AArch32 mode */
	if (regs->pstate & PSR_MODE32) {
		ret = ret & 0xFFFFFFFFULL;
	}

	return ret;
}

void cpu_vcpu_reg64_write(struct vmm_vcpu *vcpu, 
			  arch_regs_t *regs, 
			  u32 reg, u64 val)
{
	/* Truncate bits[63:32] for AArch32 mode */
	if (regs->pstate & PSR_MODE32) {
		val = val & 0xFFFFFFFFULL;
	}

	if (reg < CPU_GPR_COUNT) {
		regs->gpr[reg] = val;
	} else if (reg == 30) {
		regs->lr = val;
	} else {
		/* No such GPR */
	}
}

u64 cpu_vcpu_reg_read(struct vmm_vcpu *vcpu, 
		      arch_regs_t *regs, 
		      u32 reg) 
{
	if (regs->pstate & PSR_MODE32) {
		return cpu_vcpu_regmode32_read(regs, 
				regs->pstate & PSR_MODE32_MASK, reg & 0xF);
	} else {
		return cpu_vcpu_reg64_read(vcpu, regs, reg);
	}
}

void cpu_vcpu_reg_write(struct vmm_vcpu *vcpu, 
		        arch_regs_t *regs, 
		        u32 reg, u64 val)
{
	if (regs->pstate & PSR_MODE32) {
		cpu_vcpu_regmode32_write(regs, 
			regs->pstate & PSR_MODE32_MASK, reg & 0xF, val);
	} else {
		cpu_vcpu_reg64_write(vcpu, regs, reg, val);
	}
}

int arch_guest_init(struct vmm_guest *guest)
{
	if (!guest->reset_count) {
		guest->arch_priv = vmm_malloc(sizeof(struct arm_guest_priv));
		if (!guest->arch_priv) {
			return VMM_EFAIL;
		}
		arm_guest_priv(guest)->ttbl = mmu_lpae_ttbl_alloc(TTBL_STAGE2);
	}
	return VMM_OK;
}

int arch_guest_deinit(struct vmm_guest *guest)
{
	int rc;

	if (guest->arch_priv) {
		if ((rc = mmu_lpae_ttbl_free(arm_guest_priv(guest)->ttbl))) {
			return rc;
		}
		vmm_free(guest->arch_priv);
	}

	return VMM_OK;
}

int arch_vcpu_init(struct vmm_vcpu *vcpu)
{
	int rc;
	u32 cpuid = 0;
	const char *attr;
	irq_flags_t flags;
	u32 phys_timer_irq, virt_timer_irq;

	/* For both Orphan & Normal VCPUs */
	memset(arm_regs(vcpu), 0, sizeof(arch_regs_t));
	arm_regs(vcpu)->pc = vcpu->start_pc;
	arm_regs(vcpu)->sp = vcpu->stack_va + vcpu->stack_sz - 8;
	if (!vcpu->is_normal) {
		arm_regs(vcpu)->pstate = PSR_MODE64_EL2h;
		arm_regs(vcpu)->pstate |= PSR_ASYNC_ABORT_DISABLED;
		return VMM_OK;
	}

	/* Following initialization for normal VCPUs only */
	rc = vmm_devtree_read_string(vcpu->node, 
			VMM_DEVTREE_COMPATIBLE_ATTR_NAME, &attr);
	if (rc) {
		goto fail;
	}
	if (strcmp(attr, "armv7a,cortex-a8") == 0) {
		cpuid = ARM_CPUID_CORTEXA8;
		arm_regs(vcpu)->pstate = PSR_MODE32;
	} else if (strcmp(attr, "armv7a,cortex-a9") == 0) {
		cpuid = ARM_CPUID_CORTEXA9;
		arm_regs(vcpu)->pstate = PSR_MODE32;
	} else if (strcmp(attr, "armv7a,cortex-a15") == 0) {
		cpuid = ARM_CPUID_CORTEXA15;
		arm_regs(vcpu)->pstate = PSR_MODE32;
	} else if (strcmp(attr, "armv7a,cortex-a7") == 0) {
		cpuid = ARM_CPUID_CORTEXA7;
		arm_regs(vcpu)->pstate = PSR_MODE32;
	} else if (strcmp(attr, "armv8,generic") == 0) {
		cpuid = ARM_CPUID_ARMV8;
	} else {
		rc = VMM_EINVALID;
		goto fail;
	}
	if (arm_regs(vcpu)->pstate == PSR_MODE32) {
		/* Check if the host supports A32 mode @ EL1 */
		if (!cpu_supports_el1_a32()) {
			vmm_printf("Host does not support AArch32 mode\n");
			rc = VMM_ENOTAVAIL;
			goto fail;
		}
		arm_regs(vcpu)->pstate |= PSR_ZERO_MASK;
		arm_regs(vcpu)->pstate |= PSR_MODE32_SUPERVISOR;
	} else {
		arm_regs(vcpu)->pstate |= PSR_MODE64_DEBUG_DISABLED;
		arm_regs(vcpu)->pstate |= PSR_MODE64_EL1h;
	}
	arm_regs(vcpu)->pstate |= PSR_ASYNC_ABORT_DISABLED;
	arm_regs(vcpu)->pstate |= PSR_IRQ_DISABLED;
	arm_regs(vcpu)->pstate |= PSR_FIQ_DISABLED;

	/* First time initialization of private context */
	if (!vcpu->reset_count) {
		/* Alloc private context */
		vcpu->arch_priv = vmm_zalloc(sizeof(struct arm_priv));
		if (!vcpu->arch_priv) {
			rc = VMM_ENOMEM;
			goto fail;
		}
		/* Setup CPUID value expected by VCPU in MIDR register
		 * as-per HW specifications.
		 */
		arm_priv(vcpu)->cpuid = cpuid;
		/* Initialize VCPU features */
		arm_priv(vcpu)->features = 0;
		switch (cpuid) {
		case ARM_CPUID_CORTEXA8:
			arm_set_feature(vcpu, ARM_FEATURE_V7);
			arm_set_feature(vcpu, ARM_FEATURE_VFP3);
			arm_set_feature(vcpu, ARM_FEATURE_NEON);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2EE);
			arm_set_feature(vcpu, ARM_FEATURE_DUMMY_C15_REGS);
			arm_set_feature(vcpu, ARM_FEATURE_TRUSTZONE);
			break;
		case ARM_CPUID_CORTEXA9:
			arm_set_feature(vcpu, ARM_FEATURE_V7);
			arm_set_feature(vcpu, ARM_FEATURE_VFP3);
			arm_set_feature(vcpu, ARM_FEATURE_VFP_FP16);
			arm_set_feature(vcpu, ARM_FEATURE_NEON);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2EE);
			arm_set_feature(vcpu, ARM_FEATURE_V7MP);
			arm_set_feature(vcpu, ARM_FEATURE_TRUSTZONE);
			break;
		case ARM_CPUID_CORTEXA7:
			arm_set_feature(vcpu, ARM_FEATURE_V7);
			arm_set_feature(vcpu, ARM_FEATURE_VFP4);
			arm_set_feature(vcpu, ARM_FEATURE_VFP_FP16);
			arm_set_feature(vcpu, ARM_FEATURE_NEON);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2EE);
			arm_set_feature(vcpu, ARM_FEATURE_ARM_DIV);
			arm_set_feature(vcpu, ARM_FEATURE_V7MP);
			arm_set_feature(vcpu, ARM_FEATURE_GENERIC_TIMER);
			arm_set_feature(vcpu, ARM_FEATURE_DUMMY_C15_REGS);
			arm_set_feature(vcpu, ARM_FEATURE_LPAE);
			arm_set_feature(vcpu, ARM_FEATURE_TRUSTZONE);
			break;
		case ARM_CPUID_CORTEXA15:
			arm_set_feature(vcpu, ARM_FEATURE_V7);
			arm_set_feature(vcpu, ARM_FEATURE_VFP4);
			arm_set_feature(vcpu, ARM_FEATURE_VFP_FP16);
			arm_set_feature(vcpu, ARM_FEATURE_NEON);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2EE);
			arm_set_feature(vcpu, ARM_FEATURE_ARM_DIV);
			arm_set_feature(vcpu, ARM_FEATURE_V7MP);
			arm_set_feature(vcpu, ARM_FEATURE_GENERIC_TIMER);
			arm_set_feature(vcpu, ARM_FEATURE_DUMMY_C15_REGS);
			arm_set_feature(vcpu, ARM_FEATURE_LPAE);
			arm_set_feature(vcpu, ARM_FEATURE_TRUSTZONE);
			break;
		case ARM_CPUID_ARMV8:
			arm_set_feature(vcpu, ARM_FEATURE_V8);
			arm_set_feature(vcpu, ARM_FEATURE_VFP4);
			arm_set_feature(vcpu, ARM_FEATURE_ARM_DIV);
			arm_set_feature(vcpu, ARM_FEATURE_LPAE);
			arm_set_feature(vcpu, ARM_FEATURE_GENERIC_TIMER);
			break;
		default:
			break;
		};
		/* Some features automatically imply others: */
		if (arm_feature(vcpu, ARM_FEATURE_V7)) {
			arm_set_feature(vcpu, ARM_FEATURE_VAPA);
			arm_set_feature(vcpu, ARM_FEATURE_THUMB2);
			arm_set_feature(vcpu, ARM_FEATURE_MPIDR);
			if (!arm_feature(vcpu, ARM_FEATURE_M)) {
				arm_set_feature(vcpu, ARM_FEATURE_V6K);
			} else {
				arm_set_feature(vcpu, ARM_FEATURE_V6);
			}
		}
		if (arm_feature(vcpu, ARM_FEATURE_V6K)) {
			arm_set_feature(vcpu, ARM_FEATURE_V6);
			arm_set_feature(vcpu, ARM_FEATURE_MVFR);
		}
		if (arm_feature(vcpu, ARM_FEATURE_V6)) {
			arm_set_feature(vcpu, ARM_FEATURE_V5);
			if (!arm_feature(vcpu, ARM_FEATURE_M)) {
				arm_set_feature(vcpu, ARM_FEATURE_AUXCR);
			}
		}
		if (arm_feature(vcpu, ARM_FEATURE_V5)) {
			arm_set_feature(vcpu, ARM_FEATURE_V4T);
		}
		if (arm_feature(vcpu, ARM_FEATURE_M)) {
			arm_set_feature(vcpu, ARM_FEATURE_THUMB_DIV);
		}
		if (arm_feature(vcpu, ARM_FEATURE_ARM_DIV)) {
			arm_set_feature(vcpu, ARM_FEATURE_THUMB_DIV);
		}
		if (arm_feature(vcpu, ARM_FEATURE_VFP4)) {
			arm_set_feature(vcpu, ARM_FEATURE_VFP3);
		}
		if (arm_feature(vcpu, ARM_FEATURE_VFP3)) {
			arm_set_feature(vcpu, ARM_FEATURE_VFP);
		}
		if (arm_feature(vcpu, ARM_FEATURE_LPAE)) {
			arm_set_feature(vcpu, ARM_FEATURE_PXN);
		}
		/* Initialize Hypervisor Configuration */
		INIT_SPIN_LOCK(&arm_priv(vcpu)->hcr_lock);
		arm_priv(vcpu)->hcr =  (HCR_TACR_MASK |
					HCR_TIDCP_MASK |
					HCR_TSC_MASK |
					HCR_TWI_MASK |
					HCR_AMO_MASK |
					HCR_IMO_MASK |
					HCR_FMO_MASK |
					HCR_SWIO_MASK |
					HCR_VM_MASK);
		if (!(arm_regs(vcpu)->pstate & PSR_MODE32)) {
			arm_priv(vcpu)->hcr |= HCR_RW_MASK;
		}
		/* Initialize Coprocessor Trap Register */
		arm_priv(vcpu)->cptr = CPTR_TTA_MASK;
		arm_priv(vcpu)->cptr |= CPTR_TFP_MASK;
		/* Initialize Hypervisor System Trap Register */
		arm_priv(vcpu)->hstr = 0;
		/* Cleanup VGIC context first time */
		arm_vgic_cleanup(vcpu);
	}

	/* Clear virtual exception bits in HCR */
	vmm_spin_lock_irqsave(&arm_priv(vcpu)->hcr_lock, flags);
	arm_priv(vcpu)->hcr &= ~(HCR_VSE_MASK | 
				 HCR_VI_MASK | 
				 HCR_VF_MASK);
	vmm_spin_unlock_irqrestore(&arm_priv(vcpu)->hcr_lock, flags);

	/* Set last host CPU to invalid value */
	arm_priv(vcpu)->last_hcpu = 0xFFFFFFFF;

	/* Initialize system registers */
	rc = cpu_vcpu_sysregs_init(vcpu, cpuid);
	if (rc) {
		goto fail_sysregs_init;
	}

	/* Initialize VFP registers */
	rc = cpu_vcpu_vfp_init(vcpu);
	if (rc) {
		goto fail_vfp_init;
	}

	/* Initialize generic timer context */
	if (arm_feature(vcpu, ARM_FEATURE_GENERIC_TIMER)) {
		if (vmm_devtree_read_u32(vcpu->node, 
					 "gentimer_phys_irq",
					 &phys_timer_irq)) {
			phys_timer_irq = 0;
		}
		if (vmm_devtree_read_u32(vcpu->node, 
					 "gentimer_virt_irq",
					 &virt_timer_irq)) {
			virt_timer_irq = 0;
		}
		rc = generic_timer_vcpu_context_init(
						&arm_gentimer_context(vcpu),
						phys_timer_irq,
						virt_timer_irq);
		if (rc) {
			goto fail_gentimer_init;
		}
	}

	return VMM_OK;

fail_gentimer_init:
	if (!vcpu->reset_count) {
		cpu_vcpu_vfp_deinit(vcpu);
	}
fail_vfp_init:
	if (!vcpu->reset_count) {
		cpu_vcpu_sysregs_deinit(vcpu);
	}
fail_sysregs_init:
	if (!vcpu->reset_count) {
		vmm_free(vcpu->arch_priv);
		vcpu->arch_priv = NULL;
	}
fail:
	return rc;
}

int arch_vcpu_deinit(struct vmm_vcpu *vcpu)
{
	int rc;

	/* For both Orphan & Normal VCPUs */
	memset(arm_regs(vcpu), 0, sizeof(arch_regs_t));

	/* For Orphan VCPUs do nothing else */
	if (!vcpu->is_normal) {
		return VMM_OK;
	}

	/* Free Generic Timer Context */
	if (arm_feature(vcpu, ARM_FEATURE_GENERIC_TIMER)) {
		if ((rc = generic_timer_vcpu_context_deinit(
				&arm_gentimer_context(vcpu)))) {
			return rc;
		}
	}

	/* Free VFP registers */
	rc = cpu_vcpu_vfp_deinit(vcpu);
	if (rc) {
		return rc;
	}

	/* Free system registers */
	rc = cpu_vcpu_sysregs_deinit(vcpu);
	if (rc) {
		return rc;
	}

	/* Free private context */
	vmm_free(vcpu->arch_priv);
	vcpu->arch_priv = NULL;

	return VMM_OK;
}

void arch_vcpu_switch(struct vmm_vcpu *tvcpu, 
		      struct vmm_vcpu *vcpu, 
		      arch_regs_t *regs)
{
	u32 ite;
	irq_flags_t flags;

	/* Save user registers & banked registers */
	if (tvcpu) {
		arm_regs(tvcpu)->pc = regs->pc;
		arm_regs(tvcpu)->lr = regs->lr;
		arm_regs(tvcpu)->sp = regs->sp;
		for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
			arm_regs(tvcpu)->gpr[ite] = regs->gpr[ite];
		}
		arm_regs(tvcpu)->pstate = regs->pstate;
		if (tvcpu->is_normal) {
			/* Update last host CPU */
			arm_priv(tvcpu)->last_hcpu = vmm_smp_processor_id();
			/* Save system registers */
			cpu_vcpu_sysregs_save(tvcpu);
			/* Save VFP and SIMD register */
			cpu_vcpu_vfp_regs_save(tvcpu);
			/* Save generic timer */
			if (arm_feature(tvcpu, ARM_FEATURE_GENERIC_TIMER)) {
				generic_timer_vcpu_context_save(
						arm_gentimer_context(tvcpu));
			}
			/* Save VGIC registers */
			arm_vgic_save(tvcpu);
		}
	}
	/* Restore user registers & special registers */
	regs->pc = arm_regs(vcpu)->pc;
	regs->lr = arm_regs(vcpu)->lr;
	regs->sp = arm_regs(vcpu)->sp;
	for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
		regs->gpr[ite] = arm_regs(vcpu)->gpr[ite];
	}
	regs->pstate = arm_regs(vcpu)->pstate;
	if (vcpu->is_normal) {
		/* Restore hypervisor context */
		vmm_spin_lock_irqsave(&arm_priv(vcpu)->hcr_lock, flags);
		msr(hcr_el2, arm_priv(vcpu)->hcr);
		vmm_spin_unlock_irqrestore(&arm_priv(vcpu)->hcr_lock, flags);
		msr(cptr_el2, arm_priv(vcpu)->cptr);
		msr(hstr_el2, arm_priv(vcpu)->hstr);
		/* Restore Stage2 MMU context */
		mmu_lpae_stage2_chttbl(vcpu->guest->id, 
			       arm_guest_priv(vcpu->guest)->ttbl);
		/* Restore VGIC registers */
		arm_vgic_restore(vcpu);
		/* Restore generic timer */
		if (arm_feature(vcpu, ARM_FEATURE_GENERIC_TIMER)) {
			generic_timer_vcpu_context_restore(
						arm_gentimer_context(vcpu));
		}
		/* Restore VFP and SIMD register */
		cpu_vcpu_vfp_regs_restore(vcpu);
		/* Restore system registers */
		cpu_vcpu_sysregs_restore(vcpu);
		/* Flush TLB if moved to new host CPU */
		if (arm_priv(vcpu)->last_hcpu != vmm_smp_processor_id()) {
			/* Invalidate all guest TLB enteries because
			 * we might have stale guest TLB enteries from
			 * our previous run on new_hcpu host CPU 
			 */
			inv_tlb_guest_allis();
			/* Ensure changes are visible */
			dsb();
			isb();
		}
	}
	/* Clear exclusive monitor */
	clrex();
}

void arch_vcpu_preempt_orphan(void)
{
	/* Trigger HVC call from hypervisor mode. This will cause
	 * do_soft_irq() function to call vmm_scheduler_preempt_orphan()
	 */
	asm volatile ("hvc #0\t\n");
}

static void __cpu_vcpu_dump_user_reg(struct vmm_chardev *cdev, 
				     arch_regs_t *regs)
{
	u32 i;

	vmm_cprintf(cdev, "Core Registers\n");
	vmm_cprintf(cdev, " %11s=0x%016lx %11s=0x%016lx\n",
		    "SP", regs->sp,
		    "LR", regs->lr);
	vmm_cprintf(cdev, " %11s=0x%016lx %11s=0x%08lx\n",
		    "PC", regs->pc,
		    "PSTATE", (regs->pstate & 0xffffffff));
	vmm_cprintf(cdev, "General Purpose Registers");
	for (i = 0; i < (CPU_GPR_COUNT); i++) {
		if (i % 2 == 0) {
			vmm_cprintf(cdev, "\n");
		}
		vmm_cprintf(cdev, " %9s%02d=0x%016lx",
			    "X", i, regs->gpr[i]);
	}
	vmm_cprintf(cdev, "\n");
}

void cpu_vcpu_dump_user_reg(arch_regs_t *regs)
{
	__cpu_vcpu_dump_user_reg(NULL, regs);
}

void arch_vcpu_regs_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	struct arm_priv *p;

	/* For both Normal & Orphan VCPUs */
	__cpu_vcpu_dump_user_reg(cdev, arm_regs(vcpu));

	/* For only Normal VCPUs */
	if (!vcpu->is_normal) {
		return;
	}

	/* Get private context */
	p = arm_priv(vcpu);

	/* Hypervisor registers */
	vmm_cprintf(cdev, "Hypervisor EL2 Registers\n");
	vmm_cprintf(cdev, " %11s=0x%016lx %11s=0x%016lx\n",
		    "HCR_EL2", p->hcr,
		    "CPTR_EL2", p->cptr);
	vmm_cprintf(cdev, " %11s=0x%016lx %11s=0x%016lx\n",
		    "HSTR_EL2", p->hstr,
		    "TTBR_EL2", arm_guest_priv(vcpu->guest)->ttbl->tbl_pa);

	/* Print VFP registers */
	cpu_vcpu_vfp_regs_dump(cdev, vcpu);

	/* Print system registers */
	cpu_vcpu_sysregs_dump(cdev, vcpu);
}

void arch_vcpu_stat_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	/* For now no arch specific stats */
}
