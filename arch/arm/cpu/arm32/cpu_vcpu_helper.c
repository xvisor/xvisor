/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file cpu_vcpu_helper.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source of VCPU helper functions
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_manager.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <cpu_defines.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_vfp.h>
#include <cpu_vcpu_cp14.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_helper.h>
#include <arm_features.h>

void cpu_vcpu_halt(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	if (!vcpu || !regs) {
		return;
	}

	if (vmm_manager_vcpu_get_state(vcpu) != VMM_VCPU_STATE_HALTED) {
		vmm_printf("\n");
		cpu_vcpu_dump_user_reg(vcpu, regs);
		vmm_manager_vcpu_halt(vcpu);
	}
}

u32 cpu_vcpu_cpsr_retrieve(struct vmm_vcpu *vcpu,
			  arch_regs_t *regs)
{
	if (!vcpu || !regs) {
		return 0;
	}
	if (vcpu->is_normal) {
		return (regs->cpsr & CPSR_USERBITS_MASK) |
			(arm_priv(vcpu)->cpsr & ~CPSR_USERBITS_MASK);
	} else {
		return regs->cpsr;
	}
}

static void cpu_vcpu_banked_regs_save(struct arm_priv *p, arch_regs_t *src)
{
	switch (p->cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_USER:
		p->gpr_usr[0] = src->gpr[8];
		p->gpr_usr[1] = src->gpr[9];
		p->gpr_usr[2] = src->gpr[10];
		p->gpr_usr[3] = src->gpr[11];
		p->gpr_usr[4] = src->gpr[12];
		p->sp_usr = src->sp;
		p->lr_usr = src->lr;
		break;
	case CPSR_MODE_SYSTEM:
		p->gpr_usr[0] = src->gpr[8];
		p->gpr_usr[1] = src->gpr[9];
		p->gpr_usr[2] = src->gpr[10];
		p->gpr_usr[3] = src->gpr[11];
		p->gpr_usr[4] = src->gpr[12];
		p->sp_usr = src->sp;
		p->lr_usr = src->lr;
		break;
	case CPSR_MODE_ABORT:
		p->gpr_usr[0] = src->gpr[8];
		p->gpr_usr[1] = src->gpr[9];
		p->gpr_usr[2] = src->gpr[10];
		p->gpr_usr[3] = src->gpr[11];
		p->gpr_usr[4] = src->gpr[12];
		p->sp_abt = src->sp;
		p->lr_abt = src->lr;
		break;
	case CPSR_MODE_UNDEFINED:
		p->gpr_usr[0] = src->gpr[8];
		p->gpr_usr[1] = src->gpr[9];
		p->gpr_usr[2] = src->gpr[10];
		p->gpr_usr[3] = src->gpr[11];
		p->gpr_usr[4] = src->gpr[12];
		p->sp_und = src->sp;
		p->lr_und = src->lr;
		break;
	case CPSR_MODE_MONITOR:
		p->gpr_usr[0] = src->gpr[8];
		p->gpr_usr[1] = src->gpr[9];
		p->gpr_usr[2] = src->gpr[10];
		p->gpr_usr[3] = src->gpr[11];
		p->gpr_usr[4] = src->gpr[12];
		p->sp_mon = src->sp;
		p->lr_mon = src->lr;
		break;
	case CPSR_MODE_SUPERVISOR:
		p->gpr_usr[0] = src->gpr[8];
		p->gpr_usr[1] = src->gpr[9];
		p->gpr_usr[2] = src->gpr[10];
		p->gpr_usr[3] = src->gpr[11];
		p->gpr_usr[4] = src->gpr[12];
		p->sp_svc = src->sp;
		p->lr_svc = src->lr;
		break;
	case CPSR_MODE_IRQ:
		p->gpr_usr[0] = src->gpr[8];
		p->gpr_usr[1] = src->gpr[9];
		p->gpr_usr[2] = src->gpr[10];
		p->gpr_usr[3] = src->gpr[11];
		p->gpr_usr[4] = src->gpr[12];
		p->sp_irq = src->sp;
		p->lr_irq = src->lr;
		break;
	case CPSR_MODE_FIQ:
		p->gpr_fiq[0] = src->gpr[8];
		p->gpr_fiq[1] = src->gpr[9];
		p->gpr_fiq[2] = src->gpr[10];
		p->gpr_fiq[3] = src->gpr[11];
		p->gpr_fiq[4] = src->gpr[12];
		p->sp_fiq = src->sp;
		p->lr_fiq = src->lr;
		break;
	default:
		break;
	};
}

static void cpu_vcpu_banked_regs_restore(struct arm_priv *p, arch_regs_t *dst)
{
	switch (p->cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_USER:
		dst->gpr[8] = p->gpr_usr[0];
		dst->gpr[9] = p->gpr_usr[1];
		dst->gpr[10] = p->gpr_usr[2];
		dst->gpr[11] = p->gpr_usr[3];
		dst->gpr[12] = p->gpr_usr[4];
		dst->sp = p->sp_usr;
		dst->lr = p->lr_usr;
		break;
	case CPSR_MODE_SYSTEM:
		dst->gpr[8] = p->gpr_usr[0];
		dst->gpr[9] = p->gpr_usr[1];
		dst->gpr[10] = p->gpr_usr[2];
		dst->gpr[11] = p->gpr_usr[3];
		dst->gpr[12] = p->gpr_usr[4];
		dst->sp = p->sp_usr;
		dst->lr = p->lr_usr;
		break;
	case CPSR_MODE_ABORT:
		dst->gpr[8] = p->gpr_usr[0];
		dst->gpr[9] = p->gpr_usr[1];
		dst->gpr[10] = p->gpr_usr[2];
		dst->gpr[11] = p->gpr_usr[3];
		dst->gpr[12] = p->gpr_usr[4];
		dst->sp = p->sp_abt;
		dst->lr = p->lr_abt;
		break;
	case CPSR_MODE_UNDEFINED:
		dst->gpr[8] = p->gpr_usr[0];
		dst->gpr[9] = p->gpr_usr[1];
		dst->gpr[10] = p->gpr_usr[2];
		dst->gpr[11] = p->gpr_usr[3];
		dst->gpr[12] = p->gpr_usr[4];
		dst->sp = p->sp_und;
		dst->lr = p->lr_und;
		break;
	case CPSR_MODE_MONITOR:
		dst->gpr[8] = p->gpr_usr[0];
		dst->gpr[9] = p->gpr_usr[1];
		dst->gpr[10] = p->gpr_usr[2];
		dst->gpr[11] = p->gpr_usr[3];
		dst->gpr[12] = p->gpr_usr[4];
		dst->sp = p->sp_mon;
		dst->lr = p->lr_mon;
		break;
	case CPSR_MODE_SUPERVISOR:
		dst->gpr[8] = p->gpr_usr[0];
		dst->gpr[9] = p->gpr_usr[1];
		dst->gpr[10] = p->gpr_usr[2];
		dst->gpr[11] = p->gpr_usr[3];
		dst->gpr[12] = p->gpr_usr[4];
		dst->sp = p->sp_svc;
		dst->lr = p->lr_svc;
		break;
	case CPSR_MODE_IRQ:
		dst->gpr[8] = p->gpr_usr[0];
		dst->gpr[9] = p->gpr_usr[1];
		dst->gpr[10] = p->gpr_usr[2];
		dst->gpr[11] = p->gpr_usr[3];
		dst->gpr[12] = p->gpr_usr[4];
		dst->sp = p->sp_irq;
		dst->lr = p->lr_irq;
		break;
	case CPSR_MODE_FIQ:
		dst->gpr[8] = p->gpr_fiq[0];
		dst->gpr[9] = p->gpr_fiq[1];
		dst->gpr[10] = p->gpr_fiq[2];
		dst->gpr[11] = p->gpr_fiq[3];
		dst->gpr[12] = p->gpr_fiq[4];
		dst->sp = p->sp_fiq;
		dst->lr = p->lr_fiq;
		break;
	default:
		break;
	};
}

void cpu_vcpu_cpsr_update(struct vmm_vcpu *vcpu,
			  arch_regs_t *regs,
			  u32 new_cpsr,
			  u32 new_cpsr_mask)
{
	bool mode_change;
	struct arm_priv *p;

	/* Sanity check */
	if (!vcpu || !vcpu->is_normal || !regs) {
		return;
	}
	p = arm_priv(vcpu);
	new_cpsr &= new_cpsr_mask;

	/* Determine if mode is changing */
	mode_change = FALSE;
	if ((new_cpsr_mask & CPSR_MODE_MASK) &&
	    ((p->cpsr & CPSR_MODE_MASK) != (new_cpsr & CPSR_MODE_MASK))) {
		mode_change = TRUE;
		/* Save banked registers for old CPSR */
		cpu_vcpu_banked_regs_save(p, regs);
	}

	/* Set the new priviledged bits of CPSR */
	p->cpsr &= (~CPSR_PRIVBITS_MASK | ~new_cpsr_mask);
	p->cpsr |= new_cpsr & CPSR_PRIVBITS_MASK & new_cpsr_mask;

	/* Set the new user bits of CPSR */
	regs->cpsr &= (~CPSR_USERBITS_MASK | ~new_cpsr_mask);
	regs->cpsr |= new_cpsr & CPSR_USERBITS_MASK & new_cpsr_mask;

	/* If mode is changing then */
	if (mode_change) {
		/* Restore values of banked registers for new CPSR */
		cpu_vcpu_banked_regs_restore(p, regs);
		/* Synchronize CP15 state to change in mode */
		cpu_vcpu_cp15_sync_cpsr(vcpu);
	}

	return;
}

u32 cpu_vcpu_spsr_retrieve(struct vmm_vcpu *vcpu)
{
	struct arm_priv *p = arm_priv(vcpu);

	/* Find out correct SPSR */
	switch (p->cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_ABORT:
		return p->spsr_abt;
	case CPSR_MODE_UNDEFINED:
		return p->spsr_und;
	case CPSR_MODE_MONITOR:
		return p->spsr_mon;
	case CPSR_MODE_SUPERVISOR:
		return p->spsr_svc;
	case CPSR_MODE_IRQ:
		return p->spsr_irq;
	case CPSR_MODE_FIQ:
		return p->spsr_fiq;
	default:
		break;
	};

	return 0;
}

int cpu_vcpu_spsr_update(struct vmm_vcpu *vcpu,
			 u32 new_spsr,
			 u32 new_spsr_mask)
{
	struct arm_priv *p;

	/* Sanity check */
	if (!vcpu || !vcpu->is_normal) {
		return VMM_EFAIL;
	}
	p = arm_priv(vcpu);

	/* VCPU cannot be in user mode */
	if ((p->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		return VMM_EFAIL;
	}

	new_spsr &= new_spsr_mask;

	/* Update appropriate SPSR */
	switch (p->cpsr & CPSR_MODE_MASK) {
	case CPSR_MODE_ABORT:
		p->spsr_abt &= ~new_spsr_mask;
		p->spsr_abt |= new_spsr;
		break;
	case CPSR_MODE_UNDEFINED:
		p->spsr_und &= ~new_spsr_mask;
		p->spsr_und |= new_spsr;
		break;
	case CPSR_MODE_MONITOR:
		p->spsr_mon &= ~new_spsr_mask;
		p->spsr_mon |= new_spsr;
		break;
	case CPSR_MODE_SUPERVISOR:
		p->spsr_svc &= ~new_spsr_mask;
		p->spsr_svc |= new_spsr;
		break;
	case CPSR_MODE_IRQ:
		p->spsr_irq &= ~new_spsr_mask;
		p->spsr_irq |= new_spsr;
		break;
	case CPSR_MODE_FIQ:
		p->spsr_fiq &= ~new_spsr_mask;
		p->spsr_fiq |= new_spsr;
		break;
	default:
		break;
	};

	/* Return success */
	return VMM_OK;
}

u32 cpu_vcpu_reg_read(struct vmm_vcpu *vcpu,
		      arch_regs_t *regs,
		      u32 reg_num)
{
	switch (reg_num) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
		return regs->gpr[reg_num];
		break;
	case 13:
		return regs->sp;
		break;
	case 14:
		return regs->lr;
		break;
	case 15:
		return regs->pc;
		break;
	default:
		break;
	};
	return 0x0;
}

void cpu_vcpu_reg_write(struct vmm_vcpu *vcpu,
			arch_regs_t *regs,
			u32 reg_num,
			u32 reg_val)
{
	struct arm_priv *p = arm_priv(vcpu);
	u32 curmode = p->cpsr & CPSR_MODE_MASK;

	switch (reg_num) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		regs->gpr[reg_num] = reg_val;
		break;
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
		regs->gpr[reg_num] = reg_val;
		if (curmode == CPSR_MODE_FIQ) {
			p->gpr_fiq[reg_num - 8] = reg_val;
		} else {
			p->gpr_usr[reg_num - 8] = reg_val;
		}
		break;
	case 13:
		regs->sp = reg_val;
		switch (curmode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			p->sp_usr = reg_val;
			break;
		case CPSR_MODE_FIQ:
			p->sp_fiq = reg_val;
			break;
		case CPSR_MODE_IRQ:
			p->sp_irq = reg_val;
			break;
		case CPSR_MODE_SUPERVISOR:
			p->sp_svc = reg_val;
			break;
		case CPSR_MODE_ABORT:
			p->sp_abt = reg_val;
			break;
		case CPSR_MODE_UNDEFINED:
			p->sp_und = reg_val;
			break;
		case CPSR_MODE_MONITOR:
			p->sp_mon = reg_val;
			break;
		default:
			break;
		};
		break;
	case 14:
		regs->lr = reg_val;
		switch (curmode) {
		case CPSR_MODE_USER:
		case CPSR_MODE_SYSTEM:
			p->lr_usr = reg_val;
			break;
		case CPSR_MODE_FIQ:
			p->lr_fiq = reg_val;
			break;
		case CPSR_MODE_IRQ:
			p->lr_irq = reg_val;
			break;
		case CPSR_MODE_SUPERVISOR:
			p->lr_svc = reg_val;
			break;
		case CPSR_MODE_ABORT:
			p->lr_abt = reg_val;
			break;
		case CPSR_MODE_UNDEFINED:
			p->lr_und = reg_val;
			break;
		case CPSR_MODE_MONITOR:
			p->lr_mon = reg_val;
			break;
		default:
			break;
		};
		break;
	case 15:
		regs->pc = reg_val;
		break;
	default:
		break;
	};
}

u32 cpu_vcpu_regmode_read(struct vmm_vcpu *vcpu,
			  arch_regs_t *regs,
			  u32 mode,
			  u32 reg_num)
{
	struct arm_priv *p = arm_priv(vcpu);
	u32 curmode = p->cpsr & CPSR_MODE_MASK;

	if (mode == curmode) {
		return cpu_vcpu_reg_read(vcpu, regs, reg_num);
	} else {
		switch (reg_num) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			return regs->gpr[reg_num];
			break;
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			if (curmode == CPSR_MODE_FIQ) {
				return p->gpr_usr[reg_num - 8];
			} else {
				if (mode == CPSR_MODE_FIQ) {
					return p->gpr_fiq[reg_num - 8];
				} else {
					return regs->gpr[reg_num];
				}
			}
			break;
		case 13:
			switch (mode) {
			case CPSR_MODE_USER:
			case CPSR_MODE_SYSTEM:
				return p->sp_usr;
				break;
			case CPSR_MODE_FIQ:
				return p->sp_fiq;
				break;
			case CPSR_MODE_IRQ:
				return p->sp_irq;
				break;
			case CPSR_MODE_SUPERVISOR:
				return p->sp_svc;
				break;
			case CPSR_MODE_ABORT:
				return p->sp_abt;
				break;
			case CPSR_MODE_UNDEFINED:
				return p->sp_und;
				break;
			case CPSR_MODE_MONITOR:
				return p->sp_mon;
				break;
			default:
				break;
			};
			break;
		case 14:
			switch (mode) {
			case CPSR_MODE_USER:
			case CPSR_MODE_SYSTEM:
				return p->lr_usr;
				break;
			case CPSR_MODE_FIQ:
				return p->lr_fiq;
				break;
			case CPSR_MODE_IRQ:
				return p->lr_irq;
				break;
			case CPSR_MODE_SUPERVISOR:
				return p->lr_svc;
				break;
			case CPSR_MODE_ABORT:
				return p->lr_abt;
				break;
			case CPSR_MODE_UNDEFINED:
				return p->lr_und;
				break;
			case CPSR_MODE_MONITOR:
				return p->lr_mon;
				break;
			default:
				break;
			};
			break;
		case 15:
			return regs->pc;
			break;
		default:
			break;
		};
	}

	return 0x0;
}

void cpu_vcpu_regmode_write(struct vmm_vcpu *vcpu,
			    arch_regs_t *regs,
			    u32 mode,
			    u32 reg_num,
			    u32 reg_val)
{
	struct arm_priv *p = arm_priv(vcpu);
	u32 curmode = p->cpsr & CPSR_MODE_MASK;

	if (mode == curmode) {
		cpu_vcpu_reg_write(vcpu, regs, reg_num, reg_val);
	} else {
		switch (reg_num) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			regs->gpr[reg_num] = reg_val;
			break;
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			if (curmode == CPSR_MODE_FIQ) {
				p->gpr_usr[reg_num - 8] = reg_val;
			} else {
				if (mode == CPSR_MODE_FIQ) {
					p->gpr_fiq[reg_num - 8] = reg_val;
				} else {
					regs->gpr[reg_num] = reg_val;
				}
			}
			break;
		case 13:
			switch (mode) {
			case CPSR_MODE_USER:
			case CPSR_MODE_SYSTEM:
				p->sp_usr = reg_val;
				break;
			case CPSR_MODE_FIQ:
				p->sp_fiq = reg_val;
				break;
			case CPSR_MODE_IRQ:
				p->sp_irq = reg_val;
				break;
			case CPSR_MODE_SUPERVISOR:
				p->sp_svc = reg_val;
				break;
			case CPSR_MODE_ABORT:
				p->sp_abt = reg_val;
				break;
			case CPSR_MODE_UNDEFINED:
				p->sp_und = reg_val;
				break;
			case CPSR_MODE_MONITOR:
				p->sp_mon = reg_val;
				break;
			default:
				break;
			};
			break;
		case 14:
			switch (mode) {
			case CPSR_MODE_USER:
			case CPSR_MODE_SYSTEM:
				p->lr_usr = reg_val;
				break;
			case CPSR_MODE_FIQ:
				p->lr_fiq = reg_val;
				break;
			case CPSR_MODE_IRQ:
				p->lr_irq = reg_val;
				break;
			case CPSR_MODE_SUPERVISOR:
				p->lr_svc = reg_val;
				break;
			case CPSR_MODE_ABORT:
				p->lr_abt = reg_val;
				break;
			case CPSR_MODE_UNDEFINED:
				p->lr_und = reg_val;
				break;
			case CPSR_MODE_MONITOR:
				p->lr_mon = reg_val;
				break;
			default:
				break;
			};
			break;
		case 15:
			regs->pc = reg_val;
			break;
		default:
			break;
		};
	}
}

int arch_guest_init(struct vmm_guest *guest)
{
	int rc;
	u32 ovect_flags;
	virtual_addr_t ovect_va;
	struct cpu_page pg;

	if (!guest->reset_count) {
		guest->arch_priv = vmm_zalloc(sizeof(struct arm_guest_priv));
		if (!guest->arch_priv) {
			rc = VMM_EFAIL;
			goto fail;
		}
		ovect_flags = 0x0;
		ovect_flags |= VMM_MEMORY_READABLE;
		ovect_flags |= VMM_MEMORY_WRITEABLE;
		ovect_flags |= VMM_MEMORY_CACHEABLE;
		ovect_flags |= VMM_MEMORY_EXECUTABLE;
		ovect_va = vmm_host_alloc_pages(1, ovect_flags);
		if (!ovect_va) {
			rc = VMM_EFAIL;
			goto fail;
		}
		if ((rc = cpu_mmu_get_reserved_page(ovect_va, &pg))) {
			goto fail_freepages;
		}
		if ((rc = cpu_mmu_unmap_reserved_page(&pg))) {
			goto fail_freepages;
		}
#if defined(CONFIG_ARMV5)
		pg.ap = TTBL_AP_SRW_UR;
#else
		if (pg.ap == TTBL_AP_SR_U) {
			pg.ap = TTBL_AP_SR_UR;
		} else {
			pg.ap = TTBL_AP_SRW_UR;
		}
#endif
		if ((rc = cpu_mmu_map_reserved_page(&pg))) {
			goto fail_freepages;
		}
		arm_guest_priv(guest)->ovect = (u32 *)ovect_va;

		if (vmm_devtree_read_u32(guest->node,
				"psci_version",
				 &arm_guest_priv(guest)->psci_version)) {
			/* By default, assume PSCI v0.1 */
			arm_guest_priv(guest)->psci_version = 1;
		}
	}

	return VMM_OK;

fail_freepages:
	if (arm_guest_priv(guest)->ovect) {
		vmm_host_free_pages(
			(virtual_addr_t)arm_guest_priv(guest)->ovect, 1);
	}
fail:
	return rc;
}

int arch_guest_deinit(struct vmm_guest *guest)
{
	int rc;
	if (guest->arch_priv) {
		if (arm_guest_priv(guest)->ovect) {
			rc = vmm_host_free_pages(
			     (virtual_addr_t)arm_guest_priv(guest)->ovect, 1);
			if (rc) {
				return rc;
			}
		}
		vmm_free(guest->arch_priv);
	}
	return VMM_OK;
}

int arch_guest_add_region(struct vmm_guest *guest, struct vmm_region *region)
{
	return VMM_OK;
}

int arch_guest_del_region(struct vmm_guest *guest, struct vmm_region *region)
{
	return VMM_OK;
}

int arch_vcpu_init(struct vmm_vcpu *vcpu)
{
	int rc;
	u32 ite, cpuid;
	const char *attr;

	/* Initialize User Mode Registers */
	/* For both Orphan & Normal VCPUs */

	memset(arm_regs(vcpu), 0, sizeof(arch_regs_t));
	arm_regs(vcpu)->pc = vcpu->start_pc;

	/*
	 * Stacks must be 64-bits aligned to respect AAPCS:
	 * Procedure Call Standard for the ARM Architecture.
	 * To do so, AAPCS advises that all SP must be set to
	 * a value which is 0 modulo 8.
	 * The compiler takes care of the frame size.
	 *
	 * This is terribly important because it messes runtime
	 * with values greater than 32 bits (e.g. 64-bits integers).
	 */
	arm_regs(vcpu)->sp_excp = vcpu->stack_va + vcpu->stack_sz - 8;
	arm_regs(vcpu)->sp_excp = arm_regs(vcpu)->sp_excp & ~0x7;

	if (vcpu->is_normal) {
		arm_regs(vcpu)->cpsr  = CPSR_ZERO_MASK;
		arm_regs(vcpu)->cpsr |= CPSR_ASYNC_ABORT_DISABLED;
		arm_regs(vcpu)->cpsr |= CPSR_MODE_USER;
		arm_regs(vcpu)->sp = 0;
	} else {
		arm_regs(vcpu)->cpsr  = CPSR_ZERO_MASK;
		arm_regs(vcpu)->cpsr |= CPSR_ASYNC_ABORT_DISABLED;
		arm_regs(vcpu)->cpsr |= CPSR_MODE_SUPERVISOR;
		arm_regs(vcpu)->sp = arm_regs(vcpu)->sp_excp;
	}

	/* Initialize Supervisor Mode Registers */
	/* For only Normal VCPUs */

	if (!vcpu->is_normal) {
		return VMM_OK;
	}
	rc = vmm_devtree_read_string(vcpu->node,
			VMM_DEVTREE_COMPATIBLE_ATTR_NAME, &attr);
	if (rc) {
		goto fail;
	}
	if (strcmp(attr, "armv5te,arm926ej") == 0) {
		cpuid = ARM_CPUID_ARM926;
	} else if (strcmp(attr, "armv6,arm11mp") == 0) {
		cpuid = ARM_CPUID_ARM11MPCORE;
	} else if (strcmp(attr, "armv7a,cortex-a8") == 0) {
		cpuid = ARM_CPUID_CORTEXA8;
	} else if (strcmp(attr, "armv7a,cortex-a9") == 0) {
		cpuid = ARM_CPUID_CORTEXA9;
	} else {
		rc = VMM_EINVALID;
		goto fail;
	}
	if (!vcpu->reset_count) {
		vcpu->arch_priv = vmm_zalloc(sizeof(struct arm_priv));
		arm_priv(vcpu)->cpsr = CPSR_ASYNC_ABORT_DISABLED |
				   CPSR_IRQ_DISABLED |
				   CPSR_FIQ_DISABLED |
				   CPSR_MODE_SUPERVISOR;
	} else {
		for (ite = 0; ite < CPU_FIQ_GPR_COUNT; ite++) {
			arm_priv(vcpu)->gpr_usr[ite] = 0x0;
			arm_priv(vcpu)->gpr_fiq[ite] = 0x0;
		}
		arm_priv(vcpu)->sp_usr = 0x0;
		arm_priv(vcpu)->lr_usr = 0x0;
		arm_priv(vcpu)->sp_svc = 0x0;
		arm_priv(vcpu)->lr_svc = 0x0;
		arm_priv(vcpu)->spsr_svc = 0x0;
		arm_priv(vcpu)->sp_mon = 0x0;
		arm_priv(vcpu)->lr_mon = 0x0;
		arm_priv(vcpu)->spsr_mon = 0x0;
		arm_priv(vcpu)->sp_abt = 0x0;
		arm_priv(vcpu)->lr_abt = 0x0;
		arm_priv(vcpu)->spsr_abt = 0x0;
		arm_priv(vcpu)->sp_und = 0x0;
		arm_priv(vcpu)->lr_und = 0x0;
		arm_priv(vcpu)->spsr_und = 0x0;
		arm_priv(vcpu)->sp_irq = 0x0;
		arm_priv(vcpu)->lr_irq = 0x0;
		arm_priv(vcpu)->spsr_irq = 0x0;
		arm_priv(vcpu)->sp_fiq = 0x0;
		arm_priv(vcpu)->lr_fiq = 0x0;
		arm_priv(vcpu)->spsr_fiq = 0x0;
		cpu_vcpu_cpsr_update(vcpu,
				     arm_regs(vcpu),
				     (CPSR_ZERO_MASK |
					CPSR_ASYNC_ABORT_DISABLED |
					CPSR_IRQ_DISABLED |
					CPSR_FIQ_DISABLED |
					CPSR_MODE_SUPERVISOR),
				     CPSR_ALLBITS_MASK);
	}
	if (!vcpu->reset_count) {
		arm_priv(vcpu)->features = 0;
		switch (cpuid) {
		case ARM_CPUID_ARM926:
			arm_set_feature(vcpu, ARM_FEATURE_V5);
			arm_set_feature(vcpu, ARM_FEATURE_VFP);
			arm_set_feature(vcpu, ARM_FEATURE_DUMMY_C15_REGS);
			arm_set_feature(vcpu, ARM_FEATURE_CACHE_TEST_CLEAN);
			break;
		case ARM_CPUID_ARM11MPCORE:
			arm_set_feature(vcpu, ARM_FEATURE_V6);
			arm_set_feature(vcpu, ARM_FEATURE_V6K);
			arm_set_feature(vcpu, ARM_FEATURE_VFP);
			arm_set_feature(vcpu, ARM_FEATURE_VAPA);
			arm_set_feature(vcpu, ARM_FEATURE_MPIDR);
			arm_set_feature(vcpu, ARM_FEATURE_DUMMY_C15_REGS);
			break;
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
	}

	rc = cpu_vcpu_vfp_init(vcpu);
	if (rc) {
		goto fail_vfp_init;
	}

	rc = cpu_vcpu_cp14_init(vcpu);
	if (rc) {
		goto fail_cp14_init;
	}

	rc = cpu_vcpu_cp15_init(vcpu, cpuid);
	if (rc) {
		goto fail_cp15_init;
	}

	return VMM_OK;

fail_cp15_init:
	if (!vcpu->reset_count) {
		cpu_vcpu_cp14_deinit(vcpu);
	}
fail_cp14_init:
	if (!vcpu->reset_count) {
		cpu_vcpu_vfp_deinit(vcpu);
	}
fail_vfp_init:
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

	/* Cleanup CP15 */
	if ((rc = cpu_vcpu_cp15_deinit(vcpu))) {
		return rc;
	}

	/* Cleanup CP14 */
	if ((rc = cpu_vcpu_cp14_deinit(vcpu))) {
		return rc;
	}

	/* Cleanup VFP */
	if ((rc = cpu_vcpu_vfp_deinit(vcpu))) {
		return rc;
	}

	/* Free super regs */
	vmm_free(vcpu->arch_priv);

	return VMM_OK;
}

void arch_vcpu_switch(struct vmm_vcpu *tvcpu,
		      struct vmm_vcpu *vcpu,
                      arch_regs_t *regs)
{
	u32 ite;
	/* Save user registers & banked registers */
	if (tvcpu) {
		arm_regs(tvcpu)->pc = regs->pc;
		arm_regs(tvcpu)->lr = regs->lr;
		arm_regs(tvcpu)->sp = regs->sp;
		for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
			arm_regs(tvcpu)->gpr[ite] = regs->gpr[ite];
		}
		arm_regs(tvcpu)->cpsr = regs->cpsr;
		arm_regs(tvcpu)->sp_excp = regs->sp_excp;
		if (tvcpu->is_normal) {
			cpu_vcpu_banked_regs_save(arm_priv(tvcpu), regs);
			/* Save VFP regs */
			cpu_vcpu_vfp_regs_save(tvcpu);
			/* Save CP14 regs */
			cpu_vcpu_cp14_regs_save(tvcpu);
			/* Save CP15 regs */
			cpu_vcpu_cp15_regs_save(tvcpu);
		}
	}
	/* Restore user registers & banked registers */
	regs->pc = arm_regs(vcpu)->pc;
	regs->lr = arm_regs(vcpu)->lr;
	regs->sp = arm_regs(vcpu)->sp;
	for (ite = 0; ite < CPU_GPR_COUNT; ite++) {
		regs->gpr[ite] = arm_regs(vcpu)->gpr[ite];
	}
	regs->cpsr = arm_regs(vcpu)->cpsr;
	regs->sp_excp = arm_regs(vcpu)->sp_excp;
	if (vcpu->is_normal) {
		/* Restore VFP regs */
		cpu_vcpu_vfp_regs_restore(vcpu);
		/* Restore CP14 regs */
		cpu_vcpu_cp14_regs_restore(vcpu);
		/* Restore CP15 regs */
		cpu_vcpu_cp15_regs_restore(vcpu);
		/* Restore banked registers */
		cpu_vcpu_banked_regs_restore(arm_priv(vcpu), regs);
	} else {
		/* Restore hypervisor TTBL for Orphan VCPUs */
		if (tvcpu) {
			if (tvcpu->is_normal) {
				cpu_mmu_change_ttbr(cpu_mmu_l1tbl_default());
			}
		} else {
			cpu_mmu_change_ttbr(cpu_mmu_l1tbl_default());
		}
	}
	/* Clear exclusive monitor */
	clrex();
}

void arch_vcpu_post_switch(struct vmm_vcpu *vcpu,
			   arch_regs_t *regs)
{
	/* Nothing to do here. */
}

void arch_vcpu_preempt_orphan(void)
{
	/* Trigger SVC call from supervisor mode. This will cause
	 * do_soft_irq() function to call vmm_scheduler_preempt_orphan()
	 */
	asm volatile ("svc #0\t\n");
}

static void __cpu_vcpu_dump_user_reg(struct vmm_chardev *cdev,
				     struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	u32 i;

	vmm_cprintf(cdev, "Core Registers\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "SP", regs->sp,
		    "LR", regs->lr,
		    "PC", regs->pc);
	vmm_cprintf(cdev, " %7s=0x%08x\n",
		    "CPSR", cpu_vcpu_cpsr_retrieve(vcpu, regs));
	vmm_cprintf(cdev, "General Purpose Registers");
	for (i = 0; i < CPU_GPR_COUNT; i++) {
		if (i % 3 == 0) {
			vmm_cprintf(cdev, "\n");
		}
		vmm_cprintf(cdev, " %5s%02d=0x%08x",
			    "R", i, regs->gpr[i]);
	}
	vmm_cprintf(cdev, "\n");
}

void cpu_vcpu_dump_user_reg(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	__cpu_vcpu_dump_user_reg(NULL, vcpu, regs);
}

void arch_vcpu_regs_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	u32 i;

	/* For both Normal & Orphan VCPUs */
	__cpu_vcpu_dump_user_reg(cdev, vcpu, arm_regs(vcpu));

	/* For only Normal VCPUs */
	if (!vcpu->is_normal) {
		return;
	}

	/* Print banked registers */
	vmm_cprintf(cdev, "User Mode Registers (Banked)\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x\n",
		    "SP", arm_priv(vcpu)->sp_usr,
		    "LR", arm_priv(vcpu)->lr_usr);
	vmm_cprintf(cdev, "Supervisor Mode Registers (Banked)\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "SP", arm_priv(vcpu)->sp_svc,
		    "LR", arm_priv(vcpu)->lr_svc,
		    "SPSR", arm_priv(vcpu)->spsr_svc);
	vmm_cprintf(cdev, "Monitor Mode Registers (Banked)\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "SP", arm_priv(vcpu)->sp_mon,
		    "LR", arm_priv(vcpu)->lr_mon,
		    "SPSR", arm_priv(vcpu)->spsr_mon);
	vmm_cprintf(cdev, "Abort Mode Registers (Banked)\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "SP", arm_priv(vcpu)->sp_abt,
		    "LR", arm_priv(vcpu)->lr_abt,
		    "SPSR", arm_priv(vcpu)->spsr_abt);
	vmm_cprintf(cdev, "Undefined Mode Registers (Banked)\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "SP", arm_priv(vcpu)->sp_und,
		    "LR", arm_priv(vcpu)->lr_und,
		    "SPSR", arm_priv(vcpu)->spsr_und);
	vmm_cprintf(cdev, "IRQ Mode Registers (Banked)\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x\n",
		    "SP", arm_priv(vcpu)->sp_irq,
		    "LR", arm_priv(vcpu)->lr_irq,
		    "SPSR", arm_priv(vcpu)->spsr_irq);
	vmm_cprintf(cdev, "FIQ Mode Registers (Banked)\n");
	vmm_cprintf(cdev, " %7s=0x%08x %7s=0x%08x %7s=0x%08x",
		    "SP", arm_priv(vcpu)->sp_fiq,
		    "LR", arm_priv(vcpu)->lr_fiq,
		    "SPSR", arm_priv(vcpu)->spsr_fiq);
	for (i = 0; i < 5; i++) {
		if (i % 3 == 0) {
			vmm_cprintf(cdev, "\n");
		}
		vmm_cprintf(cdev, " %5s%02d=0x%08x",
			   "R", (i + 8), arm_priv(vcpu)->gpr_fiq[i]);
	}
	vmm_cprintf(cdev, "\n");

	/* Print VFP registers */
	cpu_vcpu_vfp_regs_dump(cdev, vcpu);

	/* Print CP14 registers */
	cpu_vcpu_cp14_regs_dump(cdev, vcpu);

	/* Print CP15 registers */
	cpu_vcpu_cp15_regs_dump(cdev, vcpu);
}

void arch_vcpu_stat_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	/* For now no arch specific stats */
}
