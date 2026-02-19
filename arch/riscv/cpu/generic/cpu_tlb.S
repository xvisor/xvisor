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
	 * HFENCE.GVMA rs1, rs2
	 * HFENCE.GVMA zero, rs2
	 * HFENCE.GVMA rs1
	 * HFENCE.GVMA
	 *
	 * rs1!=zero and rs2!=zero ==> HFENCE.GVMA rs1, rs2
	 * rs1==zero and rs2!=zero ==> HFENCE.GVMA zero, rs2
	 * rs1!=zero and rs2==zero ==> HFENCE.GVMA rs1
	 * rs1==zero and rs2==zero ==> HFENCE.GVMA
	 *
	 */

	.align 3
	.global __hfence_gvma_vmid_gpa
__hfence_gvma_vmid_gpa:
	/* rs1 = a0 (GPA), rs2 = a1 (VMID) */
	hfence.gvma a0, a1
	ret

	.align 3
	.global __hfence_gvma_vmid
__hfence_gvma_vmid:
	/* rs1 = zero, rs2 = a0 (VMID) */
	hfence.gvma zero, a0
	ret

	.align 3
	.global __hfence_gvma_gpa
__hfence_gvma_gpa:
	/* rs1 = a0 (GPA), rs2 = zero */
	hfence.gvma a0, zero
	ret

	.align 3
	.global __hfence_gvma_all
__hfence_gvma_all:
	/* rs1 = zero, rs2 = zero */
	hfence.gvma zero, zero
	ret

	/*
	 * HFENCE.VVMA rs1, rs2
	 * HFENCE.VVMA zero, rs2
	 * HFENCE.VVMA rs1
	 * HFENCE.VVMA
	 *
	 * rs1!=zero and rs2!=zero ==> HFENCE.VVMA rs1, rs2
	 * rs1==zero and rs2!=zero ==> HFENCE.VVMA zero, rs2
	 * rs1!=zero and rs2==zero ==> HFENCE.VVMA rs1
	 * rs1==zero and rs2==zero ==> HFENCE.VVMA
	 *
	 */

	.align 3
	.global __hfence_vvma_asid_va
__hfence_vvma_asid_va:
	/* rs1 = a0 (VA), rs2 = a1 (ASID) */
	hfence.vvma a0, a1
	ret

	.align 3
	.global __hfence_vvma_asid
__hfence_vvma_asid:
	/* rs1 = zero, rs2 = a0 (ASID) */
	hfence.vvma zero, a0
	ret

	.align 3
	.global __hfence_vvma_va
__hfence_vvma_va:
	/* rs1 = a0 (VA), rs2 = zero */
	hfence.vvma a0, zero
	ret

	.align 3
	.global __hfence_vvma_all
__hfence_vvma_all:
	/* rs1 = zero, rs2 = zero */
	hfence.vvma zero, zero
	ret
