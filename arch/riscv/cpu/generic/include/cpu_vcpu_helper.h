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
 * @file cpu_vcpu_helper.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header of VCPU helper functions
 */
#ifndef _CPU_VCPU_HELPER_H__
#define _CPU_VCPU_HELPER_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Function to dump general registers */
void cpu_vcpu_dump_general_regs(struct vmm_chardev *cdev,
				arch_regs_t *regs);

/** Function to dump private registers */
void cpu_vcpu_dump_private_regs(struct vmm_chardev *cdev,
				struct vmm_vcpu *vcpu);

/** Function to dump exception registers */
void cpu_vcpu_dump_exception_regs(struct vmm_chardev *cdev,
				  unsigned long scause,
				  unsigned long stval,
				  unsigned long htval);

#endif
