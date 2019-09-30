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
 * @file cpu_vcpu_switch.S
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief RISC-V low-level VCPU save/restore functions
 */

#include <riscv_asm.h>
#include <riscv_encoding.h>

.macro SETUP_CSR __sstatus, __hstatus
	/* Load MXR bit value */
	li	\__hstatus, SSTATUS_MXR

	/* Save SSTATUS and set MXR bit */
	csrrs	\__sstatus, CSR_SSTATUS, \__hstatus

	/* Save HSTATUS and set SPRV bit */
	csrrsi	\__hstatus, CSR_HSTATUS, HSTATUS_SPRV
.endm

.macro RESTORE_CSR __sstatus, __hstatus
	/* Restore HSTATUS */
	csrw	CSR_HSTATUS, \__hstatus

	/* Restore SSTATUS */
	csrw	CSR_SSTATUS, \__sstatus
.endm

.macro SETUP_TRAP __insn_len, __stvec, __tscause
	/* Setup registers for unprivilege access trap handling */
	add	\__tscause, zero, zero

	/* Change to Temporary exception handler */
	la	\__stvec, 998f
	csrrw	\__stvec, CSR_STVEC, \__stvec
	j	999f

	/* Temporary exception handler */
	.align 2
998:
	csrr	\__tscause, CSR_SEPC
	addi	\__tscause, \__tscause, \__insn_len
	csrw	CSR_SEPC, \__tscause
	csrr	\__tscause, CSR_SCAUSE
	sret
999:
.endm

.macro RESTORE_TRAP __stvec
	/* Restore exception handler */
	csrw	CSR_STVEC, \__stvec
.endm

.macro SETUP_UNPRIV __insn_len, __sstatus, __hstatus, __stvec, __tscause
	/* Setup CSRs */
	SETUP_CSR \__sstatus, \__hstatus

	/* Setup Trap */
	SETUP_TRAP \__insn_len, \__stvec, \__tscause
.endm

.macro CLEANUP_UNPRIV __sstatus, __hstatus, __stvec
	/* Restore Trap */
	RESTORE_TRAP \__stvec

	/* Restore CSRs */
	RESTORE_CSR \__sstatus, \__hstatus
.endm

	.align 3
	.global __cpu_vcpu_unpriv_read_insn
__cpu_vcpu_unpriv_read_insn:
	/* Setup unprivilege access */
	SETUP_UNPRIV 4, t0, t1, t2, t3

	/* Read instruction (only t4, t5 and t6 registers available) */
	add	t5, a0, zero
	lhu	a0, (t5)
	andi	t6, a0, 3
	addi	t6, t6, -3
	bne	t6, zero, 2f
	lhu	t6, 2(t5)
	sll	t6, t6, 16
	add	a0, a0, t6
2:

	/* Cleanup unprivilege access */
	CLEANUP_UNPRIV t0, t1, t2

	/* Return trap details */
	REG_S	t3, (a1)

	/* Return to C code */
	ret

	.align 3
	.global __cpu_vcpu_unpriv_read_ulong
__cpu_vcpu_unpriv_read_ulong:
	/* Setup unprivilege access */
	SETUP_UNPRIV 2, t0, t1, t2, t3

	/* Read unsigned long (only t4, t5 and t6 registers available) */
	REG_L	a0, (a0)

	/* Cleanup unprivilege access */
	CLEANUP_UNPRIV t0, t1, t2

	/* Return trap details */
	REG_S	t3, (a1)

	/* Return to C code */
	ret
