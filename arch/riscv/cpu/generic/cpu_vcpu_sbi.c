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
#include <arch_vcpu.h>
#include <cpu_guest_serial.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_timer.h>
#include <cpu_vcpu_sbi.h>
#include <cpu_tlb.h>
#include <vmm_timer.h>
#include <vmm_vcpu_irq.h>
#include <vio/vmm_vserial.h>

#include <riscv_encoding.h>
#include <riscv_sbi.h>
#include <riscv_unpriv.h>

u16 cpu_vcpu_sbi_version_major(void)
{
	return SBI_VERSION_MAJOR;
}

u16 cpu_vcpu_sbi_version_minor(void)
{
	return SBI_VERSION_MINOR;
}

int cpu_vcpu_sbi_ecall(struct vmm_vcpu *vcpu, ulong mcause,
		       arch_regs_t *regs)
{
	struct riscv_timer_event *tevent;
	struct riscv_guest_serial *gs = riscv_guest_serial(vcpu->guest);
	int ret = 0;
	u8 send;
	u64 next_cycle, delta_ns;
	int vcpuid;
	struct vmm_vcpu *remote_vcpu;
	ulong dhart_mask;

	switch (regs->a7) {
	case SBI_SET_TIMER:
		tevent = riscv_timer_priv(vcpu);
#if __riscv_xlen == 32
		next_cycle = ((u64)regs->a1 << 32) | (u64)regs->a0;
#else
		next_cycle = (u64)regs->a0;
#endif
		delta_ns = vmm_timer_delta_cycles_to_ns(next_cycle);

		/* In RISC-V, we should clear the timer pending bit before
		 * programming next one.
		 */
		vmm_vcpu_irq_clear(vcpu, IRQ_S_TIMER);
		/* No point in programming a timer for 1us */
		if (delta_ns <= TIMER_EVENT_THRESHOLD_NS)
			vmm_vcpu_irq_assert(vcpu, IRQ_S_TIMER, 0x0);
		else
			vmm_timer_event_start(&tevent->time_ev, delta_ns);
		break;
	case SBI_CONSOLE_PUTCHAR:
		send = (u8) regs->a0;
		vmm_vserial_receive(gs->vserial, &send, 1);
		break;
	case SBI_CONSOLE_GETCHAR:
		/* TODO: Implement get function if required */
		regs->a0 = VMM_ENOTSUPP;
		break;
	case SBI_CLEAR_IPI:
		vmm_vcpu_irq_clear(vcpu, IRQ_S_SOFT);
		break;
	case SBI_SEND_IPI:
		dhart_mask = load_ulong(&regs->a0);
		for_each_set_bit(vcpuid, &dhart_mask, BITS_PER_LONG) {
			remote_vcpu = vmm_manager_guest_vcpu(vcpu->guest,
							     vcpuid);
			vmm_vcpu_irq_assert(remote_vcpu, IRQ_S_SOFT, 0x0);
		}
		break;
	case SBI_SHUTDOWN:
		ret = vmm_manager_guest_shutdown_request(vcpu->guest);
		if (ret)
			vmm_printf("%s: guest %s shutdown request failed "
				   "with error = %d\n", __func__,
				   vcpu->guest->name, ret);
		break;
	case SBI_REMOTE_FENCE_I:
		__asm__ __volatile("fence.i");
		break;
	case SBI_REMOTE_SFENCE_VMA:
		/*TODO: Parse vma range */
		__hfence_bvma_all();
		break;
	case SBI_REMOTE_SFENCE_VMA_ASID:
		/*TODO: Parse vma range for given ASID */
		__hfence_bvma_asid(regs->a3);
		break;
	default:
		regs->a0 = VMM_ENOTSUPP;
		break;
	};

	if (!ret)
		regs->sepc += 4;

	return ret;
}
