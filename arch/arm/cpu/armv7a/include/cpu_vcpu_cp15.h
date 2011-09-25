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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header File for VCPU cp15 emulation
 */
#ifndef _CPU_VCPU_CP15_H__
#define _CPU_VCPU_CP15_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Handle translation fault for a VCPU */
int cpu_vcpu_cp15_trans_fault(vmm_vcpu_t * vcpu, 
			      vmm_user_regs_t * regs, 
			      u32 far, u32 wnr, u32 page, u32 xn);

/** Handle access fault for a VCPU */
int cpu_vcpu_cp15_access_fault(vmm_vcpu_t * vcpu, 
			       vmm_user_regs_t * regs, 
			       u32 far, u32 wnr, u32 page, u32 xn);

/** Handle domain fault for a VCPU */
int cpu_vcpu_cp15_domain_fault(vmm_vcpu_t * vcpu, 
			       vmm_user_regs_t * regs, 
			       u32 far, u32 wnr, u32 page, u32 xn);

/** Handle permission fault for a VCPU */
int cpu_vcpu_cp15_perm_fault(vmm_vcpu_t * vcpu, 
			     vmm_user_regs_t * regs, 
			     u32 far, u32 wnr, u32 page, u32 xn);

/** Read one registers from CP15 */
bool cpu_vcpu_cp15_read(vmm_vcpu_t * vcpu, 
			vmm_user_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u32 *data);

/** Write one registers to CP15 */
bool cpu_vcpu_cp15_write(vmm_vcpu_t * vcpu, 
			 vmm_user_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			 u32 data);

/** Read from memory using VCPU CP15 */
int cpu_vcpu_cp15_mem_read(vmm_vcpu_t * vcpu, 
			   vmm_user_regs_t * regs,
			   virtual_addr_t addr, 
			   void *dst, u32 dst_len, 
			   bool force_unpriv);

/** Write to memory using VCPU CP15 */
int cpu_vcpu_cp15_mem_write(vmm_vcpu_t * vcpu, 
			    vmm_user_regs_t * regs,
			    virtual_addr_t addr, 
			    void *src, u32 src_len,
			    bool force_unpriv);

/* Read from memory using VCPU CP15 */
virtual_addr_t cpu_vcpu_cp15_vector_addr(vmm_vcpu_t * vcpu, 
					 u32 irq_no);

/* Syncronize VCPU CP15 with change in VCPU mode */
void cpu_vcpu_cp15_sync_cpsr(vmm_vcpu_t * vcpu);

/** Set MMU context for given VCPU */
void cpu_vcpu_cp15_set_mmu_context(vmm_vcpu_t * vcpu);

/** Initialize CP15 subsystem for a VCPU */
int cpu_vcpu_cp15_init(vmm_vcpu_t * vcpu, u32 cpuid);

#endif /* _CPU_VCPU_CP15_H */
