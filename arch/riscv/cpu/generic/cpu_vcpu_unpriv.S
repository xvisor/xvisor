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
 * @file cpu_vcpu_unpriv.S
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief RISC-V low-level unpriv access functions
 */

#include <cpu_vcpu_trap.h>
#include <riscv_asm.h>
#include <riscv_encoding.h>

.macro TRAP_HANDLER __insn_len __ttmp, __taddr
	csrr	\__ttmp, CSR_SEPC
	REG_S	\__ttmp, RISCV_VCPU_TRAP_SEPC(\__taddr)
	addi	\__ttmp, \__ttmp, \__insn_len
	csrw	CSR_SEPC, \__ttmp
	csrr	\__ttmp, CSR_SCAUSE
	REG_S	\__ttmp, RISCV_VCPU_TRAP_SCAUSE(\__taddr)
	csrr	\__ttmp, CSR_STVAL
	REG_S	\__ttmp, RISCV_VCPU_TRAP_STVAL(\__taddr)
	csrr	\__ttmp, CSR_HTVAL
	REG_S	\__ttmp, RISCV_VCPU_TRAP_HTVAL(\__taddr)
	csrr	\__ttmp, CSR_HTINST
	REG_S	\__ttmp, RISCV_VCPU_TRAP_HTINST(\__taddr)
	sret
.endm

.macro SETUP_TRAP __insn_len, __stvec, __ttmp, __taddr
	/* Change to Temporary exception handler */
	la	\__stvec, 998f
	csrrw	\__stvec, CSR_STVEC, \__stvec
	j	999f

	/* Temporary exception handler */
	.align 2
998:
	TRAP_HANDLER \__insn_len, \__ttmp, \__taddr
999:
.endm

.macro RESTORE_TRAP __stvec
	/* Restore exception handler */
	csrw	CSR_STVEC, \__stvec
.endm

.macro SETUP_UNPRIV __insn_len, __stvec, __ttmp, __taddr
	/* Setup Trap */
	SETUP_TRAP \__insn_len, \__stvec, \__ttmp, \__taddr
.endm

.macro CLEANUP_UNPRIV __stvec
	/* Restore Trap */
	RESTORE_TRAP \__stvec
.endm

	.align 3
	.global __cpu_vcpu_unpriv_trap_handler
__cpu_vcpu_unpriv_trap_handler:
	TRAP_HANDLER 4, t0, t1

	.align 3
	.global __cpu_vcpu_unpriv_read_insn
__cpu_vcpu_unpriv_read_insn:
	/* Setup unprivilege access */
	SETUP_UNPRIV 4, t0, t1, a1

	/*
	 * Read instruction (only t2, t3, t4, t5 and t6 registers available)
	 *
	 * HLVX.HU instruction
	 * 0110010 00011 rs1 100 rd 1110011
	 */
	add	t5, a0, zero
	/* HLVX.HU a0, (t5) */
	.word	0x643f4573
	andi	t6, a0, 3
	addi	t6, t6, -3
	bne	t6, zero, 2f
	addi	t5, t5, 2
	/* HLVX.HU t6, (t5) */
	.word	0x643f4ff3
	sll	t6, t6, 16
	add	a0, a0, t6
2:

	/* Cleanup unprivilege access */
	CLEANUP_UNPRIV t0

	/* Return to C code */
	ret

	.align 3
	.global __cpu_vcpu_unpriv_read_ulong
__cpu_vcpu_unpriv_read_ulong:
	/* Setup unprivilege access */
	SETUP_UNPRIV 2, t0, t1, a1

	/*
	 * Read instruction (only t2, t3, t4, t5 and t6 registers available)
	 *
	 * HLV.D instruction
	 * 0110110 00000 rs1 100 rd 1110011
	 *
	 * HLV.W instruction
	 * 0110100 00000 rs1 100 rd 1110011
	 */
#ifdef CONFIG_64BIT
	/* HLV.D a0, (a0) */
	.word	0x6c054573
#else
	/* HLV.W a0, (a0) */
	.word	0x68054573
#endif

	/* Cleanup unprivilege access */
	CLEANUP_UNPRIV t0

	/* Return to C code */
	ret
