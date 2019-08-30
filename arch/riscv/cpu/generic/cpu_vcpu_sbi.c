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
#include <vmm_vcpu_irq.h>
#include <vio/vmm_vserial.h>
#include <arch_vcpu.h>

#include <cpu_guest_serial.h>
#include <cpu_vcpu_sbi.h>
#include <cpu_vcpu_trap.h>
#include <cpu_vcpu_timer.h>
#include <cpu_vcpu_unpriv.h>
#include <cpu_tlb.h>
#include <riscv_encoding.h>
#include <riscv_sbi.h>

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
	u8 send;
	int i, ret = 0;
	struct vmm_vcpu *rvcpu;
	unsigned long hmask, ut_scause, ut_stval;
	struct riscv_guest_serial *gs = riscv_guest_serial(vcpu->guest);

	switch (regs->a7) {
	case SBI_SET_TIMER:
		if (riscv_priv(vcpu)->xlen == 32)
			riscv_timer_event_start(vcpu,
				((u64)regs->a1 << 32) | (u64)regs->a0);
		else
			riscv_timer_event_start(vcpu, (u64)regs->a0);
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
		ut_scause = ut_stval = 0;
		hmask = __cpu_vcpu_unpriv_read_ulong(regs->a0, &ut_scause);
		if (ut_scause) {
			return cpu_vcpu_redirect_trap(vcpu, regs,
						      ut_scause, regs->a0);
		} else {
			for_each_set_bit(i, &hmask, BITS_PER_LONG) {
				rvcpu = vmm_manager_guest_vcpu(vcpu->guest, i);
				vmm_vcpu_irq_assert(rvcpu, IRQ_S_SOFT, 0x0);
			}
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
		sbi_remote_fence_i(NULL);
		break;

	/*TODO:There should be a way to call remote hfence.bvma.
	 * Prefered method is now a SBI call. Until then, just flush
	 * all tlbs.
	 */
	case SBI_REMOTE_SFENCE_VMA:
		/*TODO: Parse vma range.*/
		sbi_remote_sfence_vma(NULL, 0, 0);
		break;
	case SBI_REMOTE_SFENCE_VMA_ASID:
		/*TODO: Parse vma range for given ASID */
		sbi_remote_sfence_vma(NULL, 0, 0);
		break;
	default:
		regs->a0 = VMM_ENOTSUPP;
		break;
	};

	if (!ret)
		regs->sepc += 4;

	return ret;
}
