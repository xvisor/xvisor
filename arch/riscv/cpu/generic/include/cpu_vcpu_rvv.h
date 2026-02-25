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
 * @file cpu_vcpu_rvv.h
 * @author Jaromir Mikusik
 * @brief header of VCPU RVV functions
 */

#ifndef _CPU_VCPU_RVV_H__
#define _CPU_VCPU_RVV_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Function to reset RVV state */
void cpu_vcpu_rvv_reset(struct vmm_vcpu *vcpu);

/** Function to initialize RVV state */
int cpu_vcpu_rvv_init(struct vmm_vcpu *vcpu);

/** Function to deinitialize RVV state */
void cpu_vcpu_rvv_deinit(struct vmm_vcpu *vcpu);

/** Function to save RVV state */
void cpu_vcpu_rvv_save(struct vmm_vcpu *vcpu, arch_regs_t *regs);

/** Function to restore RVV state */
void cpu_vcpu_rvv_restore(struct vmm_vcpu *vcpu, arch_regs_t *regs);

/** Function to dump RVV state */
void cpu_vcpu_rvv_dump_regs(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu);

#endif
