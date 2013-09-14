/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Header File for VCPU cp15 emulation
 */
#ifndef _CPU_VCPU_CP15_H__
#define _CPU_VCPU_CP15_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Handle stage2 instruction abort */
int cpu_vcpu_inst_abort(struct vmm_vcpu * vcpu, 
			arch_regs_t * regs,
			u32 il, u32 iss, 
			physical_addr_t fipa);

/** Handle stage2 data abort */
int cpu_vcpu_data_abort(struct vmm_vcpu * vcpu, 
			arch_regs_t * regs,
			u32 il, u32 iss, 
			physical_addr_t fipa);

/** Read one SPR */
bool cpu_vcpu_spr_read(struct vmm_vcpu * vcpu, 
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u64 *data);

/** Write one SPR */
bool cpu_vcpu_spr_write(struct vmm_vcpu * vcpu, 
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u64 data);

bool cpu_vcpu_cp15_read(struct vmm_vcpu * vcpu, 
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			u64 *data);

bool cpu_vcpu_cp15_write(struct vmm_vcpu * vcpu, 
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm, 
			 u64 data);

/** Initialize SPR subsystem for a VCPU */
int cpu_vcpu_spr_init(struct vmm_vcpu * vcpu, u32 cpuid);

/** DeInitialize SPR subsystem for a VCPU */
int cpu_vcpu_spr_deinit(struct vmm_vcpu * vcpu);

#endif /* _CPU_VCPU_CP15_H */
