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
#include <libs/bitops.h>

#define RISCV_ISA_EXT_a		('a' - 'a')
#define RISCV_ISA_EXT_c		('c' - 'a')
#define RISCV_ISA_EXT_d		('d' - 'a')
#define RISCV_ISA_EXT_f		('f' - 'a')
#define RISCV_ISA_EXT_h		('h' - 'a')
#define RISCV_ISA_EXT_i		('i' - 'a')
#define RISCV_ISA_EXT_m		('m' - 'a')
#define RISCV_ISA_EXT_s		('s' - 'a')
#define RISCV_ISA_EXT_u		('u' - 'a')

#define RISCV_ISA_EXT_zicsr	(('z' - 'a') + 1)
#define RISCV_ISA_EXT_zifencei	(('z' - 'a') + 2)
#define RISCV_ISA_EXT_zam	(('z' - 'a') + 3)
#define RISCV_ISA_EXT_ztso	(('z' - 'a') + 4)

#define RISCV_ISA_EXT_MAX	256

/**
 * Get base extension word
 *
 * @isa_bitmap ISA bitmap to use
 * @returns base extension word as unsigned long value
 *
 * NOTE: If isa_bitmap is NULL then Host ISA bitmap will be used.
 */
unsigned long riscv_isa_extension_base(const unsigned long *isa_bitmap);

#define riscv_isa_extension_mask(ext) BIT_MASK(RISCV_ISA_EXT_##ext)

/**
 * Check whether given extension is available or not
 *
 * @isa_bitmap ISA bitmap to use
 * @bit bit position of the desired extension
 * @returns true or false
 *
 * NOTE: If isa_bitmap is NULL then Host ISA bitmap will be used.
 */
bool __riscv_isa_extension_available(const unsigned long *isa_bitmap, int bit);

#define riscv_isa_extension_available(isa_bitmap, ext)	\
	__riscv_isa_extension_available(isa_bitmap, RISCV_ISA_EXT_##ext)

/**
 * Populate string representation of ISA features
 *
 * @xlen register length
 * @isa_bitmap ISA bitmap to use
 * @out output string buffer
 * @out_sz output string buffer size
 *
 * NOTE: output string buffer has to be atleast 16 bytes
 * NOTE: If isa_bitmap is NULL then Host ISA bitmap will be used.
 */
int riscv_isa_populate_string(unsigned long xlen,
			      const unsigned long *isa_bitmap,
			      char *out, size_t out_sz);

/**
 * Parse string representation of ISA features
 *
 * @isa input ISA feature string
 * @out_xlen output register length
 * @out_bitmap output ISA bitmap
 * @out_bitmap_sz output ISA bitmap size
 */
int riscv_isa_parse_string(const char *isa,
			   unsigned long *out_xlen,
			   unsigned long *out_bitmap,
			   size_t out_bitmap_sz);

/** RISC-V XLEN */
extern unsigned long riscv_xlen;

/** Available RISC-V VMID bits */
extern unsigned long riscv_vmid_bits;

/** RISC-V Time Base Frequency */
extern unsigned long riscv_timer_hz;

#endif
