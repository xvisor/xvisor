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
 * @file cpu_vcpu_switch_rvv.S
 * @author Jaromir Mikusik
 * @brief RISC-V Vector extension low-level VCPU save/restore functions
 */

#include <riscv_asm.h>
#include <riscv_encoding.h>
#include <arch_regs.h>


	.align 3
	.global __cpu_vcpu_rvv_save
__cpu_vcpu_rvv_save:
	csrr t6, CSR_SSTATUS
	li t1, SSTATUS_VS
	csrs CSR_SSTATUS, t1

    addi t1, a0, 0
    csrr t2, vtype
    sd t2, (t1)

    addi t1, t1, 8
    csrr t2, vl
    sd t2, (t1)

    addi t1, t1, 8
    csrr t2, vxrm
    sd t2, (t1)

    addi t1, t1, 8
    csrr t2, vxsat
    sd t2, (t1)

    addi t1, t1, 8
    csrr t2, vstart
    sd t2, (t1)

    addi t1, t1, 8
    csrr t2, vcsr
    sd t2, (t1)

    addi t1, t1, 8

    csrr t3, vlenb
    li t2, 1
    sll t2, t2, t3
    vs8r.v  v0, (t1)
    add t1, t1, t2
    vs8r.v  v8, (t1)
    add t1, t1, t2
    vs8r.v  v16, (t1)
    add t1, t1, t2
    vs8r.v  v24, (t1)

	csrw CSR_SSTATUS, t6
    ret

	.align 3
	.global __cpu_vcpu_rvv_restore
__cpu_vcpu_rvv_restore:
	csrr t6, CSR_SSTATUS
	li t1, SSTATUS_VS
	csrs CSR_SSTATUS, t1

    addi t1, a0, 0
    # vl and vtype are RO and can only be manipulated via vset*vl* instructions
    # so load both vtype and vl and set them properly
    ld t2, (t1)
    addi t1, t1, 8
    ld t3, (t1)
    vsetvl zero, t3, t2

    addi t1, t1, 8
    ld t2, (t1)
    csrw vxrm, t2

    addi t1, t1, 8
    ld t2, (t1)
    csrw vxsat, t2

    addi t1, t1, 8
    ld t2, (t1)
    csrw vstart, t2

    addi t1, t1, 8
    ld t2, (t1)
    csrw vcsr, t2

    addi t1, t1, 8

    csrr t3, vlenb
    li t2, 1
    vl8r.v  v0, (t1)
    add t1, t1, t2
    vl8r.v  v8, (t1)
    add t1, t1, t2
    vl8r.v  v16, (t1)
    add t1, t1, t2
    vl8r.v  v24, (t1)
    add t1, t1, t2

	csrw CSR_SSTATUS, t6
    ret

