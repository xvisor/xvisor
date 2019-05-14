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

#include <riscv_sbi.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <arch_vcpu.h>
#include <cpu_guest_serial.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_timer.h>
#include <cpu_vcpu_sbi.h>
#include <vmm_timer.h>
#include <vmm_vcpu_irq.h>
#include <vio/vmm_vserial.h>
#include <drv/irqchip/riscv-intc.h>

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
		vmm_vcpu_irq_clear(vcpu, RISCV_IRQ_SUPERVISOR_TIMER);
		/* No point in programming a timer for 1us */
		if (delta_ns <= TIMER_EVENT_THRESHOLD_NS)
			vmm_vcpu_irq_assert(vcpu,
					    RISCV_IRQ_SUPERVISOR_TIMER, 0x0);
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
		regs->a0 = VMM_ENOTSUPP;
		break;
	case SBI_SEND_IPI:
		regs->a0 = VMM_ENOTSUPP;
		break;
	case SBI_SHUTDOWN:
		regs->a0 = VMM_ENOTSUPP;
		break;
	case SBI_REMOTE_FENCE_I:
		__asm__ __volatile("fence.i");
		break;
	case SBI_REMOTE_SFENCE_VMA:
		 __asm__ __volatile("sfence.vma");
		break;
	case SBI_REMOTE_SFENCE_VMA_ASID:
		 __asm__ __volatile("sfence.vma");
		break;
	default:
		regs->a0 = VMM_ENOTSUPP;
		break;
	};

	if (!ret)
		regs->sepc += 4;

	return ret;
}
