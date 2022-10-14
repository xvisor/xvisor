/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file cpu_exception.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for handling cpu exceptions
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_smp.h>
#include <vmm_host_irq.h>
#include <vmm_scheduler.h>
#include <arch_vcpu.h>
#include <cpu_hwcap.h>
#include <cpu_vcpu_trap.h>
#include <cpu_vcpu_nested.h>
#include <cpu_vcpu_sbi.h>
#include <cpu_vcpu_helper.h>

#include <riscv_csr.h>

static virtual_addr_t preempt_orphan_pc =
			(virtual_addr_t)&arch_vcpu_preempt_orphan;

void do_error(struct vmm_vcpu *vcpu, arch_regs_t *regs,
	      unsigned long scause, const char *msg, int err, bool panic)
{
	u32 cpu = vmm_smp_processor_id();

	vmm_printf("%s: CPU%d: VCPU=%s %s (error %d)\n",
		   __func__, cpu, (vcpu) ? vcpu->name : "(NULL)", msg, err);
	cpu_vcpu_dump_general_regs(NULL, regs);
	cpu_vcpu_dump_exception_regs(NULL, scause, csr_read(CSR_STVAL),
				     csr_read(CSR_HTVAL), csr_read(CSR_HTINST));
	if (panic) {
		vmm_panic("%s: please reboot ...\n", __func__);
	}
}

void do_handle_irq(arch_regs_t *regs, unsigned long cause)
{
	int rc = VMM_OK;

	vmm_scheduler_irq_enter(regs, FALSE);

	if (cause == IRQ_VS_SOFT ||
	    cause == IRQ_VS_TIMER ||
	    cause == IRQ_VS_EXT) {
		rc = cpu_vcpu_redirect_vsirq(vmm_scheduler_current_vcpu(),
					     regs, cause);
		goto done;
	}

	/* NOTE: Only exec <= 0xFFFFFFFFUL will be handled */
	if (cause <= 0xFFFFFFFFUL) {
		rc = vmm_host_active_irq_exec(cause);
	} else {
		rc = VMM_EINVALID;
	}

done:
	if (rc) {
		do_error(vmm_scheduler_current_vcpu(), regs,
			 cause | SCAUSE_INTERRUPT_MASK,
			 "interrupt handling failed", rc, TRUE);
	}

	vmm_scheduler_irq_exit(regs);
}

void do_handle_trap(arch_regs_t *regs, unsigned long cause)
{
	int rc = VMM_OK;
	bool panic = TRUE;
	const char *msg = "trap handling failed";
	struct cpu_vcpu_trap trap;
	struct vmm_vcpu *vcpu;

	if ((cause == CAUSE_STORE_PAGE_FAULT) &&
	    !(regs->hstatus & HSTATUS_SPV) &&
	    (regs->sepc == preempt_orphan_pc)) {
		regs->sepc += 4;
		vmm_scheduler_preempt_orphan(regs);
		return;
	}

	vmm_scheduler_irq_enter(regs, TRUE);

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->is_normal) {
		rc = VMM_EFAIL;
		msg = "unexpected trap";
		goto done;
	}

	switch (cause) {
	case CAUSE_MISALIGNED_FETCH:
	case CAUSE_FETCH_ACCESS:
	case CAUSE_ILLEGAL_INSTRUCTION:
	case CAUSE_BREAKPOINT:
	case CAUSE_MISALIGNED_LOAD:
	case CAUSE_LOAD_ACCESS:
	case CAUSE_MISALIGNED_STORE:
	case CAUSE_STORE_ACCESS:
	case CAUSE_USER_ECALL:
	case CAUSE_FETCH_PAGE_FAULT:
	case CAUSE_LOAD_PAGE_FAULT:
	case CAUSE_STORE_PAGE_FAULT:
		msg = "general fault failed";
		if (regs->hstatus & HSTATUS_SPV) {
			trap.sepc = regs->sepc;
			trap.scause = cause;
			trap.stval = csr_read(CSR_STVAL);
			trap.htval = csr_read(CSR_HTVAL);
			trap.htinst = csr_read(CSR_HTINST);
			rc = cpu_vcpu_general_fault(vcpu, regs, &trap);
			panic = FALSE;
		} else {
			rc = VMM_EINVALID;
		}
		break;
	case CAUSE_FETCH_GUEST_PAGE_FAULT:
	case CAUSE_LOAD_GUEST_PAGE_FAULT:
	case CAUSE_STORE_GUEST_PAGE_FAULT:
		msg = "page fault failed";
		if (regs->hstatus & HSTATUS_SPV) {
			trap.sepc = regs->sepc;
			trap.scause = cause;
			trap.stval = csr_read(CSR_STVAL);
			trap.htval = csr_read(CSR_HTVAL);
			trap.htinst = csr_read(CSR_HTINST);
			rc = cpu_vcpu_page_fault(vcpu, regs, &trap);
			panic = FALSE;
		} else {
			rc = VMM_EINVALID;
		}
		break;
	case CAUSE_VIRTUAL_INST_FAULT:
		msg = "virtual instruction fault failed";
		if (regs->hstatus & HSTATUS_SPV) {
			rc = cpu_vcpu_virtual_insn_fault(vcpu, regs,
							 csr_read(CSR_STVAL));
			panic = FALSE;
		} else {
			rc = VMM_EINVALID;
		}
		break;
	case CAUSE_VIRTUAL_SUPERVISOR_ECALL:
		msg = "ecall failed";
		if (regs->hstatus & HSTATUS_SPV) {
			rc = cpu_vcpu_sbi_ecall(vcpu, cause, regs);
			panic = FALSE;
		} else {
			rc = VMM_EINVALID;
		}
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	if (rc) {
		vmm_manager_vcpu_halt(vcpu);
	} else {
		riscv_stats_priv(vcpu)->trap[cause]++;
	}

done:
	if (rc) {
		do_error(vcpu, regs, cause, msg, rc, panic);
	}

	vmm_scheduler_irq_exit(regs);
}

void do_handle_exception(arch_regs_t *regs)
{
	unsigned long scause = csr_read(CSR_SCAUSE);

	if (scause & SCAUSE_INTERRUPT_MASK) {
		do_handle_irq(regs, scause & ~SCAUSE_INTERRUPT_MASK);
	} else {
		do_handle_trap(regs, scause & ~SCAUSE_INTERRUPT_MASK);
	}

	cpu_vcpu_nested_take_vsirq(vmm_scheduler_current_vcpu(), regs);
}

int __cpuinit arch_cpu_irq_setup(void)
{
	extern unsigned long _handle_exception[];
	extern unsigned long _handle_hyp_exception[];

	if (riscv_isa_extension_available(NULL, h)) {
		/* Update HEDELEG */
		csr_write(CSR_HEDELEG, HEDELEG_DEFAULT);

		/* Update HCOUNTEREN */
		csr_write(CSR_HCOUNTEREN, HCOUNTEREN_DEFAULT);

		/* Setup final exception handler with hypervisor enabled */
		csr_write(CSR_STVEC, (virtual_addr_t)&_handle_hyp_exception);
	} else {
		/* Setup final exception handler */
		csr_write(CSR_STVEC, (virtual_addr_t)&_handle_exception);
	}

	return VMM_OK;
}
