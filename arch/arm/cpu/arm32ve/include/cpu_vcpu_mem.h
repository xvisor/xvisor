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
 * @file cpu_vcpu_mem.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header File for VCPU memory read/write emulation
 */
#ifndef _CPU_VCPU_MEM_H__
#define _CPU_VCPU_MEM_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Read from memory for VCPU */
int cpu_vcpu_mem_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *dst, u32 dst_len, 
			bool force_unpriv);

/** Write to memory for VCPU */
int cpu_vcpu_mem_write(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *src, u32 src_len,
			bool force_unpriv);

/** Read-Exclusive from memory for VCPU */
int cpu_vcpu_mem_readex(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *dst, u32 dst_len, 
			bool force_unpriv);

/** Write-Exclusive to memory for VCPU */
int cpu_vcpu_mem_writeex(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *src, u32 src_len,
			bool force_unpriv);

#endif /* _CPU_VCPU_MEM_H */
