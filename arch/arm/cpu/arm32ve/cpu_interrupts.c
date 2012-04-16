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
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>
#include <cpu_inline_asm.h>
#include <cpu_mmu.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_helper.h>
#include <cpu_defines.h>

void do_undef_inst(arch_regs_t * uregs)
{
	vmm_printf("%s: unexpected exception\n", __func__);
	cpu_vcpu_dump_user_reg(uregs);
	vmm_panic("%s: please reboot ...\n", __func__);
}

void do_soft_irq(arch_regs_t * uregs)
{
	vmm_printf("%s: unexpected exception\n", __func__);
	cpu_vcpu_dump_user_reg(uregs);
	vmm_panic("%s: please reboot ...\n", __func__);
}

void do_prefetch_abort(arch_regs_t * uregs)
{
	vmm_printf("%s: unexpected exception\n", __func__);
	cpu_vcpu_dump_user_reg(uregs);
	vmm_panic("%s: please reboot ...\n", __func__);
}

void do_data_abort(arch_regs_t * uregs)
{
	vmm_printf("%s: unexpected exception\n", __func__);
	cpu_vcpu_dump_user_reg(uregs);
	vmm_panic("%s: please reboot ...\n", __func__);
}

void do_hyp_trap(arch_regs_t * uregs)
{
	int rc = VMM_EFAIL;
	u32 hsr, ec, il, iss;
	struct vmm_vcpu * vcpu;

	hsr = read_hsr();
	ec = (hsr & HSR_EC_MASK) >> HSR_EC_SHIFT;
	il = (hsr & HSR_IL_MASK) >> HSR_IL_SHIFT;
	iss = (hsr & HSR_ISS_MASK) >> HSR_ISS_SHIFT;

	if ((uregs->cpsr & CPSR_MODE_MASK) == CPSR_MODE_HYPERVISOR) {
		vmm_printf("%s: unexpected exception\n", __func__);
		vmm_printf("%s: ec=0x%x, il=0x%x, iss=0x%x\n", 
			   __func__, ec, il, iss);
		cpu_vcpu_dump_user_reg(uregs);
		vmm_panic("%s: please reboot ...\n", __func__);
	}

	vmm_scheduler_irq_enter(uregs, TRUE);

	vcpu = vmm_scheduler_current_vcpu();

	switch (ec) {
	case HSR_EC_UNKNOWN:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_WFI_WFE:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_MCR_MRC_CP15:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_MCRR_MRRC_CP15:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_MCR_MRC_CP14:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_LDC_STC_CP14:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_CP0_TO_CP13:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_VMRS:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_JAZELLE:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_BXJ:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_MRRC_CP14:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_SVC:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_HVC:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_SMC:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_STAGE2_INST_ABORT:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_STAGE1_INST_ABORT:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_STAGE2_DATA_ABORT:
		/* FIXME: */
		break;
	case HSR_EC_TRAP_STAGE1_DATA_ABORT:
		/* FIXME: */
		break;
	default:
		/* FIXME: */
		break;
	};

	if (rc) {
		vmm_printf("%s: ec=0x%x, il=0x%x, iss=0x%x, error=%d\n", 
			   __func__, ec, il, iss, rc);
		if (vcpu->state != VMM_VCPU_STATE_HALTED) {
			cpu_vcpu_halt(vcpu, uregs);
		}
	}

	vmm_scheduler_irq_exit(uregs);
}

void do_irq(arch_regs_t * uregs)
{
	vmm_scheduler_irq_enter(uregs, FALSE);

	vmm_host_irq_exec(CPU_EXTERNAL_IRQ, uregs);

	vmm_scheduler_irq_exit(uregs);
}

void do_fiq(arch_regs_t * uregs)
{
	vmm_scheduler_irq_enter(uregs, FALSE);

	vmm_host_irq_exec(CPU_EXTERNAL_FIQ, uregs);

	vmm_scheduler_irq_exit(uregs);
}

int __init arch_cpu_irq_setup(void)
{
	extern u32 _start_vect[];

	/* Update HVBAR to point to hypervisor vector table */
	write_hvbar((virtual_addr_t)&_start_vect);

	return VMM_OK;
}

void arch_cpu_irq_enable(void)
{
	__asm("cpsie i");
}

void arch_cpu_irq_disable(void)
{
	__asm("cpsid i");
}

irq_flags_t arch_cpu_irq_save(void)
{
	unsigned long retval;

	asm volatile (" mrs     %0, cpsr\n\t" " cpsid   i"	/* Syntax CPSID <iflags> {, #<p_mode>}
								 * Note: This instruction is supported 
								 * from ARM6 and above
								 */
		      :"=r" (retval)::"memory", "cc");

	return retval;
}

void arch_cpu_irq_restore(irq_flags_t flags)
{
	asm volatile (" msr     cpsr_c, %0"::"r" (flags)
		      :"memory", "cc");
}

void arch_cpu_wait_for_irq(void)
{
	/* We could also use soft delay here. */
	asm volatile (" wfi ");
}
