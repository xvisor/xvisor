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
#include <arch_regs.h>

	.align 3
	.global __cpu_vcpu_fp_f_save
__cpu_vcpu_fp_f_save:
	csrr t2, CSR_SSTATUS
	li t1, SSTATUS_FS
	csrs CSR_SSTATUS, t1
	frcsr t0
	fsw f0,  RISCV_PRIV_FP_F_F0(a0)
	fsw f1,  RISCV_PRIV_FP_F_F1(a0)
	fsw f2,  RISCV_PRIV_FP_F_F2(a0)
	fsw f3,  RISCV_PRIV_FP_F_F3(a0)
	fsw f4,  RISCV_PRIV_FP_F_F4(a0)
	fsw f5,  RISCV_PRIV_FP_F_F5(a0)
	fsw f6,  RISCV_PRIV_FP_F_F6(a0)
	fsw f7,  RISCV_PRIV_FP_F_F7(a0)
	fsw f8,  RISCV_PRIV_FP_F_F8(a0)
	fsw f9,  RISCV_PRIV_FP_F_F9(a0)
	fsw f10, RISCV_PRIV_FP_F_F10(a0)
	fsw f11, RISCV_PRIV_FP_F_F11(a0)
	fsw f12, RISCV_PRIV_FP_F_F12(a0)
	fsw f13, RISCV_PRIV_FP_F_F13(a0)
	fsw f14, RISCV_PRIV_FP_F_F14(a0)
	fsw f15, RISCV_PRIV_FP_F_F15(a0)
	fsw f16, RISCV_PRIV_FP_F_F16(a0)
	fsw f17, RISCV_PRIV_FP_F_F17(a0)
	fsw f18, RISCV_PRIV_FP_F_F18(a0)
	fsw f19, RISCV_PRIV_FP_F_F19(a0)
	fsw f20, RISCV_PRIV_FP_F_F20(a0)
	fsw f21, RISCV_PRIV_FP_F_F21(a0)
	fsw f22, RISCV_PRIV_FP_F_F22(a0)
	fsw f23, RISCV_PRIV_FP_F_F23(a0)
	fsw f24, RISCV_PRIV_FP_F_F24(a0)
	fsw f25, RISCV_PRIV_FP_F_F25(a0)
	fsw f26, RISCV_PRIV_FP_F_F26(a0)
	fsw f27, RISCV_PRIV_FP_F_F27(a0)
	fsw f28, RISCV_PRIV_FP_F_F28(a0)
	fsw f29, RISCV_PRIV_FP_F_F29(a0)
	fsw f30, RISCV_PRIV_FP_F_F30(a0)
	fsw f31, RISCV_PRIV_FP_F_F31(a0)
	sw t0, RISCV_PRIV_FP_F_FCSR(a0)
	csrw CSR_SSTATUS, t2
	ret

	.align 3
	.global __cpu_vcpu_fp_d_save
__cpu_vcpu_fp_d_save:
	csrr t2, CSR_SSTATUS
	li t1, SSTATUS_FS
	csrs CSR_SSTATUS, t1
	frcsr t0
	fsd f0,  RISCV_PRIV_FP_D_F0(a0)
	fsd f1,  RISCV_PRIV_FP_D_F1(a0)
	fsd f2,  RISCV_PRIV_FP_D_F2(a0)
	fsd f3,  RISCV_PRIV_FP_D_F3(a0)
	fsd f4,  RISCV_PRIV_FP_D_F4(a0)
	fsd f5,  RISCV_PRIV_FP_D_F5(a0)
	fsd f6,  RISCV_PRIV_FP_D_F6(a0)
	fsd f7,  RISCV_PRIV_FP_D_F7(a0)
	fsd f8,  RISCV_PRIV_FP_D_F8(a0)
	fsd f9,  RISCV_PRIV_FP_D_F9(a0)
	fsd f10, RISCV_PRIV_FP_D_F10(a0)
	fsd f11, RISCV_PRIV_FP_D_F11(a0)
	fsd f12, RISCV_PRIV_FP_D_F12(a0)
	fsd f13, RISCV_PRIV_FP_D_F13(a0)
	fsd f14, RISCV_PRIV_FP_D_F14(a0)
	fsd f15, RISCV_PRIV_FP_D_F15(a0)
	fsd f16, RISCV_PRIV_FP_D_F16(a0)
	fsd f17, RISCV_PRIV_FP_D_F17(a0)
	fsd f18, RISCV_PRIV_FP_D_F18(a0)
	fsd f19, RISCV_PRIV_FP_D_F19(a0)
	fsd f20, RISCV_PRIV_FP_D_F20(a0)
	fsd f21, RISCV_PRIV_FP_D_F21(a0)
	fsd f22, RISCV_PRIV_FP_D_F22(a0)
	fsd f23, RISCV_PRIV_FP_D_F23(a0)
	fsd f24, RISCV_PRIV_FP_D_F24(a0)
	fsd f25, RISCV_PRIV_FP_D_F25(a0)
	fsd f26, RISCV_PRIV_FP_D_F26(a0)
	fsd f27, RISCV_PRIV_FP_D_F27(a0)
	fsd f28, RISCV_PRIV_FP_D_F28(a0)
	fsd f29, RISCV_PRIV_FP_D_F29(a0)
	fsd f30, RISCV_PRIV_FP_D_F30(a0)
	fsd f31, RISCV_PRIV_FP_D_F31(a0)
	sw t0, RISCV_PRIV_FP_D_FCSR(a0)
	csrw CSR_SSTATUS, t2
	ret

	.align 3
	.global __cpu_vcpu_fp_f_restore
__cpu_vcpu_fp_f_restore:
	csrr t2, CSR_SSTATUS
	li t1, SSTATUS_FS
	lw t0, RISCV_PRIV_FP_F_FCSR(a0)
	csrs CSR_SSTATUS, t1
	flw f0,  RISCV_PRIV_FP_F_F0(a0)
	flw f1,  RISCV_PRIV_FP_F_F1(a0)
	flw f2,  RISCV_PRIV_FP_F_F2(a0)
	flw f3,  RISCV_PRIV_FP_F_F3(a0)
	flw f4,  RISCV_PRIV_FP_F_F4(a0)
	flw f5,  RISCV_PRIV_FP_F_F5(a0)
	flw f6,  RISCV_PRIV_FP_F_F6(a0)
	flw f7,  RISCV_PRIV_FP_F_F7(a0)
	flw f8,  RISCV_PRIV_FP_F_F8(a0)
	flw f9,  RISCV_PRIV_FP_F_F9(a0)
	flw f10, RISCV_PRIV_FP_F_F10(a0)
	flw f11, RISCV_PRIV_FP_F_F11(a0)
	flw f12, RISCV_PRIV_FP_F_F12(a0)
	flw f13, RISCV_PRIV_FP_F_F13(a0)
	flw f14, RISCV_PRIV_FP_F_F14(a0)
	flw f15, RISCV_PRIV_FP_F_F15(a0)
	flw f16, RISCV_PRIV_FP_F_F16(a0)
	flw f17, RISCV_PRIV_FP_F_F17(a0)
	flw f18, RISCV_PRIV_FP_F_F18(a0)
	flw f19, RISCV_PRIV_FP_F_F19(a0)
	flw f20, RISCV_PRIV_FP_F_F20(a0)
	flw f21, RISCV_PRIV_FP_F_F21(a0)
	flw f22, RISCV_PRIV_FP_F_F22(a0)
	flw f23, RISCV_PRIV_FP_F_F23(a0)
	flw f24, RISCV_PRIV_FP_F_F24(a0)
	flw f25, RISCV_PRIV_FP_F_F25(a0)
	flw f26, RISCV_PRIV_FP_F_F26(a0)
	flw f27, RISCV_PRIV_FP_F_F27(a0)
	flw f28, RISCV_PRIV_FP_F_F28(a0)
	flw f29, RISCV_PRIV_FP_F_F29(a0)
	flw f30, RISCV_PRIV_FP_F_F30(a0)
	flw f31, RISCV_PRIV_FP_F_F31(a0)
	fscsr t0
	csrw CSR_SSTATUS, t2
	ret

	.align 3
	.global __cpu_vcpu_fp_d_restore
__cpu_vcpu_fp_d_restore:
	csrr t2, CSR_SSTATUS
	li t1, SSTATUS_FS
	lw t0, RISCV_PRIV_FP_D_FCSR(a0)
	csrs CSR_SSTATUS, t1
	fld f0,  RISCV_PRIV_FP_D_F0(a0)
	fld f1,  RISCV_PRIV_FP_D_F1(a0)
	fld f2,  RISCV_PRIV_FP_D_F2(a0)
	fld f3,  RISCV_PRIV_FP_D_F3(a0)
	fld f4,  RISCV_PRIV_FP_D_F4(a0)
	fld f5,  RISCV_PRIV_FP_D_F5(a0)
	fld f6,  RISCV_PRIV_FP_D_F6(a0)
	fld f7,  RISCV_PRIV_FP_D_F7(a0)
	fld f8,  RISCV_PRIV_FP_D_F8(a0)
	fld f9,  RISCV_PRIV_FP_D_F9(a0)
	fld f10, RISCV_PRIV_FP_D_F10(a0)
	fld f11, RISCV_PRIV_FP_D_F11(a0)
	fld f12, RISCV_PRIV_FP_D_F12(a0)
	fld f13, RISCV_PRIV_FP_D_F13(a0)
	fld f14, RISCV_PRIV_FP_D_F14(a0)
	fld f15, RISCV_PRIV_FP_D_F15(a0)
	fld f16, RISCV_PRIV_FP_D_F16(a0)
	fld f17, RISCV_PRIV_FP_D_F17(a0)
	fld f18, RISCV_PRIV_FP_D_F18(a0)
	fld f19, RISCV_PRIV_FP_D_F19(a0)
	fld f20, RISCV_PRIV_FP_D_F20(a0)
	fld f21, RISCV_PRIV_FP_D_F21(a0)
	fld f22, RISCV_PRIV_FP_D_F22(a0)
	fld f23, RISCV_PRIV_FP_D_F23(a0)
	fld f24, RISCV_PRIV_FP_D_F24(a0)
	fld f25, RISCV_PRIV_FP_D_F25(a0)
	fld f26, RISCV_PRIV_FP_D_F26(a0)
	fld f27, RISCV_PRIV_FP_D_F27(a0)
	fld f28, RISCV_PRIV_FP_D_F28(a0)
	fld f29, RISCV_PRIV_FP_D_F29(a0)
	fld f30, RISCV_PRIV_FP_D_F30(a0)
	fld f31, RISCV_PRIV_FP_D_F31(a0)
	fscsr t0
	csrw CSR_SSTATUS, t2
	ret
