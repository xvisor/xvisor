/**
 * Copyright (c) 2022 Ventana Micro Systems Inc.
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
 * @file cpu_vcpu_nested.h
 * @author Anup Patel (apatel@ventanamicro.com)
 * @brief header of VCPU nested functions
 */

#ifndef _CPU_VCPU_NESTED_H__
#define _CPU_VCPU_NESTED_H__

#include <vmm_types.h>

struct vmm_vcpu;
struct cpu_vcpu_trap;
struct arch_regs;

/** Function to flush nested software TLB */
void cpu_vcpu_nested_swtlb_flush(struct vmm_vcpu *vcpu,
				 physical_addr_t guest_gpa,
				 physical_size_t guest_gpa_size);

/** Function to init nested state */
int cpu_vcpu_nested_init(struct vmm_vcpu *vcpu);

/** Function to reset nested state */
void cpu_vcpu_nested_reset(struct vmm_vcpu *vcpu);

/** Function to initialize nested state */
void cpu_vcpu_nested_deinit(struct vmm_vcpu *vcpu);

/** Function to dump nested registers */
void cpu_vcpu_nested_dump_regs(struct vmm_chardev *cdev,
			       struct vmm_vcpu *vcpu);

/** Function to access nested non-virt CSRs */
int cpu_vcpu_nested_smode_csr_rmw(struct vmm_vcpu *vcpu, arch_regs_t *regs,
			unsigned int csr_num, unsigned long *val,
			unsigned long new_val, unsigned long wr_mask);

/** Function to access nested virt CSRs */
int cpu_vcpu_nested_hext_csr_rmw(struct vmm_vcpu *vcpu, arch_regs_t *regs,
			unsigned int csr_num, unsigned long *val,
			unsigned long new_val, unsigned long wr_mask);

/** Function to handle nested page fault */
int cpu_vcpu_nested_page_fault(struct vmm_vcpu *vcpu,
			       bool trap_from_smode,
			       const struct cpu_vcpu_trap *trap,
			       struct cpu_vcpu_trap *out_trap);

/** Function to handle nested hfence.vvma instruction */
void cpu_vcpu_nested_hfence_vvma(struct vmm_vcpu *vcpu,
				 unsigned long *vaddr, unsigned int *asid);

/** Function to handle nested hfence.gvma instruction */
void cpu_vcpu_nested_hfence_gvma(struct vmm_vcpu *vcpu,
				 physical_addr_t *gaddr, unsigned int *vmid);

/**
 * Function to handle nested hlv instruction
 * @returns (< 0) error code upon failure and (>= 0) trap return value
 * upon success
 */
int cpu_vcpu_nested_hlv(struct vmm_vcpu *vcpu, unsigned long vaddr,
			bool hlvx, void *data, unsigned long len,
			unsigned long *out_scause,
			unsigned long *out_stval,
			unsigned long *out_htval);

/**
 * Function to handle nested hsv instruction
 * @returns (< 0) error code upon failure and (>= 0) trap return value
 * upon success
 */
int cpu_vcpu_nested_hsv(struct vmm_vcpu *vcpu, unsigned long vaddr,
			const void *data, unsigned long len,
			unsigned long *out_scause,
			unsigned long *out_stval,
			unsigned long *out_htval);

enum nested_set_virt_event {
	NESTED_SET_VIRT_EVENT_TRAP = 0,
	NESTED_SET_VIRT_EVENT_SRET,
};

/**
 * Function to change nested virtualization state
 * NOTE: This can also update Guest hstatus.SPV and hstatus.SPVP bits
 */
void cpu_vcpu_nested_set_virt(struct vmm_vcpu *vcpu, struct arch_regs *regs,
			      enum nested_set_virt_event event, bool virt,
			      bool spvp, bool gva);

/** Function to take virtual-VS mode interrupts */
void cpu_vcpu_nested_take_vsirq(struct vmm_vcpu *vcpu,
				struct arch_regs *regs);

#endif
