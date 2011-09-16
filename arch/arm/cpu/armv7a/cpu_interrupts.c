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
 * @version 1.0
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling cpu interrupts
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>
#include <cpu_inline_asm.h>
#include <cpu_mmu.h>
#include <cpu_vcpu_emulate_arm.h>
#include <cpu_vcpu_emulate_thumb.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_helper.h>
#include <cpu_defines.h>

void do_undefined_instruction(vmm_user_regs_t * uregs)
{
	int rc = VMM_OK;
	vmm_vcpu_t * vcpu;

	if ((uregs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		vmm_panic("%s: unexpected exception\n", __func__);
	}

	vmm_scheduler_irq_enter(uregs, TRUE);

	vcpu = vmm_scheduler_current_vcpu();

	/* If vcpu priviledge is user then generate exception 
	 * and return without emulating instruction 
	 */
	if ((vcpu->sregs->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		vmm_vcpu_irq_assert(vcpu, CPU_UNDEF_INST_IRQ, 0x0);
	} else {
		if (uregs->cpsr & CPSR_THUMB_ENABLED) {
			rc = cpu_vcpu_emulate_thumb_inst(vcpu, uregs, FALSE);
		} else {
			rc = cpu_vcpu_emulate_arm_inst(vcpu, uregs, FALSE);
		}
	}

	if (rc) {
		vmm_printf("%s: error %d\n", __func__, rc);
	}

	vmm_scheduler_irq_exit(uregs);
}

void do_software_interrupt(vmm_user_regs_t * uregs)
{
	int rc = VMM_OK;
	vmm_vcpu_t * vcpu;

	if ((uregs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		vmm_panic("%s: unexpected exception\n", __func__);
	}

	vmm_scheduler_irq_enter(uregs, TRUE);

	vcpu = vmm_scheduler_current_vcpu();

	/* If vcpu priviledge is user then generate exception 
	 * and return without emulating instruction 
	 */
	if ((vcpu->sregs->cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		vmm_vcpu_irq_assert(vcpu, CPU_SOFT_IRQ, 0x0);
	} else {
		if (uregs->cpsr & CPSR_THUMB_ENABLED) {
			rc = cpu_vcpu_emulate_thumb_inst(vcpu, uregs, TRUE);
		} else {
			rc = cpu_vcpu_emulate_arm_inst(vcpu, uregs, TRUE);
		}
	}

	if (rc) {
		vmm_printf("%s: error %d\n", __func__, rc);
	}

	vmm_scheduler_irq_exit(uregs);
}

void do_prefetch_abort(vmm_user_regs_t * uregs)
{
	int rc = VMM_EFAIL;
	u32 ifsr, ifar, fs;
	vmm_vcpu_t * vcpu;

	if ((uregs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		vmm_panic("%s: unexpected exception\n", __func__);
	}

	vmm_scheduler_irq_enter(uregs, TRUE);

	ifsr = read_ifsr();
	ifar = read_ifar();
	vcpu = vmm_scheduler_current_vcpu();

	fs = (ifsr & IFSR_FS4_MASK) >> IFSR_FS4_SHIFT;
	fs = (fs << 4) | (ifsr & IFSR_FS_MASK);

	switch(fs) {
	case IFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_1:
	case IFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_2:
		break;
	case IFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_1:
	case IFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_2:
		break;
	case IFSR_FS_TRANS_FAULT_SECTION:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, uregs, ifar, 0, 0, 0);
		break;
	case IFSR_FS_TRANS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, uregs, ifar, 0, 1, 0);
		break;
	case IFSR_FS_ACCESS_FAULT_SECTION:
		rc = cpu_vcpu_cp15_access_fault(vcpu, uregs, ifar, 0, 0, 0);
		break;
	case IFSR_FS_ACCESS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_access_fault(vcpu, uregs, ifar, 0, 1, 0);
		break;
	case IFSR_FS_DOMAIN_FAULT_SECTION:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, uregs, ifar, 0, 0, 0);
		break;
	case IFSR_FS_DOMAIN_FAULT_PAGE:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, uregs, ifar, 0, 1, 0);
		break;
	case IFSR_FS_PERM_FAULT_SECTION:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, uregs, ifar, 0, 0, 0);
		break;
	case IFSR_FS_PERM_FAULT_PAGE:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, uregs, ifar, 0, 1, 0);
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

	if (rc) {
		vmm_printf("\n");
		vmm_printf("%s: error %d\n", __func__, rc);
		vmm_printf("%s: vcpu_id = %d, ifar = 0x%x, ifsr = 0x%x\n", 
				__func__, vcpu->id, ifar, ifsr);
	}

	vmm_scheduler_irq_exit(uregs);
}

void do_data_abort(vmm_user_regs_t * uregs)
{
	int rc = VMM_EFAIL;
	u32 dfsr, dfar, fs, wnr;
	vmm_vcpu_t * vcpu;

	if ((uregs->cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		vmm_panic("%s: unexpected exception\n", __func__);
	}

	vmm_scheduler_irq_enter(uregs, TRUE);

	dfsr = read_dfsr();
	dfar = read_dfar();
	vcpu = vmm_scheduler_current_vcpu();

	fs = (dfsr & DFSR_FS4_MASK) >> DFSR_FS4_SHIFT;
	fs = (fs << 4) | (dfsr & DFSR_FS_MASK);
	wnr = (dfsr & DFSR_WNR_MASK) >> DFSR_WNR_SHIFT;

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
		rc = cpu_vcpu_cp15_trans_fault(vcpu, uregs, dfar, wnr, 0, 1);
		break;
	case DFSR_FS_TRANS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, uregs, dfar, wnr, 1, 1);
		break;
	case DFSR_FS_ACCESS_FAULT_SECTION:
		rc = cpu_vcpu_cp15_access_fault(vcpu, uregs, dfar, wnr, 0, 1);
		break;
	case DFSR_FS_ACCESS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_access_fault(vcpu, uregs, dfar, wnr, 1, 1);
		break;
	case DFSR_FS_DOMAIN_FAULT_SECTION:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, uregs, dfar, wnr, 0, 1);
		break;
	case DFSR_FS_DOMAIN_FAULT_PAGE:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, uregs, dfar, wnr, 1, 1);
		break;
	case DFSR_FS_PERM_FAULT_SECTION:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, uregs, dfar, wnr, 0, 1);
		break;
	case DFSR_FS_PERM_FAULT_PAGE:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, uregs, dfar, wnr, 1, 1);
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

	if (rc) {
		vmm_printf("\n");
		vmm_printf("%s: error %d\n", __func__, rc);
		vmm_printf("%s: vcpu_id = %d, dfar = 0x%x, dfsr = 0x%x\n", 
				__func__, vcpu->id, dfar, dfsr);
	}

	vmm_scheduler_irq_exit(uregs);
}

void do_not_used(vmm_user_regs_t * uregs)
{
	vmm_panic("%s: unexpected exception\n", __func__);
}

void do_irq(vmm_user_regs_t * uregs)
{
	vmm_scheduler_irq_enter(uregs, FALSE);

	vmm_host_irq_exec(CPU_EXTERNAL_IRQ, uregs);

	vmm_scheduler_irq_exit(uregs);
}

void do_fiq(vmm_user_regs_t * uregs)
{
	vmm_scheduler_irq_enter(uregs, FALSE);

	vmm_host_irq_exec(CPU_EXTERNAL_FIQ, uregs);

	vmm_scheduler_irq_exit(uregs);
}

int vmm_cpu_irq_setup(void)
{
	int rc;
	extern u32 _start_vect[];
	u32 *vectors, *vectors_data;
	u32 vec;
	cpu_page_t vec_page;

#if defined(CONFIG_ARMV7A_HIGHVEC)
	/* Enable high vectors in SCTLR */
	write_sctlr(read_sctlr() | SCTLR_V_MASK);
	vectors = (u32 *) CPU_IRQ_HIGHVEC_BASE;
#else
	vectors = (u32 *) CPU_IRQ_LOWVEC_BASE;
#endif
	vectors_data = vectors + CPU_IRQ_NR;

	/* If vectors are at correct location then do nothing */
	if ((u32) _start_vect == (u32) vectors) {
		return VMM_OK;
	}

	/* If vectors are not mapped in virtual memory then map them. */
	vmm_memset(&vec_page, 0, sizeof(cpu_page_t));
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
		vec_page.imp = 0;
		vec_page.dom = TTBL_L1TBL_TTE_DOM_RESERVED;
		vec_page.ap = TTBL_AP_SRW_U;
		vec_page.xn = 0;
		vec_page.c = 0;
		vec_page.b = 0;
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

void vmm_cpu_irq_enable(void)
{
	__asm("cpsie i");
}

void vmm_cpu_irq_disable(void)
{
	__asm("cpsid i");
}

irq_flags_t vmm_cpu_irq_save(void)
{
	unsigned long retval;

	asm volatile (" mrs     %0, cpsr\n\t" " cpsid   i"	/* Syntax CPSID <iflags> {, #<p_mode>}
								 * Note: This instruction is supported 
								 * from ARM6 and above
								 */
		      :"=r" (retval)::"memory", "cc");

	return retval;
}

void vmm_cpu_irq_restore(irq_flags_t flags)
{
	asm volatile (" msr     cpsr_c, %0"::"r" (flags)
		      :"memory", "cc");
}

