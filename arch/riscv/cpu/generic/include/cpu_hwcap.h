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
 * @file cpu_hwcap.h
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief RISC-V CPU hardware capabilities
 */

#ifndef _CPU_HWCAP_H__
#define _CPU_HWCAP_H__

#include <vmm_types.h>

#define RISCV_ISA_EXT_A		(1UL << ('A' - 'A'))
#define RISCV_ISA_EXT_a		RISCV_ISA_EXT_A
#define RISCV_ISA_EXT_C		(1UL << ('C' - 'A'))
#define RISCV_ISA_EXT_c		RISCV_ISA_EXT_C
#define RISCV_ISA_EXT_D		(1UL << ('D' - 'A'))
#define RISCV_ISA_EXT_d		RISCV_ISA_EXT_D
#define RISCV_ISA_EXT_F		(1UL << ('F' - 'A'))
#define RISCV_ISA_EXT_f		RISCV_ISA_EXT_F
#define RISCV_ISA_EXT_H		(1UL << ('H' - 'A'))
#define RISCV_ISA_EXT_h		RISCV_ISA_EXT_H
#define RISCV_ISA_EXT_I		(1UL << ('I' - 'A'))
#define RISCV_ISA_EXT_i		RISCV_ISA_EXT_I
#define RISCV_ISA_EXT_M		(1UL << ('M' - 'A'))
#define RISCV_ISA_EXT_m		RISCV_ISA_EXT_M
#define RISCV_ISA_EXT_S		(1UL << ('S' - 'A'))
#define RISCV_ISA_EXT_s		RISCV_ISA_EXT_S
#define RISCV_ISA_EXT_U		(1UL << ('U' - 'A'))
#define RISCV_ISA_EXT_u		RISCV_ISA_EXT_U

/** Available RISC-V ISA flags */
extern unsigned long riscv_isa;

#define riscv_isa_extension_available(ext_char)	\
		(riscv_isa & RISCV_ISA_EXT_##ext_char)

#define riscv_hyp_ext_enabled (riscv_isa_extension_available(H) || \
				riscv_isa_extension_available(h))
/** Available RISC-V VMID bits */
extern unsigned long riscv_vmid_bits;

#endif
