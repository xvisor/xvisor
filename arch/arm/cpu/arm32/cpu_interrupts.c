/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file cpu_interrupts.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling cpu interrupts
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_host_ram.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>
#include <emulate_arm.h>
#include <emulate_thumb.h>
#include <cpu_inline_asm.h>
#include <cpu_mmu.h>
#include <cpu_vcpu_hypercall_arm.h>
#include <cpu_vcpu_hypercall_thumb.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_helper.h>
#include <cpu_defines.h>

void do_undef_inst(arch_regs_t *regs)
{
	int rc = VMM_OK;
	struct vmm_vcpu *vcpu;

	if ((regs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		vmm_panic("%s: unexpected exception\n", __func__);
	}

	vmm_scheduler_irq_enter(regs, TRUE);

	vcpu = vmm_scheduler_current_vcpu();

	/* If vcpu priviledge is user then generate exception 
	 * and return without emulating instruction 
	 */
	if ((arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
	} else {
		if (regs->cpsr & CPSR_THUMB_ENABLED) {
			rc = emulate_thumb_inst(vcpu, regs, 
						*((u32 *)regs->pc));
		} else {
			rc = emulate_arm_inst(vcpu, regs, 
						*((u32 *)regs->pc));
		}
	}

	if (rc) {
		vmm_printf("%s: error %d\n", __func__, rc);
	}

	vmm_scheduler_irq_exit(regs);
}

void do_soft_irq(arch_regs_t *regs)
{
	int rc = VMM_OK;
	struct vmm_vcpu * vcpu;

	if ((regs->cpsr & CPSR_MODE_MASK) == CPSR_MODE_SUPERVISOR) {
		regs->pc += 4;
		vmm_scheduler_preempt_orphan(regs);
		return;
	} else if ((regs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		vmm_panic("%s: unexpected exception\n", __func__);
	}

	vmm_scheduler_irq_enter(regs, TRUE);

	vcpu = vmm_scheduler_current_vcpu();

	/* If vcpu priviledge is user then generate exception 
	 * and return without emulating instruction 
	 */
	if ((arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		vmm_vcpu_irq_assert(vcpu, CPU_SOFT_IRQ, 0x0);
	} else {
		if (regs->cpsr & CPSR_THUMB_ENABLED) {
			rc = cpu_vcpu_hypercall_thumb(vcpu, regs, 
							*((u32 *)regs->pc));
		} else {
			rc = cpu_vcpu_hypercall_arm(vcpu, regs, 
							*((u32 *)regs->pc));
		}
	}

	if (rc) {
		vmm_printf("%s: error %d\n", __func__, rc);
	}

	vmm_scheduler_irq_exit(regs);
}

void do_prefetch_abort(arch_regs_t *regs)
{
	int rc = VMM_EFAIL;
	bool crash_dump = FALSE;
	u32 ifsr, ifar, fs;
	struct vmm_vcpu *vcpu;

	ifsr = read_ifsr();
	ifar = read_ifar();

	fs = (ifsr & IFSR_FS_MASK);
#if !defined(CONFIG_ARMV5)
	fs |= (ifsr & IFSR_FS4_MASK) >> (IFSR_FS4_SHIFT - 4);
#endif

	if ((regs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		struct cpu_l1tbl * l1;
		struct cpu_page pg;
		if (fs != IFSR_FS_TRANS_FAULT_SECTION &&
		    fs != IFSR_FS_TRANS_FAULT_PAGE) {
			vmm_panic("%s: unexpected prefetch abort\n"
				  "%s: pc = 0x%08x, ifsr = 0x%08x, ifar = 0x%08x\n", 
				  __func__, __func__, regs->pc, ifsr, ifar);
		}
		rc = cpu_mmu_get_reserved_page((virtual_addr_t)ifar, &pg);
		if (rc) {
			vmm_panic("%s: cannot find reserved page\n"
				  "%s: ifsr = 0x%08x, ifar = 0x%08x\n", 
				  __func__, __func__, ifsr, ifar);
		}
		l1 = cpu_mmu_l1tbl_current();
		if (!l1) {
			vmm_panic("%s: cannot find l1 table\n"
				  "%s: ifsr = 0x%08x, ifar = 0x%08x\n",
				  __func__, __func__, ifsr, ifar);
		}
		rc = cpu_mmu_map_page(l1, &pg);
		if (rc) {
			vmm_panic("%s: cannot map page in l1 table\n"
				  "%s: ifsr = 0x%08x, ifar = 0x%08x\n",
				  __func__, __func__, ifsr, ifar);
		}
		return;
	}

	vcpu = vmm_scheduler_current_vcpu();

	if ((regs->pc & ~(TTBL_L2TBL_SMALL_PAGE_SIZE - 1)) == 
	    arm_priv(vcpu)->cp15.ovect_base) {
		regs->pc = (virtual_addr_t)arm_guest_priv(vcpu->guest)->ovect 
			    + (regs->pc & (TTBL_L2TBL_SMALL_PAGE_SIZE - 1));
		return;
	}

	vmm_scheduler_irq_enter(regs, TRUE);

	switch(fs) {
	case IFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_1:
	case IFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_2:
		break;
	case IFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_1:
	case IFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_2:
		break;
	case IFSR_FS_TRANS_FAULT_SECTION:
	case IFSR_FS_TRANS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, 
						ifar, fs, 0, 0, 0, FALSE);
		crash_dump = TRUE;
		break;
	case IFSR_FS_ACCESS_FAULT_SECTION:
	case IFSR_FS_ACCESS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_access_fault(vcpu, regs, 
						ifar, fs, 0, 0, 0);
		crash_dump = TRUE;
		break;
	case IFSR_FS_DOMAIN_FAULT_SECTION:
	case IFSR_FS_DOMAIN_FAULT_PAGE:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, regs, 
						ifar, fs, 0, 0, 0);
		crash_dump = TRUE;
		break;
	case IFSR_FS_PERM_FAULT_SECTION:
	case IFSR_FS_PERM_FAULT_PAGE:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, regs, 
						ifar, fs, 0, 0, 0);
		crash_dump = TRUE;
		break;
	case IFSR_FS_DEBUG_EVENT:
	case IFSR_FS_SYNC_EXT_ABORT:
	case IFSR_FS_IMP_VALID_LOCKDOWN:
	case IFSR_FS_IMP_VALID_COPROC_ABORT:
	case IFSR_FS_MEM_ACCESS_SYNC_PARITY_ERROR:
		break;
	default:
		break; 
	};

	if (rc && crash_dump) {
		vmm_printf("\n");
		vmm_printf("%s: error %d\n", __func__, rc);
		vmm_printf("%s: vcpu_id = %d, ifar = 0x%x, ifsr = 0x%x\n", 
				__func__, vcpu->id, ifar, ifsr);
		cpu_vcpu_dump_user_reg(vcpu, regs);
	}

	vmm_scheduler_irq_exit(regs);
}

void do_data_abort(arch_regs_t *regs)
{
	int rc = VMM_EFAIL; 
	bool crash_dump = FALSE;
	u32 dfsr, dfar, fs, dom, wnr;
	struct vmm_vcpu *vcpu;
	struct cpu_l1tbl *l1;
	struct cpu_page pg;

	dfsr = read_dfsr();
	dfar = read_dfar();

	fs = (dfsr & DFSR_FS_MASK);
#if !defined(CONFIG_ARMV5)
	fs |= (dfsr & DFSR_FS4_MASK) >> (DFSR_FS4_SHIFT - 4);
#endif
	wnr = (dfsr & DFSR_WNR_MASK) >> DFSR_WNR_SHIFT;
	dom = (dfsr & DFSR_DOM_MASK) >> DFSR_DOM_SHIFT;

	if ((regs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		if (fs != DFSR_FS_TRANS_FAULT_SECTION &&
		    fs != DFSR_FS_TRANS_FAULT_PAGE) {
			vmm_panic("%s: unexpected data abort\n"
				  "%s: pc = 0x%08x, dfsr = 0x%08x, dfar = 0x%08x\n", 
				  __func__, __func__, regs->pc, dfsr, dfar);
		}
		rc = cpu_mmu_get_reserved_page(dfar, &pg);
		if (rc) {
			/* If we were in normal context then just handle
			 * trans fault for current normal VCPU and exit
			 * else there is nothing we can do so panic.
			 */
			if (vmm_scheduler_normal_context()) {
				vcpu = vmm_scheduler_current_vcpu();
				cpu_vcpu_cp15_trans_fault(vcpu, regs, 
						dfar, fs, dom, wnr, 1, FALSE);
				return;
			}
			vmm_panic("%s: cannot find reserved page\n"
				  "%s: dfsr = 0x%08x, dfar = 0x%08x\n", 
				  __func__, __func__, dfsr, dfar);
		}
		l1 = cpu_mmu_l1tbl_current();
		if (!l1) {
			vmm_panic("%s: cannot find l1 table\n"
				  "%s: dfsr = 0x%08x, dfar = 0x%08x\n",
				  __func__, __func__, dfsr, dfar);
		}
		rc = cpu_mmu_map_page(l1, &pg);
		if (rc) {
			vmm_panic("%s: cannot map page in l1 table\n"
				  "%s: dfsr = 0x%08x, dfar = 0x%08x\n",
				  __func__, __func__, dfsr, dfar);
		}
		return;
	}

	vcpu = vmm_scheduler_current_vcpu();

	vmm_scheduler_irq_enter(regs, TRUE);

	switch(fs) {
	case DFSR_FS_ALIGN_FAULT:
		break;
	case DFSR_FS_ICACHE_MAINT_FAULT:
		break;
	case DFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_1:
	case DFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_2:
		break;
	case DFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_1:
	case DFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_2:
		break;
	case DFSR_FS_TRANS_FAULT_SECTION:
	case DFSR_FS_TRANS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, 
						dfar, fs, dom, wnr, 1, FALSE);
		crash_dump = TRUE;
		break;
	case DFSR_FS_ACCESS_FAULT_SECTION:
	case DFSR_FS_ACCESS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_access_fault(vcpu, regs, 
						dfar, fs, dom, wnr, 1);
		crash_dump = TRUE;
		break;
	case DFSR_FS_DOMAIN_FAULT_SECTION:
	case DFSR_FS_DOMAIN_FAULT_PAGE:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, regs, 
						dfar, fs, dom, wnr, 1);
		crash_dump = TRUE;
		break;
	case DFSR_FS_PERM_FAULT_SECTION:
	case DFSR_FS_PERM_FAULT_PAGE:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, regs, 
						dfar, fs, dom, wnr, 1);
		if ((dfar & ~(TTBL_L2TBL_SMALL_PAGE_SIZE - 1)) != 
						arm_priv(vcpu)->cp15.ovect_base) {
			crash_dump = FALSE;
		}
		break;
	case DFSR_FS_DEBUG_EVENT:
	case DFSR_FS_SYNC_EXT_ABORT:
	case DFSR_FS_IMP_VALID_LOCKDOWN:
	case DFSR_FS_IMP_VALID_COPROC_ABORT:
	case DFSR_FS_MEM_ACCESS_SYNC_PARITY_ERROR:
	case DFSR_FS_ASYNC_EXT_ABORT:
	case DFSR_FS_MEM_ACCESS_ASYNC_PARITY_ERROR:
		break;
	default:
		break;
	};

	if (rc && crash_dump) {
		vmm_printf("\n");
		vmm_printf("%s: error %d\n", __func__, rc);
		vmm_printf("%s: vcpu_id = %d, dfar = 0x%x, dfsr = 0x%x\n", 
				__func__, vcpu->id, dfar, dfsr);
		cpu_vcpu_dump_user_reg(vcpu, regs);
	}

	vmm_scheduler_irq_exit(regs);
}

void do_not_used(arch_regs_t *regs)
{
	vmm_panic("%s: unexpected exception\n", __func__);
}

void do_irq(arch_regs_t *regs)
{
	vmm_scheduler_irq_enter(regs, FALSE);

	vmm_host_irq_exec(CPU_EXTERNAL_IRQ);

	vmm_scheduler_irq_exit(regs);
}

void do_fiq(arch_regs_t *regs)
{
	vmm_scheduler_irq_enter(regs, FALSE);

	vmm_host_irq_exec(CPU_EXTERNAL_FIQ);

	vmm_scheduler_irq_exit(regs);
}

int __cpuinit arch_cpu_irq_setup(void)
{
	static const struct cpu_page zero_filled_cpu_page = { 0 };

	int rc;
	extern u32 _start_vect[];
	u32 *vectors, *vectors_data;
	u32 vec, cpu = vmm_smp_processor_id();
	struct cpu_page vec_page;

#if defined(CONFIG_ARM32_HIGHVEC)
	/* Enable high vectors in SCTLR */
	write_sctlr(read_sctlr() | SCTLR_V_MASK);
	vectors = (u32 *) CPU_IRQ_HIGHVEC_BASE;
#else
#if defined(CONFIG_ARMV7A_SECUREX)
	write_vbar(CPU_IRQ_LOWVEC_BASE);
#endif
	vectors = (u32 *) CPU_IRQ_LOWVEC_BASE;
#endif
	vectors_data = vectors + CPU_IRQ_NR;

	/* For secondary CPUs nothing else to be done. */
	if (cpu) {
		return VMM_OK;
	}

	/* If vectors are at correct location then do nothing */
	if ((u32) _start_vect == (u32) vectors) {
		return VMM_OK;
	}

	/* If vectors are not mapped in virtual memory then map them. */
	vec_page = zero_filled_cpu_page;
	rc = cpu_mmu_get_reserved_page((virtual_addr_t)vectors, &vec_page);
	if (rc) {
		rc = vmm_host_ram_alloc(&vec_page.pa, 
					TTBL_L2TBL_SMALL_PAGE_SIZE, 
					TRUE);
		if (rc) {
			return rc;
		}

		vec_page.va = (virtual_addr_t)vectors;
		vec_page.sz = TTBL_L2TBL_SMALL_PAGE_SIZE;
		vec_page.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
		vec_page.ap = TTBL_AP_SRW_U;

		if ((rc = cpu_mmu_map_reserved_page(&vec_page))) {
			return rc;
		}
	}

	/*
	 * Loop through the vectors we're taking over, and copy the
	 * vector's insn and data word.
	 */
	for (vec = 0; vec < CPU_IRQ_NR; vec++) {
		vectors[vec] = _start_vect[vec];
		vectors_data[vec] = _start_vect[vec + CPU_IRQ_NR];
	}

	return VMM_OK;
}

