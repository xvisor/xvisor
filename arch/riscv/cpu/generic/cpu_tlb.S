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
 * @file cpu_tlb.S
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief Low-level implementation of TLB functions
 */

#include <riscv_asm.h>

	/*
	 * Instruction encoding of hfence.gvma is:
	 * 1010001 rs2(5) rs1(5) 000 00000 1110011
	 */

	.align 3
	.global __hfence_gvma_vmid_gpa
__hfence_gvma_vmid_gpa:
	/* hfence.gvma a1, a0 */
	.word 0xa2a60073
	ret

	.align 3
	.global __hfence_gvma_vmid
__hfence_gvma_vmid:
	/* hfence.gvma zero, a0 */
	.word 0xa2a00073
	ret

	.align 3
	.global __hfence_gvma_gpa
__hfence_gvma_gpa:
	/* hfence.gvma a0 */
	.word 0xa2050073
	ret

	.align 3
	.global __hfence_gvma_all
__hfence_gvma_all:
	/* hfence.gvma */
	.word 0xa2000073
	ret
