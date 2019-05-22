/**
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
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
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief RISC-V low-level VCPU save/restore functions
 */

#ifndef _CPU_VCPU_SWITCH_H__
#define _CPU_VCPU_SWITCH_H__

#include <vmm_types.h>

struct riscv_priv_fp_f;

/* Save FP 'F' context */
void __cpu_vcpu_fp_f_save(struct riscv_priv_fp_f *f);

/* Restore FP 'F' context */
void __cpu_vcpu_fp_f_restore(struct riscv_priv_fp_f *f);

struct riscv_priv_fp_d;

/* Save FP 'D' context */
void __cpu_vcpu_fp_d_save(struct riscv_priv_fp_d *d);

/* Restore FP 'D' context */
void __cpu_vcpu_fp_d_restore(struct riscv_priv_fp_d *d);

#endif
