/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source code for handling cpu interrupts
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <vmm_scheduler.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_spr.h>
#include <cpu_vcpu_emulate.h>
#include <cpu_vcpu_helper.h>
#include <cpu_defines.h>

void do_bad_mode(arch_regs_t *regs, unsigned long mode)
{
	u32 ec, il, iss;
	u64 esr, far, elr;
	struct vmm_vcpu *vcpu;

	esr = mrs(esr_el2);
	far = mrs(far_el2);
	elr = mrs(elr_el2);

	ec = (esr & ESR_EC_MASK) >> ESR_EC_SHIFT;
	il = (esr & ESR_IL_MASK) >> ESR_IL_SHIFT;
	iss = (esr & ESR_ISS_MASK) >> ESR_ISS_SHIFT;

	vcpu = vmm_scheduler_current_vcpu();

	vmm_printf("%s: CPU%d VCPU=%s unexpected exception\n",
		   __func__, vmm_smp_processor_id(),
		   (vcpu) ? vcpu->name : "(NULL)");
	vmm_printf("%s: ESR=0x%016lx EC=0x%x IL=0x%x ISS=0x%x\n",
		   __func__, esr, ec, il, iss);
	vmm_printf("%s: ELR=0x%016lx FAR=0x%016lx HPFAR=0x%016lx\n",
		   __func__, elr, far, mrs(hpfar_el2));
	cpu_vcpu_dump_user_reg(regs);
	vmm_panic("%s: please reboot ...\n", __func__);
}

void do_sync(arch_regs_t *regs, unsigned long mode)
{
	int rc = VMM_OK;
	u32 ec, il, iss;
	u64 esr, far, elr;
	physical_addr_t fipa = 0;
	struct vmm_vcpu *vcpu;

	esr = mrs(esr_el2);
	far = mrs(far_el2);
	elr = mrs(elr_el2);

	ec = (esr & ESR_EC_MASK) >> ESR_EC_SHIFT;
	il = (esr & ESR_IL_MASK) >> ESR_IL_SHIFT;
	iss = (esr & ESR_ISS_MASK) >> ESR_ISS_SHIFT;

	vcpu = vmm_scheduler_current_vcpu();

	/* We dont expect any faults from hypervisor code itself 
	 * so, any trap we get from hypervisor mode means something
	 * unexpected has occured.
	 */
	if ((regs->pstate & PSR_EL_MASK) == PSR_EL_2) {
		if ((ec == EC_TRAP_HVC_A64) && (iss == 0)) {
			vmm_scheduler_preempt_orphan(regs);
			return;
		}
		vmm_printf("%s: CPU%d VCPU=%s unexpected exception\n",
			   __func__, vmm_smp_processor_id(),
			   (vcpu) ? vcpu->name : "(NULL)");
		vmm_printf("%s: ESR=0x%016lx EC=0x%x IL=0x%x ISS=0x%x\n",
			   __func__, esr, ec, il, iss);
		vmm_printf("%s: ELR=0x%016lx FAR=0x%016lx HPFAR=0x%016lx\n",
			   __func__, elr, far, mrs(hpfar_el2));
		cpu_vcpu_dump_user_reg(regs);
		vmm_panic("%s: please reboot ...\n", __func__);
	}

	vmm_scheduler_irq_enter(regs, TRUE);

	switch (ec) {
	case EC_UNKNOWN:
		/* We dont expect to get this trap so error */
		rc = VMM_EFAIL;
		break;
	case EC_TRAP_WFI_WFE:
		/* WFI emulation */
		rc = cpu_vcpu_emulate_wfi_wfe(vcpu, regs, il, iss);
		break;
	case EC_TRAP_MCR_MRC_CP15_A32:
		/* MCR/MRC CP15 emulation */
		rc = cpu_vcpu_emulate_mcr_mrc_cp15(vcpu, regs, il, iss);
		break;
	case EC_TRAP_MCRR_MRRC_CP15_A32:
		/* MCRR/MRRC CP15 emulation */
		rc = cpu_vcpu_emulate_mcrr_mrrc_cp15(vcpu, regs, il, iss);
		break;
	case EC_TRAP_MCR_MRC_CP14_A32:
		/* MCR/MRC CP14 emulation */
		rc = cpu_vcpu_emulate_mcr_mrc_cp14(vcpu, regs, il, iss);
		break;
	case EC_TRAP_LDC_STC_CP14_A32:
		/* LDC/STC CP14 emulation */
		rc = cpu_vcpu_emulate_ldc_stc_cp14(vcpu, regs, il, iss);
		break;
	case EC_SIMD_FPU:
		/* Advanced SIMD and FPU emulation */
		rc = cpu_vcpu_emulate_simd_fp_regs(vcpu, regs, il, iss);
		break;
	case EC_FPEXC_A32:
	case EC_FPEXC_A64:
		/* We dont expect any FP execution faults */
		rc = VMM_EFAIL;
		break;
	case EC_TRAP_MRC_VMRS_CP10_A32:
		/* MRC (or VMRS) to CP10 for MVFR0, MVFR1 or FPSID */
		rc = cpu_vcpu_emulate_vmrs(vcpu, regs, il, iss);
		break;
	case EC_TRAP_MCRR_MRRC_CP14_A32:
		/* MRRC to CP14 emulation */
		rc = cpu_vcpu_emulate_mcrr_mrrc_cp14(vcpu, regs, il, iss);
		break;
	case EC_TRAP_SVC_A32:
	case EC_TRAP_SVC_A64:
	case EC_TRAP_SMC_A32:
	case EC_TRAP_SMC_A64:
		/* We dont expect to get these traps so error */
		rc = VMM_EFAIL;
		break;
	case EC_TRAP_HVC_A32:
		/* Hypercall or HVC emulation for A32 guest */
		rc = cpu_vcpu_emulate_hvc32(vcpu, regs, il, iss);
		break;
	case EC_TRAP_HVC_A64:
		/* Hypercall or HVC emulation for A64 guest */
		rc = cpu_vcpu_emulate_hvc64(vcpu, regs, il, iss);
		break;
	case EC_TRAP_MSR_MRS_SYSTEM:
		/* MSR/MRS/SystemRegs emulation */
		rc = cpu_vcpu_emulate_msr_mrs_system(vcpu, regs, il, iss);
		break;
	case EC_TRAP_LWREL_INST_ABORT:
		/* Stage2 instruction abort */
		fipa = (mrs(hpfar_el2) & HPFAR_FIPA_MASK) >> HPFAR_FIPA_SHIFT;
		fipa = fipa << HPFAR_FIPA_PAGE_SHIFT;
		fipa = fipa | (mrs(far_el2) & HPFAR_FIPA_PAGE_MASK);
		rc = cpu_vcpu_inst_abort(vcpu, regs, il, iss, fipa);
		break;
	case EC_TRAP_LWREL_DATA_ABORT:
		/* Stage2 data abort */
		fipa = (mrs(hpfar_el2) & HPFAR_FIPA_MASK) >> HPFAR_FIPA_SHIFT;
		fipa = fipa << HPFAR_FIPA_PAGE_SHIFT;
		fipa = fipa | (mrs(far_el2) & HPFAR_FIPA_PAGE_MASK);
		rc = cpu_vcpu_data_abort(vcpu, regs, il, iss, fipa);
		break;
	case EC_CUREL_INST_ABORT:
	case EC_CUREL_DATA_ABORT:
	case EC_SERROR:
		/* We dont expect to get aborts from EL2 so error */
		rc = VMM_EFAIL;
		break;
	case EC_PC_UNALIGNED:
	case EC_SP_UNALIGNED:
		/* We dont expect to get alignment faults from EL2 */
		rc = VMM_EFAIL;
		break;
	default:
		/* Unhandled or unknown EC value so error */
		rc = VMM_EFAIL;
		break;
	};

	if (rc) {
		vmm_printf("%s: CPU%d VCPU=%s sync failed (error %d)\n", 
			   __func__, vmm_smp_processor_id(), 
			   (vcpu) ? vcpu->name : "(NULL)", rc);
		vmm_printf("%s: ESR=0x%016lx EC=0x%x IL=0x%x ISS=0x%x\n",
			   __func__, esr, ec, il, iss);
		vmm_printf("%s: ELR=0x%016lx FAR=0x%016lx HPFAR=0x%016lx\n",
			   __func__, elr, far, mrs(hpfar_el2));
		if (vcpu->state != VMM_VCPU_STATE_HALTED) {
			cpu_vcpu_halt(vcpu, regs);
		}
	}

	vmm_scheduler_irq_exit(regs);
}

void do_irq(arch_regs_t *regs)
{
	vmm_scheduler_irq_enter(regs, FALSE);

	vmm_host_irq_exec(EXC_HYP_IRQ_SPx);

	vmm_scheduler_irq_exit(regs);
}

void do_hyp_fiq(arch_regs_t *regs)
{
	vmm_scheduler_irq_enter(regs, FALSE);

	vmm_host_irq_exec(CPU_EXTERNAL_FIQ);

	vmm_scheduler_irq_exit(regs);
}

int __cpuinit arch_cpu_irq_setup(void)
{
	extern u32 vectors[];

	/* Update VBAR_EL2 to point to hypervisor vector table */
	msr_sync(vbar_el2, (virtual_addr_t)&vectors);

	return VMM_OK;
}

