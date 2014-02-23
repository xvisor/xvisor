/**
 * Copyright (c) 2012 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling cpu interrupts
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <vmm_scheduler.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_excep.h>
#include <cpu_vcpu_emulate.h>
#include <cpu_vcpu_helper.h>

void do_undef_inst(arch_regs_t *regs)
{
	struct vmm_vcpu *vcpu = vmm_scheduler_current_vcpu();

	vmm_printf("%s: CPU%d unexpected exception\n",
		   __func__, vmm_smp_processor_id());
	vmm_printf("%s: Current VCPU=%s HSR=0x%08x\n",
		   __func__, (vcpu) ? vcpu->name : "(NULL)", 
		   read_hsr());
	vmm_printf("%s: HPFAR=0x%08x HIFAR=0x%08x HDFAR=0x%08x\n",
		   __func__, read_hpfar(), read_hifar(), read_hdfar());

	cpu_vcpu_dump_user_reg(regs);

	vmm_panic("%s: please reboot ...\n", __func__);
}

void do_soft_irq(arch_regs_t *regs)
{
	struct vmm_vcpu *vcpu;

	if ((regs->cpsr & CPSR_MODE_MASK) == CPSR_MODE_HYPERVISOR) {
		vmm_scheduler_preempt_orphan(regs);
		return;
	} else {
		vcpu = vmm_scheduler_current_vcpu();

		vmm_printf("%s: CPU%d unexpected exception\n",
			   __func__, vmm_smp_processor_id());
		vmm_printf("%s: Current VCPU=%s HSR=0x%08x\n",
			   __func__, (vcpu) ? vcpu->name : "(NULL)", 
			   read_hsr());
		vmm_printf("%s: HPFAR=0x%08x HIFAR=0x%08x HDFAR=0x%08x\n",
			   __func__, read_hpfar(), read_hifar(), read_hdfar());

		cpu_vcpu_dump_user_reg(regs);

		vmm_panic("%s: please reboot ...\n", __func__);
	}
}

void do_prefetch_abort(arch_regs_t *regs)
{
	struct vmm_vcpu *vcpu = vmm_scheduler_current_vcpu();

	vmm_printf("%s: CPU%d unexpected exception\n",
		   __func__, vmm_smp_processor_id());
	vmm_printf("%s: Current VCPU=%s HSR=0x%08x\n",
		   __func__, (vcpu) ? vcpu->name : "(NULL)", 
		   read_hsr());
	vmm_printf("%s: HPFAR=0x%08x HIFAR=0x%08x HDFAR=0x%08x\n",
		   __func__, read_hpfar(), read_hifar(), read_hdfar());

	cpu_vcpu_dump_user_reg(regs);

	vmm_panic("%s: please reboot ...\n", __func__);
}

void do_data_abort(arch_regs_t *regs)
{
	struct vmm_vcpu *vcpu = vmm_scheduler_current_vcpu();

	vmm_printf("%s: CPU%d unexpected exception\n",
		   __func__, vmm_smp_processor_id());
	vmm_printf("%s: Current VCPU=%s HSR=0x%08x\n",
		   __func__, (vcpu) ? vcpu->name : "(NULL)", 
		   read_hsr());
	vmm_printf("%s: HPFAR=0x%08x HIFAR=0x%08x HDFAR=0x%08x\n",
		   __func__, read_hpfar(), read_hifar(), read_hdfar());

	cpu_vcpu_dump_user_reg(regs);

	vmm_panic("%s: please reboot ...\n", __func__);
}

void do_hyp_trap(arch_regs_t *regs)
{
	int rc = VMM_OK;
	u32 hsr, ec, il, iss;
	virtual_addr_t far;
	physical_addr_t fipa = 0;
	struct vmm_vcpu *vcpu;

	hsr = read_hsr();
	ec = (hsr & HSR_EC_MASK) >> HSR_EC_SHIFT;
	il = (hsr & HSR_IL_MASK) >> HSR_IL_SHIFT;
	iss = (hsr & HSR_ISS_MASK) >> HSR_ISS_SHIFT;

	vcpu = vmm_scheduler_current_vcpu();

	/* We dont expect any faults from hypervisor code itself 
	 * so, any trap we get from hypervisor mode means something
	 * unexpected has occured.
	 */
	if ((regs->cpsr & CPSR_MODE_MASK) == CPSR_MODE_HYPERVISOR) {
		vmm_printf("%s: CPU%d unexpected exception\n", 
			   __func__, vmm_smp_processor_id());
		vmm_printf("%s: Current VCPU=%s HSR=0x%08x\n",
			   __func__, (vcpu) ? vcpu->name : "(NULL)", 
			   read_hsr());
		vmm_printf("%s: HPFAR=0x%08x HIFAR=0x%08x HDFAR=0x%08x\n",
			   __func__, read_hpfar(), read_hifar(), read_hdfar());
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
	case EC_TRAP_MCR_MRC_CP15:
		/* MCR/MRC CP15 emulation */
		rc = cpu_vcpu_emulate_mcr_mrc_cp15(vcpu, regs, il, iss);
		break;
	case EC_TRAP_MCRR_MRRC_CP15:
		/* MCRR/MRRC CP15 emulation */
		rc = cpu_vcpu_emulate_mcrr_mrrc_cp15(vcpu, regs, il, iss);
		break;
	case EC_TRAP_MCR_MRC_CP14:
		/* MCR/MRC CP14 emulation */
		rc = cpu_vcpu_emulate_mcr_mrc_cp14(vcpu, regs, il, iss);
		break;
	case EC_TRAP_LDC_STC_CP14:
		/* LDC/STC CP14 emulation */
		rc = cpu_vcpu_emulate_ldc_stc_cp14(vcpu, regs, il, iss);
		break;
	case EC_TRAP_CP0_TO_CP13:
		/* CP0 to CP13 emulation */
		rc = cpu_vcpu_emulate_cp0_cp13(vcpu, regs, il, iss);
		break;
	case EC_TRAP_VMRS:
		/* MRC (or VMRS) to CP10 for MVFR0, MVFR1 or FPSID */
		rc = cpu_vcpu_emulate_vmrs(vcpu, regs, il, iss);
		break;
	case EC_TRAP_JAZELLE:
		/* Jazelle emulation */
		rc = cpu_vcpu_emulate_jazelle(vcpu, regs, il, iss);
		break;
	case EC_TRAP_BXJ:
		/* BXJ emulation */
		rc = cpu_vcpu_emulate_bxj(vcpu, regs, il, iss);
		break;
	case EC_TRAP_MRRC_CP14:
		/* MRRC to CP14 emulation */
		rc = cpu_vcpu_emulate_mrrc_cp14(vcpu, regs, il, iss);
		break;
	case EC_TRAP_SVC:
		/* We dont expect to get this trap so error */
		rc = VMM_EFAIL;
		break;
	case EC_TRAP_HVC:
		/* Hypercall or HVC emulation */
		rc = cpu_vcpu_emulate_hvc(vcpu, regs, il, iss);
		break;
	case EC_TRAP_SMC:
		/* System Monitor Call or SMC emulation */
		rc = cpu_vcpu_emulate_smc(vcpu, regs, il, iss);
		break;
	case EC_TRAP_STAGE2_INST_ABORT:
		/* Stage2 instruction abort */
		far  = read_hifar();
		fipa = (read_hpfar() & HPFAR_FIPA_MASK) >> HPFAR_FIPA_SHIFT;
		fipa = fipa << HPFAR_FIPA_PAGE_SHIFT;
		fipa = fipa | (far & HPFAR_FIPA_PAGE_MASK);
		rc = cpu_vcpu_inst_abort(vcpu, regs, il, iss, far, fipa);
		break;
	case EC_TRAP_STAGE1_INST_ABORT:
		/* We dont expect to get this trap so error */
		rc = VMM_EFAIL;
		break;
	case EC_TRAP_STAGE2_DATA_ABORT:
		/* Stage2 data abort */
		far  = read_hdfar();
		fipa = (read_hpfar() & HPFAR_FIPA_MASK) >> HPFAR_FIPA_SHIFT;
		fipa = fipa << HPFAR_FIPA_PAGE_SHIFT;
		fipa = fipa | (far & HPFAR_FIPA_PAGE_MASK);
		rc = cpu_vcpu_data_abort(vcpu, regs, il, iss, far, fipa);
		break;
	case EC_TRAP_STAGE1_DATA_ABORT:
		/* We dont expect to get this trap so error */
		rc = VMM_EFAIL;
		break;
	default:
		/* Unknown EC value so error */
		rc = VMM_EFAIL;
		break;
	};

	if (rc) {
		vmm_printf("\n%s: ec=0x%x, il=0x%x, iss=0x%x,"
			   " fipa=0x%x, error=%d\n", __func__,
			   ec, il, iss, fipa, rc);
		if (vmm_manager_vcpu_get_state(vcpu) != VMM_VCPU_STATE_HALTED) {
			cpu_vcpu_halt(vcpu, regs);
		}
	}

	vmm_scheduler_irq_exit(regs);
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
	extern u32 _start_vect[];

	/* Update HVBAR to point to hypervisor vector table */
	write_hvbar((virtual_addr_t)&_start_vect);

	return VMM_OK;
}

