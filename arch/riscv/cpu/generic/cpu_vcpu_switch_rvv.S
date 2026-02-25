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
    REG_S t2, (t1)

    addi t1, t1, SZREG
    csrr t2, vl
    REG_S t2, (t1)

    addi t1, t1, SZREG
    csrr t2, vxrm
    REG_S t2, (t1)

    addi t1, t1, SZREG
    csrr t2, vxsat
    REG_S t2, (t1)

    addi t1, t1, SZREG
    csrr t2, vstart
    REG_S t2, (t1)

    addi t1, t1, SZREG
    csrr t2, vcsr
    REG_S t2, (t1)

    /* Skip vlenb, it is read-only, initialized once in cpu_vcpu_rvv_init,
    * and never changes. Advance past it and dereference v to obtain
    * the address of the dynamically allocated vector register buffer.
    */

    addi t1, t1, SZREG
    addi t1, t1, SZREG
    REG_L t1, (t1)

    csrr t3, vlenb
    slli t2, t3, 3
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
    REG_L t2, (t1)
    addi t1, t1, SZREG
    REG_L t3, (t1)
    vsetvl zero, t3, t2

    addi t1, t1, SZREG
    REG_L t2, (t1)
    csrw vxrm, t2

    addi t1, t1, SZREG
    REG_L t2, (t1)
    csrw vxsat, t2

    addi t1, t1, SZREG
    REG_L t2, (t1)
    csrw vstart, t2

    addi t1, t1, SZREG
    REG_L t2, (t1)
    csrw vcsr, t2

    /* Skip vlenb, it is read-only, initialized once in cpu_vcpu_rvv_init,
    * and never changes. Advance past it and dereference v to obtain
    * the address of the dynamically allocated vector register buffer.
    */

    addi t1, t1, SZREG
    addi t1, t1, SZREG
    REG_L t1, (t1)

    csrr t3, vlenb
    slli t2, t3, 3
    vl8r.v  v0, (t1)
    add t1, t1, t2
    vl8r.v  v8, (t1)
    add t1, t1, t2
    vl8r.v  v16, (t1)
    add t1, t1, t2
    vl8r.v  v24, (t1)

	csrw CSR_SSTATUS, t6
    ret

