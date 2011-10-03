/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file arm_regs.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief  register related macros & defines for ARM test code
 */
#ifndef __ARM_REGS_H__
#define __ARM_REGS_H__

/* CPSR related macros & defines */
#define CPSR_MODE_MASK			0x0000001f
#define CPSR_MODE_USER			0x00000010
#define CPSR_MODE_FIQ			0x00000011
#define CPSR_MODE_IRQ			0x00000012
#define CPSR_MODE_SUPERVISOR		0x00000013
#define CPSR_MODE_ABORT			0x00000017
#define CPSR_MODE_UNDEFINED		0x0000001b
#define CPSR_MODE_SYSTEM		0x0000001f

#define CPSR_THUMB_ENABLED		(1 << 5)
#define CPSR_FIQ_DISABLED		(1 << 6)
#define CPSR_IRQ_DISABLED		(1 << 7)
#define CPSR_ASYNC_ABORT_DISABLED	(1 << 8)
#define CPSR_BE_ENABLED			(1 << 9)

#define CPSR_COND_OVERFLOW		(1 << 28)
#define CPSR_COND_CARRY			(1 << 29)
#define CPSR_COND_ZERO			(1 << 30)
#define CPSR_COND_NEGATIVE		(1 << 31)

/* SCTLR related macros & defines */
#define SCTLR_TE_MASK					0x40000000
#define SCTLR_AFE_MASK					0x20000000
#define SCTLR_TRE_MASK					0x10000000
#define SCTLR_NFI_MASK					0x08000000
#define SCTLR_EE_MASK					0x02000000
#define SCTLR_VE_MASK					0x01000000
#define SCTLR_U_MASK					0x00400000
#define SCTLR_FI_MASK					0x00200000
#define SCTLR_HA_MASK					0x00020000
#define SCTLR_RR_MASK					0x00004000
#define SCTLR_V_MASK					0x00002000
#define SCTLR_I_MASK					0x00001000
#define SCTLR_Z_MASK					0x00000800
#define SCTLR_SW_MASK					0x00000400
#define SCTLR_B_MASK					0x00000080
#define SCTLR_C_MASK					0x00000004
#define SCTLR_A_MASK					0x00000002
#define SCTLR_M_MASK					0x00000001

#endif
