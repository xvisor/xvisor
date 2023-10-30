/**
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
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
 * @file cpu_vcpu_sbi_replace.c
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief source of SBI v0.2 replacement extensions
 */

#include <vmm_error.h>
#include <vmm_guest_aspace.h>
#include <vmm_macros.h>
#include <vmm_manager.h>
#include <vmm_stdio.h>
#include <vmm_vcpu_irq.h>
#include <vio/vmm_vserial.h>
#include <cpu_guest_serial.h>
#include <cpu_sbi.h>
#include <cpu_vcpu_sbi.h>
#include <cpu_vcpu_timer.h>
#include <cpu_vcpu_nested.h>
#include <generic_mmu.h>
#include <riscv_sbi.h>

static int vcpu_sbi_time_ecall(struct vmm_vcpu *vcpu, unsigned long ext_id,
			       unsigned long func_id, unsigned long *args,
			       struct cpu_vcpu_sbi_return *out)
{
	if (func_id != SBI_EXT_TIME_SET_TIMER)
		return SBI_ERR_NOT_SUPPORTED;

	if (riscv_priv(vcpu)->xlen == 32)
		cpu_vcpu_timer_start(vcpu,
				((u64)args[1] << 32) | (u64)args[0]);
	else
		cpu_vcpu_timer_start(vcpu, (u64)args[0]);

	return 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_time = {
	.name = "time",
	.extid_start = SBI_EXT_TIME,
	.extid_end = SBI_EXT_TIME,
	.handle = vcpu_sbi_time_ecall,
};

static int vcpu_sbi_rfence_ecall(struct vmm_vcpu *vcpu, unsigned long ext_id,
				 unsigned long func_id, unsigned long *args,
				 struct cpu_vcpu_sbi_return *out)
{
	u32 hcpu;
	struct vmm_vcpu *rvcpu;
	struct vmm_cpumask cm, hm;
	struct vmm_guest *guest = vcpu->guest;
	unsigned long hgatp, hmask = args[0], hbase = args[1];
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	vmm_cpumask_clear(&cm);
	vmm_manager_for_each_guest_vcpu(rvcpu, guest) {
		if (!(vmm_manager_vcpu_get_state(rvcpu) &
		      VMM_VCPU_STATE_INTERRUPTIBLE))
			continue;
		if (hbase != -1UL) {
			if (rvcpu->subid < hbase)
				continue;
			if (!(hmask & (1UL << (rvcpu->subid - hbase))))
				continue;
		}
		if (vmm_manager_vcpu_get_hcpu(rvcpu, &hcpu))
			continue;
		vmm_cpumask_set_cpu(hcpu, &cm);
	}
	sbi_cpumask_to_hartmask(&cm, &hm);

	switch (func_id) {
	case SBI_EXT_RFENCE_REMOTE_FENCE_I:
		sbi_remote_fence_i(vmm_cpumask_bits(&hm));
		break;
	case SBI_EXT_RFENCE_REMOTE_SFENCE_VMA:
		sbi_remote_hfence_vvma(vmm_cpumask_bits(&hm),
					args[2], args[3]);
		break;
	case SBI_EXT_RFENCE_REMOTE_SFENCE_VMA_ASID:
		sbi_remote_hfence_vvma_asid(vmm_cpumask_bits(&hm),
					    args[2], args[3], args[4]);
		break;
	case SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA:
	case SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA_VMID:
		/* Flush the nested software TLB of calling VCPU */
		cpu_vcpu_nested_swtlb_flush(vcpu, args[2], args[3]);

		if (mmu_pgtbl_has_hw_tag(npriv->pgtbl)) {
			/*
			 * We use two VMIDs for nested virtualization:
			 * one for virtual-HS/U modes and another for
			 * virtual-VS/VU modes. This means we need to
			 * restrict guest remote HFENCE.GVMA to VMID
			 * used for virtual-VS/VU modes.
			 */
			sbi_remote_hfence_gvma_vmid(vmm_cpumask_bits(&hm),
					args[2], args[3],
					mmu_pgtbl_has_hw_tag(npriv->pgtbl));
		} else {
			/*
			 * No VMID support so we do remote HFENCE.GVMA
			 * accross all VMIDs.
			 */
			sbi_remote_hfence_gvma(vmm_cpumask_bits(&hm),
					       args[2], args[3]);
		}
		break;
	case SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA:
	case SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA_ASID:
		hgatp = 0;
		if (mmu_pgtbl_has_hw_tag(npriv->pgtbl)) {
			/*
			 * We use two VMIDs for nested virtualization:
			 * one for virtual-HS/U modes and another for
			 * virtual-VS/VU modes. This means we need to
			 * switch hgatp.VMID before doing forwarding
			 * SBI call to host firmware.
			 */
			hgatp = mmu_pgtbl_hw_tag(npriv->pgtbl);
			hgatp = csr_swap(CSR_HGATP, hgatp << HGATP_VMID_SHIFT);
		}

		if (func_id == SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA) {
			sbi_remote_hfence_vvma(vmm_cpumask_bits(&hm),
					       args[2], args[3]);
		} else {
			sbi_remote_hfence_vvma_asid(
					vmm_cpumask_bits(&hm),
					args[2], args[3], args[4]);
		}

		if (mmu_pgtbl_has_hw_tag(npriv->pgtbl)) {
			csr_write(CSR_HGATP, hgatp);
		}
		break;
	default:
		return SBI_ERR_NOT_SUPPORTED;
	};

	return 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_rfence = {
	.name = "rfence",
	.extid_start = SBI_EXT_RFENCE,
	.extid_end = SBI_EXT_RFENCE,
	.handle = vcpu_sbi_rfence_ecall,
};

static int vcpu_sbi_ipi_ecall(struct vmm_vcpu *vcpu, unsigned long ext_id,
			      unsigned long func_id, unsigned long *args,
			      struct cpu_vcpu_sbi_return *out)
{
	struct vmm_vcpu *rvcpu;
	struct vmm_guest *guest = vcpu->guest;
	unsigned long hmask = args[0], hbase = args[1];

	if (func_id != SBI_EXT_IPI_SEND_IPI)
		return SBI_ERR_NOT_SUPPORTED;

	vmm_manager_for_each_guest_vcpu(rvcpu, guest) {
		if (!(vmm_manager_vcpu_get_state(rvcpu) &
		      VMM_VCPU_STATE_INTERRUPTIBLE))
			continue;
		if (hbase != -1UL) {
			if (rvcpu->subid < hbase)
				continue;
			if (!(hmask & (1UL << (rvcpu->subid - hbase))))
				continue;
		}
		vmm_vcpu_irq_assert(rvcpu, IRQ_VS_SOFT, 0x0);
	}

	return 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_ipi = {
	.name = "ipi",
	.extid_start = SBI_EXT_IPI,
	.extid_end = SBI_EXT_IPI,
	.handle = vcpu_sbi_ipi_ecall,
};

static int vcpu_sbi_srst_ecall(struct vmm_vcpu *vcpu, unsigned long ext_id,
			       unsigned long func_id, unsigned long *args,
			       struct cpu_vcpu_sbi_return *out)
{
	int ret;
	struct vmm_guest *guest = vcpu->guest;

	if (func_id != SBI_EXT_SRST_RESET)
		return SBI_ERR_NOT_SUPPORTED;

	if ((((u32)-1U) <= ((u64)args[0])) ||
	    (((u32)-1U) <= ((u64)args[1])))
		return SBI_ERR_INVALID_PARAM;

	switch (args[0]) {
	case SBI_SRST_RESET_TYPE_SHUTDOWN:
		ret = vmm_manager_guest_shutdown_request(guest);
		if (ret) {
			vmm_printf("%s: guest %s shutdown request failed "
				   "with error = %d\n", __func__,
				   guest->name, ret);
		}
		break;
	case SBI_SRST_RESET_TYPE_COLD_REBOOT:
	case SBI_SRST_RESET_TYPE_WARM_REBOOT:
		ret = vmm_manager_guest_reboot_request(guest);
		if (ret) {
			vmm_printf("%s: guest %s reset request failed "
				   "with error = %d\n", __func__,
				   guest->name, ret);
		}
		break;
	default:
		return SBI_ERR_NOT_SUPPORTED;
	};

	return 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_srst = {
	.name = "srst",
	.extid_start = SBI_EXT_SRST,
	.extid_end = SBI_EXT_SRST,
	.handle = vcpu_sbi_srst_ecall,
};

#define DBCN_BUF_SIZE			256

static int vcpu_sbi_dbcn_ecall(struct vmm_vcpu *vcpu, unsigned long ext_id,
			       unsigned long func_id, unsigned long *args,
			       struct cpu_vcpu_sbi_return *out)
{
	struct riscv_guest_serial *gs = riscv_guest_serial(vcpu->guest);
	int ret = SBI_SUCCESS;
	u8 buf[DBCN_BUF_SIZE];
	unsigned long len;

	switch (func_id) {
	case SBI_EXT_DBCN_CONSOLE_WRITE:
	case SBI_EXT_DBCN_CONSOLE_READ:
		/*
		 * On RV32, the M-mode can only access the first 4GB of
		 * the physical address space because M-mode does not have
		 * MMU to access full 34-bit physical address space.
		 *
		 * Based on above, we simply fail if the upper 32bits of
		 * the physical address (i.e. a2 register) is non-zero on
		 * RV32.
		 *
		 * Analogously, we fail if the upper 64bit of the
		 * physical address (i.e. a2 register) is non-zero on
		 * RV64.
		 */
		if (args[2]) {
			ret = SBI_ERR_FAILED;
			break;
		}

		len = (DBCN_BUF_SIZE < args[0]) ? DBCN_BUF_SIZE : args[0];
		if (func_id == SBI_EXT_DBCN_CONSOLE_WRITE) {
			len = vmm_guest_memory_read(vcpu->guest, args[1],
						    buf, len, TRUE);
			out->value = vmm_vserial_receive(gs->vserial, buf, len);
		} else {
			/*
			 * We don't support read operation because Guest
			 * always has a proper console with read/write
			 * support.
			 */
			ret = SBI_ERR_DENIED;
		}
		break;
	case SBI_EXT_DBCN_CONSOLE_WRITE_BYTE:
		buf[0] = (u8)args[0];
		if (!vmm_vserial_receive(gs->vserial, buf, 1))
			ret = SBI_ERR_FAILED;
		out->value = 0;
		break;
	default:
		ret = SBI_ERR_NOT_SUPPORTED;
		break;
	}

	return ret;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_dbcn = {
	.name = "dbcn",
	.extid_start = SBI_EXT_DBCN,
	.extid_end = SBI_EXT_DBCN,
	.handle = vcpu_sbi_dbcn_ecall,
};
