/**
 * Copyright (c) 2011-2013 Sting Cheng.
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
 * @file cpu_vcpu_cp14.h
 * @author Sting Cheng (sting.cheng@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for VCPU cp14 (Debug, Trace, and ThumbEE) emulation
 */
#ifndef _CPU_VCPU_CP14_H__
#define _CPU_VCPU_CP14_H__

#include <vmm_types.h>
#include <vmm_chardev.h>
#include <vmm_manager.h>

/** Read one registers from CP14 */
bool cpu_vcpu_cp14_read(struct vmm_vcpu *vcpu,
			arch_regs_t *regs,
			u32 opc1, u32 opc2, u32 CRn, u32 CRm,
			u32 *data);

/** Write one registers to CP14 */
bool cpu_vcpu_cp14_write(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs,
			 u32 opc1, u32 opc2, u32 CRn, u32 CRm,
			 u32 data);

/** Save CP14 context for given VCPU */
void cpu_vcpu_cp14_save(struct vmm_vcpu *vcpu);

/** Restore CP14 context for given VCPU */
void cpu_vcpu_cp14_restore(struct vmm_vcpu *vcpu);

/** Print CP14 context for given VCPU */
void cpu_vcpu_cp14_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu);

/** Initialize CP14 context for given VCPU */
int cpu_vcpu_cp14_init(struct vmm_vcpu *vcpu);

/** DeInitialize CP14 context for given VCPU */
int cpu_vcpu_cp14_deinit(struct vmm_vcpu *vcpu);

#endif /* _CPU_VCPU_CP14_H__ */
