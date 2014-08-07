/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file cpu_vcpu_switch.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for low-level VCPU context switching functions
 */
#ifndef _CPU_VCPU_SWITCH_H__
#define _CPU_VCPU_SWITCH_H__

#include <arch_regs.h>

/** Save sysregs for given VCPU */
void cpu_vcpu_sysregs_regs_save(struct arm_priv_sysregs *s);

/** Restore sysregs for given VCPU */
void cpu_vcpu_sysregs_regs_restore(struct arm_priv_sysregs *s);

/** Save VFP registers for given VCPU */
void cpu_vcpu_vfp_regs_save(struct arm_priv_vfp *vfp);

/** Restore VFP registers for given VCPU */
void cpu_vcpu_vfp_regs_restore(struct arm_priv_vfp *vfp);

#endif /* _CPU_VCPU_SWITCH_H */
