/**
 * Copyright (c) 2018 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modifycpu_vcpu_helper.c
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
 * @file cpu_vcpu_helper.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source of VCPU helper functions
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_pagepool.h>
#include <vmm_host_aspace.h>
#include <arch_barrier.h>
#include <arch_guest.h>
#include <arch_vcpu.h>

int arch_guest_init(struct vmm_guest *guest)
{
	/* TODO: For now, nothing to do here. */
	return VMM_OK;
}

int arch_guest_deinit(struct vmm_guest *guest)
{
	/* TODO: For now, nothing to do here. */
	return VMM_OK;
}

int arch_guest_add_region(struct vmm_guest *guest, struct vmm_region *region)
{
	return VMM_OK;
}

int arch_guest_del_region(struct vmm_guest *guest, struct vmm_region *region)
{
	return VMM_OK;
}

int arch_vcpu_init(struct vmm_vcpu *vcpu)
{
	virtual_addr_t sp_exec;

	/* First time allocate exception stack */
	if (!vcpu->reset_count) {
		sp_exec = vmm_pagepool_alloc(VMM_PAGEPOOL_NORMAL,
				VMM_SIZE_TO_PAGE(CONFIG_IRQ_STACK_SIZE));
		if (!sp_exec) {
			return VMM_ENOMEM;
		}
		sp_exec += CONFIG_IRQ_STACK_SIZE;
	} else {
		sp_exec = riscv_regs(vcpu)->sp_exec;
	}

	/* For both Orphan & Normal VCPUs */
	memset(riscv_regs(vcpu), 0, sizeof(arch_regs_t));
	riscv_regs(vcpu)->sepc = vcpu->start_pc;
	riscv_regs(vcpu)->sstatus = SSTATUS_SPP | SSTATUS_SPIE; /* TODO: */
	riscv_regs(vcpu)->sp = vcpu->stack_va +
			     (vcpu->stack_sz - ARCH_CACHE_LINE_SIZE);
	riscv_regs(vcpu)->sp = riscv_regs(vcpu)->sp & ~0x7;
	riscv_regs(vcpu)->sp_exec = sp_exec;

	/* TODO: For Normal VCPUs */

	return VMM_OK;
}

int arch_vcpu_deinit(struct vmm_vcpu *vcpu)
{
	virtual_addr_t sp_exec =
			riscv_regs(vcpu)->sp_exec - CONFIG_IRQ_STACK_SIZE;

	/* TODO: For Normal VCPUs */

	/* For both Orphan & Normal VCPUs */

	/* Free-up excepiton stack */
	vmm_pagepool_free(VMM_PAGEPOOL_NORMAL, sp_exec,
			  VMM_SIZE_TO_PAGE(CONFIG_IRQ_STACK_SIZE));

	/* Clear arch registers */
	memset(riscv_regs(vcpu), 0, sizeof(arch_regs_t));

	return VMM_OK;
}

void arch_vcpu_switch(struct vmm_vcpu *tvcpu,
		      struct vmm_vcpu *vcpu,
		      arch_regs_t *regs)
{
	if (tvcpu) {
		memcpy(riscv_regs(tvcpu), regs, sizeof(*regs));
	}
	memcpy(regs, riscv_regs(vcpu), sizeof(*regs));
}

void arch_vcpu_post_switch(struct vmm_vcpu *vcpu,
			   arch_regs_t *regs)
{
	/* Nothing to do here */
}

void arch_vcpu_regs_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	/* TODO: */
}

void arch_vcpu_stat_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	/* For now no arch specific stats */
}
