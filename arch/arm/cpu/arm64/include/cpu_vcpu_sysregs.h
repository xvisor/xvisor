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
 * @file cpu_vcpu_sysregs.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Header file for VCPU sysreg, cp15 and cp14 emulation
 */
#ifndef _CPU_VCPU_SYSREGS_H__
#define _CPU_VCPU_SYSREGS_H__

#include <vmm_types.h>
#include <vmm_chardev.h>
#include <vmm_manager.h>

/** Read one CP15 register */
bool cpu_vcpu_cp15_read(struct vmm_vcpu *vcpu,
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm,
			u64 *data);

/** Write one CP15 register */
bool cpu_vcpu_cp15_write(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm,
			 u64 data);

/** Read one CP14 register */
bool cpu_vcpu_cp14_read(struct vmm_vcpu *vcpu,
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm,
			u64 *data);

/** Write one CP14 register */
bool cpu_vcpu_cp14_write(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm,
			 u64 data);

/** Read one system register */
bool cpu_vcpu_sysregs_read(struct vmm_vcpu *vcpu,
			   arch_regs_t *regs,
			   u32 iss_sysreg, u64 *data);

/** Write one system register */
bool cpu_vcpu_sysregs_write(struct vmm_vcpu *vcpu, 
			    arch_regs_t *regs,
			    u32 iss_sysreg, u64 data);

/** Save system registers */
void cpu_vcpu_sysregs_save(struct vmm_vcpu *vcpu);

/** Restore system registers */
void cpu_vcpu_sysregs_restore(struct vmm_vcpu *vcpu);

/** Print system registers for given VCPU */
void cpu_vcpu_sysregs_dump(struct vmm_chardev *cdev,
			   struct vmm_vcpu *vcpu);

/** Initialize system registers for given VCPU */
int cpu_vcpu_sysregs_init(struct vmm_vcpu *vcpu, u32 cpuid);

/** DeInitialize system registers for given VCPU */
int cpu_vcpu_sysregs_deinit(struct vmm_vcpu *vcpu);

#endif /* _CPU_VCPU_SYSREGS_H__ */
