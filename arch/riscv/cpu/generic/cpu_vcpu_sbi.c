/**
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
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
 * @file cpu_vcpu_sbi.c
 * @author Atish Patra (atish.patra@wdc.com)
 * @brief source of SBI call handling
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_cpumask.h>
#include <vmm_vcpu_irq.h>
#include <vio/vmm_vserial.h>
#include <arch_vcpu.h>

#include <cpu_guest_serial.h>
#include <cpu_vcpu_sbi.h>
#include <cpu_vcpu_trap.h>
#include <cpu_vcpu_timer.h>
#include <cpu_vcpu_unpriv.h>
#include <cpu_sbi.h>
#include <cpu_tlb.h>
#include <riscv_encoding.h>
#include <riscv_sbi.h>

int cpu_vcpu_sbi_ecall(struct vmm_vcpu *vcpu, ulong cause,
		       arch_regs_t *regs)
{
	u8 send;
	u32 hcpu;
	int i, ret = 0;
	bool next_sepc = TRUE;
	struct vmm_vcpu *rvcpu;
	struct vmm_cpumask cm, hm;
	unsigned long hmask, ut_scause = 0;
	struct vmm_guest *guest = vcpu->guest;
	struct riscv_guest_serial *gs = riscv_guest_serial(guest);

	switch (regs->a7) {
	case SBI_EXT_0_1_SET_TIMER:
		if (riscv_priv(vcpu)->xlen == 32)
			riscv_timer_event_start(vcpu,
				((u64)regs->a1 << 32) | (u64)regs->a0);
		else
			riscv_timer_event_start(vcpu, (u64)regs->a0);
		break;
	case SBI_EXT_0_1_CONSOLE_PUTCHAR:
		send = (u8)regs->a0;
		vmm_vserial_receive(gs->vserial, &send, 1);
		break;
	case SBI_EXT_0_1_CONSOLE_GETCHAR:
		/* TODO: Implement get function if required */
		regs->a0 = SBI_ERR_NOT_SUPPORTED;
		break;
	case SBI_EXT_0_1_CLEAR_IPI:
		vmm_vcpu_irq_clear(vcpu, IRQ_VS_SOFT);
		break;
	case SBI_EXT_0_1_SEND_IPI:
		if (regs->a0)
			hmask = __cpu_vcpu_unpriv_read_ulong(regs->a0,
							     &ut_scause);
		else
			hmask = (1UL << guest->vcpu_count) - 1;
		if (ut_scause) {
			next_sepc = FALSE;
			ret = cpu_vcpu_redirect_trap(vcpu, regs,
						     ut_scause, regs->a0);
			break;
		}
		for_each_set_bit(i, &hmask, BITS_PER_LONG) {
			rvcpu = vmm_manager_guest_vcpu(guest, i);
			if (!(vmm_manager_vcpu_get_state(rvcpu) &
			      VMM_VCPU_STATE_INTERRUPTIBLE))
				continue;
			vmm_vcpu_irq_assert(rvcpu, IRQ_VS_SOFT, 0x0);
		}
		break;
	case SBI_EXT_0_1_SHUTDOWN:
		ret = vmm_manager_guest_shutdown_request(guest);
		if (ret)
			vmm_printf("%s: guest %s shutdown request failed "
				   "with error = %d\n", __func__,
				   guest->name, ret);
		break;
	case SBI_EXT_0_1_REMOTE_FENCE_I:
	case SBI_EXT_0_1_REMOTE_SFENCE_VMA:
	case SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID:
		if (regs->a0)
			hmask = __cpu_vcpu_unpriv_read_ulong(regs->a0,
							     &ut_scause);
		else
			hmask = (1UL << guest->vcpu_count) - 1;
		if (ut_scause) {
			next_sepc = FALSE;
			ret = cpu_vcpu_redirect_trap(vcpu, regs,
						     ut_scause, regs->a0);
			break;
		}
		vmm_cpumask_clear(&cm);
		for_each_set_bit(i, &hmask, BITS_PER_LONG) {
			rvcpu = vmm_manager_guest_vcpu(guest, i);
			if (!(vmm_manager_vcpu_get_state(rvcpu) &
			      VMM_VCPU_STATE_INTERRUPTIBLE))
				continue;
			if (vmm_manager_vcpu_get_hcpu(rvcpu, &hcpu))
				continue;
			vmm_cpumask_set_cpu(hcpu, &cm);
		}
		sbi_cpumask_to_hartmask(&cm, &hm);
		if (regs->a7 == SBI_EXT_0_1_REMOTE_FENCE_I) {
			sbi_remote_fence_i(vmm_cpumask_bits(&hm));
		} else if (regs->a7 == SBI_EXT_0_1_REMOTE_SFENCE_VMA) {
			sbi_remote_hfence_vvma(vmm_cpumask_bits(&hm),
					       regs->a1, regs->a2);
		} else if (regs->a7 == SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID) {
			sbi_remote_hfence_vvma_asid(vmm_cpumask_bits(&hm),
						    regs->a1, regs->a2,
						    regs->a3);
		}
		break;
	default:
		regs->a0 = SBI_ERR_NOT_SUPPORTED;
		break;
	};

	if (next_sepc)
		regs->sepc += 4;

	return ret;
}
