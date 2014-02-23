/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file cpu_vcpu_inject.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for VCPU exception injection
 */
#ifndef _CPU_VCPU_INJECT_H__
#define _CPU_VCPU_INJECT_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Function to inject undef exception to a VCPU */
int cpu_vcpu_inject_undef(struct vmm_vcpu *vcpu,
			  arch_regs_t *regs);

/** Function to inject prefetch abort exception to a VCPU */
int cpu_vcpu_inject_pabt(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs);

/** Function to inject data abort exception to a VCPU */
int cpu_vcpu_inject_dabt(struct vmm_vcpu *vcpu,
			 arch_regs_t *regs,
			 virtual_addr_t addr);

#endif /* _CPU_VCPU_INJECT_H__ */
