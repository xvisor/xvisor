/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file cpu_vcpu_cp15.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header File for VCPU cp15 emulation
 */
#ifndef _CPU_VCPU_CP15_H__
#define _CPU_VCPU_CP15_H__

#include <vmm_types.h>
#include <vmm_chardev.h>
#include <vmm_manager.h>

/** Flush entire VTLB for a VCPU */
int cpu_vcpu_cp15_vtlb_flush(struct arm_priv_cp15 *cp15);

/** Flush given virtual address from VTLB for a VCPU */
int cpu_vcpu_cp15_vtlb_flush_va(struct arm_priv_cp15 *cp15,
				virtual_addr_t va);

/** Flush non-global pages from VTLB for a VCPU */
int cpu_vcpu_cp15_vtlb_flush_ng(struct arm_priv_cp15 *cp15);

/** Flush pages whos domain permissions have changed from VTLB for a VCPU */
int cpu_vcpu_cp15_vtlb_flush_domain(struct arm_priv_cp15 *cp15, 
				    u32 dacr_xor_diff);

enum cpu_vcpu_cp15_access_types {
	CP15_ACCESS_READ = 0,
	CP15_ACCESS_WRITE = 1,
	CP15_ACCESS_EXECUTE = 2
};

/** Fill up the cpu_page object for given virtual address */
u32 cpu_vcpu_cp15_find_page(struct vmm_vcpu *vcpu,
			   virtual_addr_t va,
			   int access_type,
			   bool is_user, struct cpu_page *pg);

/** Assert the appropriate abort/fault to vcpu */
int cpu_vcpu_cp15_assert_fault(struct vmm_vcpu *vcpu,
			      arch_regs_t *regs,
			      u32 far, u32 fs, u32 dom, u32 wnr, u32 xn);

/** Handle translation fault for a VCPU */
int cpu_vcpu_cp15_trans_fault(struct vmm_vcpu *vcpu, 
			      arch_regs_t *regs, 
			      u32 far, u32 fs, u32 dom,
			      u32 wnr, u32 xn, bool force_user);

/** Handle access fault for a VCPU */
int cpu_vcpu_cp15_access_fault(struct vmm_vcpu *vcpu, 
			       arch_regs_t *regs, 
			       u32 far, u32 fs, u32 dom,
			       u32 wnr, u32 xn);

/** Handle domain fault for a VCPU */
int cpu_vcpu_cp15_domain_fault(struct vmm_vcpu *vcpu, 
			       arch_regs_t *regs, 
			       u32 far, u32 fs, u32 dom,
			       u32 wnr, u32 xn);

/** Handle permission fault for a VCPU */
int cpu_vcpu_cp15_perm_fault(struct vmm_vcpu *vcpu, 
			     arch_regs_t *regs, 
			     u32 far, u32 fs, u32 dom,
			     u32 wnr, u32 xn);

/** Read one registers from CP15 */
bool cpu_vcpu_cp15_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u32 *data);

/** Write one registers to CP15 */
bool cpu_vcpu_cp15_write(struct vmm_vcpu *vcpu, 
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			 u32 data);

/* Read from memory using VCPU CP15 */
virtual_addr_t cpu_vcpu_cp15_vector_addr(struct vmm_vcpu *vcpu, 
					 u32 irq_no);

/* Syncronize VCPU CP15 with change in VCPU mode */
void cpu_vcpu_cp15_sync_cpsr(struct vmm_vcpu *vcpu);

/** Save CP15 registers for given VCPU */
void cpu_vcpu_cp15_regs_save(struct vmm_vcpu *vcpu);

/** Restore CP15 registers for given VCPU */
void cpu_vcpu_cp15_regs_restore(struct vmm_vcpu *vcpu);

/** Print CP15 registers for given VCPU */
void cpu_vcpu_cp15_regs_dump(struct vmm_chardev *cdev,
			     struct vmm_vcpu *vcpu);

/** Initialize CP15 subsystem for a VCPU */
int cpu_vcpu_cp15_init(struct vmm_vcpu *vcpu, u32 cpuid);

/** DeInitialize CP15 subsystem for a VCPU */
int cpu_vcpu_cp15_deinit(struct vmm_vcpu *vcpu);

#endif /* _CPU_VCPU_CP15_H */
