/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file cpu_vcpu_trap.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header of VCPU trap handling
 */
#ifndef _CPU_VCPU_TRAP_H__
#define _CPU_VCPU_TRAP_H__

#include <vmm_types.h>
#include <vmm_manager.h>

int cpu_vcpu_redirect_trap(struct vmm_vcpu *vcpu,
			   arch_regs_t *regs,
			   unsigned long scause,
			   unsigned long stval);

int cpu_vcpu_page_fault(struct vmm_vcpu *vcpu,
			arch_regs_t *regs,
			unsigned long cause,
			unsigned long stval,
			unsigned long htval,
			unsigned long htinst);

int cpu_vcpu_illegal_insn_fault(struct vmm_vcpu *vcpu,
				arch_regs_t *regs,
				unsigned long stval);

#endif
