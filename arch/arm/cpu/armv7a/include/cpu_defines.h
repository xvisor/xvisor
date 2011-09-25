/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file cpu_defines.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief common macros & defines for shared by all C & Assembly code
 */
#ifndef __CPU_DEFINES_H__
#define __CPU_DEFINES_H__

/* Coprocessor related macros & defines */
#define CPU_COPROC_COUNT				16

/* GPR related macros & defines */
#define CPU_GPR_COUNT 					13
#define CPU_FIQ_GPR_COUNT 				5

/* Interrupt or Exception related macros & defines */
#define CPU_IRQ_NR					8
#define CPU_IRQ_LOWVEC_BASE				0x00000000
#define CPU_IRQ_HIGHVEC_BASE				0xFFFF0000
#define CPU_RESET_IRQ					0
#define CPU_UNDEF_INST_IRQ				1
#define CPU_SOFT_IRQ					2
#define CPU_PREFETCH_ABORT_IRQ				3
#define CPU_DATA_ABORT_IRQ				4
#define CPU_NOT_USED_IRQ				5
#define CPU_EXTERNAL_IRQ				6
#define CPU_EXTERNAL_FIQ				7

/* CPSR related macors & defines */
#define CPSR_VALIDBITS_MASK				0xFF0FFFFF
#define CPSR_USERBITS_MASK				0xFFFFFC00
#define CPSR_USERBITS_SHIFT				10
#define CPSR_PRIVBITS_MASK				0x000003FF
#define CPSR_PRIVBITS_SHIFT				0
#define CPSR_MODE_MASK					0x0000001f
#define CPSR_MODE_USER					0x00000010
#define CPSR_MODE_FIQ					0x00000011
#define CPSR_MODE_IRQ					0x00000012
#define CPSR_MODE_SUPERVISOR				0x00000013
#define CPSR_MODE_MONITOR				0x00000016
#define CPSR_MODE_ABORT					0x00000017
#define CPSR_MODE_UNDEFINED				0x0000001b
#define CPSR_MODE_SYSTEM				0x0000001f
#define CPSR_THUMB_ENABLED				(1 << 5)
#define CPSR_FIQ_DISABLED				(1 << 6)
#define CPSR_IRQ_DISABLED				(1 << 7)
#define CPSR_ASYNC_ABORT_DISABLED			(1 << 8)
#define CPSR_BE_ENABLED					(1 << 9)
#define CPSR_IT2_MASK					0x0000FC00
#define CPSR_IT2_SHIFT					10
#define CPSR_GE_MASK					0x000F0000
#define CPSR_GE_SHIFT					16
#define CPSR_JAZZLE_ENABLED				(1 << 24)
#define CPSR_IT1_MASK					0x06000000
#define CPSR_IT1_SHIFT					25
#define CPSR_COND_OVERFLOW_MASK				(1 << 28)
#define CPSR_COND_OVERFLOW_SHIFT			28
#define CPSR_COND_CARRY_MASK				(1 << 29)
#define CPSR_COND_CARRY_SHIFT				29
#define CPSR_COND_ZERO_MASK				(1 << 30)
#define CPSR_COND_ZERO_SHIFT				30
#define CPSR_COND_NEGATIVE_MASK				(1 << 31)
#define CPSR_COND_NEGATIVE_SHIFT			31

/* VFP system registers.  */
#define VFP_FPSID					0
#define VFP_FPSCR					1
#define VFP_MVFR1					6
#define VFP_MVFR0					7
#define VFP_FPEXC					8
#define VFP_FPINST					9
#define VFP_FPINST2					10

/* iwMMXt coprocessor control registers.  */
#define IWMMXT_wCID					0
#define IWMMXT_wCon					1
#define IWMMXT_wCSSF					2
#define IWMMXT_wCASF					3
#define IWMMXT_wCGR0					8
#define IWMMXT_wCGR1					9
#define IWMMXT_wCGR2					10
#define IWMMXT_wCGR3					11

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

/* Translation table related macors & defines */
#define TTBL_MIN_SIZE					0x1000
#define TTBL_MIN_PAGE_SIZE				0x1000
#define TTBL_MAX_SIZE					0x4000
#define TTBL_MAX_PAGE_SIZE				0x1000000
#define TTBL_AP_S_U					0x0
#define TTBL_AP_SRW_U					0x1
#define TTBL_AP_SRW_UR					0x2
#define TTBL_AP_SRW_URW					0x3
#define TTBL_AP_SR_U					0x5
#define TTBL_AP_SR_UR					0x7
#define TTBL_DOM_MANAGER				0x3
#define TTBL_DOM_RESERVED				0x2
#define TTBL_DOM_CLIENT					0x1
#define TTBL_DOM_NOACCESS				0x0
#define TTBL_L1TBL_SIZE					0x4000
#define TTBL_L1TBL_SECTION_PAGE_SIZE			0x100000
#define TTBL_L1TBL_SUPSECTION_PAGE_SIZE			0x1000000
#define TTBL_L1TBL_TTE_OFFSET_MASK			0xFFF00000
#define TTBL_L1TBL_TTE_OFFSET_SHIFT			20
#define TTBL_L1TBL_TTE_BASE24_MASK			0xFF000000
#define TTBL_L1TBL_TTE_BASE24_SHIFT			24
#define TTBL_L1TBL_TTE_BASE20_MASK			0xFFF00000
#define TTBL_L1TBL_TTE_BASE20_SHIFT			20
#define TTBL_L1TBL_TTE_BASE10_MASK			0xFFFFFC00
#define TTBL_L1TBL_TTE_BASE10_SHIFT			10
#define TTBL_L1TBL_TTE_NS2_MASK				0x00080000
#define TTBL_L1TBL_TTE_NS2_SHIFT			19
#define TTBL_L1TBL_TTE_SECTYPE_MASK			0x00040000
#define TTBL_L1TBL_TTE_SECTYPE_SHIFT			18
#define TTBL_L1TBL_TTE_NG_MASK				0x00020000
#define TTBL_L1TBL_TTE_NG_SHIFT				17
#define TTBL_L1TBL_TTE_S_MASK				0x00010000
#define TTBL_L1TBL_TTE_S_SHIFT				16
#define TTBL_L1TBL_TTE_AP2_MASK				0x00008000
#define TTBL_L1TBL_TTE_AP2_SHIFT			15
#define TTBL_L1TBL_TTE_TEX_MASK				0x00007000
#define TTBL_L1TBL_TTE_TEX_SHIFT			12
#define TTBL_L1TBL_TTE_AP_MASK				0x00000C00
#define TTBL_L1TBL_TTE_AP_SHIFT				10
#define TTBL_L1TBL_TTE_IMP_MASK				0x00000200
#define TTBL_L1TBL_TTE_IMP_SHIFT			9
#define TTBL_L1TBL_TTE_DOM_MASK				0x000001E0
#define TTBL_L1TBL_TTE_DOM_SHIFT			5
#define TTBL_L1TBL_TTE_DOM_RESERVED			0x0
#define TTBL_L1TBL_TTE_DOM_VCPU_NOMMU			0x1
#define TTBL_L1TBL_TTE_DOM_VCPU_SUPER			0x2
#define TTBL_L1TBL_TTE_DOM_VCPU_USER			0x3
#define TTBL_L1TBL_TTE_XN_MASK				0x00000010
#define TTBL_L1TBL_TTE_XN_SHIFT				4
#define TTBL_L1TBL_TTE_NS1_MASK				0x00000008
#define TTBL_L1TBL_TTE_NS1_SHIFT			3
#define TTBL_L1TBL_TTE_C_MASK				0x00000008
#define TTBL_L1TBL_TTE_C_SHIFT				3
#define TTBL_L1TBL_TTE_B_MASK				0x00000004
#define TTBL_L1TBL_TTE_B_SHIFT				2
#define TTBL_L1TBL_TTE_TYPE_MASK			0x00000003
#define TTBL_L1TBL_TTE_TYPE_SHIFT			0
#define TTBL_L1TBL_TTE_TYPE_FAULT			0x0
#define TTBL_L1TBL_TTE_TYPE_L2TBL			0x1
#define TTBL_L1TBL_TTE_TYPE_SECTION			0x2
#define TTBL_L1TBL_TTE_TYPE_RESERVED			0x3
#define TTBL_L2TBL_SIZE					0x400
#define TTBL_L2TBL_LARGE_PAGE_SIZE			0x10000
#define TTBL_L2TBL_SMALL_PAGE_SIZE			0x1000
#define TTBL_L2TBL_TTE_OFFSET_MASK			0x000FF000
#define TTBL_L2TBL_TTE_OFFSET_SHIFT			12
#define TTBL_L2TBL_TTE_BASE16_MASK			0xFFFF0000
#define TTBL_L2TBL_TTE_BASE16_SHIFT			16
#define TTBL_L2TBL_TTE_LXN_MASK				0x00008000
#define TTBL_L2TBL_TTE_LXN_SHIFT			15
#define TTBL_L2TBL_TTE_BASE12_MASK			0xFFFFF000
#define TTBL_L2TBL_TTE_BASE12_SHIFT			12
#define TTBL_L2TBL_TTE_LTEX_MASK			0x00007000
#define TTBL_L2TBL_TTE_LTEX_SHIFT			12
#define TTBL_L2TBL_TTE_NG_MASK				0x00000800
#define TTBL_L2TBL_TTE_NG_SHIFT				11
#define TTBL_L2TBL_TTE_S_MASK				0x00000400
#define TTBL_L2TBL_TTE_S_SHIFT				10
#define TTBL_L2TBL_TTE_AP2_MASK				0x00000200
#define TTBL_L2TBL_TTE_AP2_SHIFT			9
#define TTBL_L2TBL_TTE_STEX_MASK			0x000001C0
#define TTBL_L2TBL_TTE_STEX_SHIFT			6
#define TTBL_L2TBL_TTE_AP_MASK				0x00000030
#define TTBL_L2TBL_TTE_AP_SHIFT				4
#define TTBL_L2TBL_TTE_C_MASK				0x00000008
#define TTBL_L2TBL_TTE_C_SHIFT				3
#define TTBL_L2TBL_TTE_B_MASK				0x00000004
#define TTBL_L2TBL_TTE_B_SHIFT				2
#define TTBL_L2TBL_TTE_SXN_MASK				0x00000001
#define TTBL_L2TBL_TTE_SXN_SHIFT			0
#define TTBL_L2TBL_TTE_TYPE_MASK			0x00000003
#define TTBL_L2TBL_TTE_TYPE_SHIFT			0
#define TTBL_L2TBL_TTE_TYPE_FAULT			0x0
#define TTBL_L2TBL_TTE_TYPE_LARGE			0x1
#define TTBL_L2TBL_TTE_TYPE_SMALL_X			0x2
#define TTBL_L2TBL_TTE_TYPE_SMALL_XN			0x3

/* TTBR0 related macros & defines */
#define TTBR0_IGRN0_MASK				0x00000040
#define TTBR0_IRGN0_SHIFT				6
#define TTBR0_NOS_MASK					0x00000020
#define TTBR0_RGN_MASK					0x00000018
#define TTBR0_RGN_SHIFT					3
#define TTBR0_IMP_MASK					0x00000004
#define TTBR0_S_MASK					0x00000002
#define TTBR0_C_MASK					0x00000001
#define TTBR0_IGRN1_MASK				0x00000001

/* TTBR1 related macros & defines */
#define TTBR1_IGRN0_MASK				0x00000040
#define TTBR1_IRGN0_SHIFT				6
#define TTBR1_NOS_MASK					0x00000020
#define TTBR1_RGN_MASK					0x00000018
#define TTBR1_RGN_SHIFT					3
#define TTBR1_IMP_MASK					0x00000004
#define TTBR1_S_MASK					0x00000002
#define TTBR1_C_MASK					0x00000001
#define TTBR1_IGRN1_MASK				0x00000001

/* TTBCR related macros & defines */
#define TTBCR_PD1_MASK					0x00000020
#define TTBCR_PD2_MASK					0x00000010
#define TTBCR_N_MASK					0x00000007

/* IFSR related macros & defines */
#define IFSR_EXT_MASK					0x00001000
#define IFSR_EXT_SHIFT					12
#define IFSR_FS4_MASK					0x00000400
#define IFSR_FS4_SHIFT					10
#define IFSR_FS_MASK					0x0000000F
#define IFSR_FS_SHIFT					0
#define IFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_1		12
#define IFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_2		14
#define IFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_1		28
#define IFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_2		30
#define IFSR_FS_TRANS_FAULT_SECTION			5
#define IFSR_FS_TRANS_FAULT_PAGE			7
#define IFSR_FS_ACCESS_FAULT_SECTION			3
#define IFSR_FS_ACCESS_FAULT_PAGE			6
#define IFSR_FS_DOMAIN_FAULT_SECTION			9
#define IFSR_FS_DOMAIN_FAULT_PAGE			11
#define IFSR_FS_PERM_FAULT_SECTION			13
#define IFSR_FS_PERM_FAULT_PAGE				15
#define IFSR_FS_DEBUG_EVENT				2
#define IFSR_FS_SYNC_EXT_ABORT				8
#define IFSR_FS_IMP_VALID_LOCKDOWN			20
#define IFSR_FS_IMP_VALID_COPROC_ABORT			26
#define IFSR_FS_MEM_ACCESS_SYNC_PARITY_ERROR		25

/* DFSR related macros & defines */
#define DFSR_EXT_MASK					0x00001000
#define DFSR_EXT_SHIFT					12
#define DFSR_WNR_MASK					0x00000800
#define DFSR_WNR_SHIFT					11
#define DFSR_FS4_MASK					0x00000400
#define DFSR_FS4_SHIFT					10
#define DFSR_DOM_MASK					0x000000F0
#define DFSR_DOM_SHIFT					4
#define DFSR_FS_MASK					0x0000000F
#define DFSR_FS_SHIFT					0
#define DFSR_FS_ALIGN_FAULT				1
#define DFSR_FS_ICACHE_MAINT_FAULT			4
#define DFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_1		12
#define DFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_2		14
#define DFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_1		28
#define DFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_2		30
#define DFSR_FS_TRANS_FAULT_SECTION			5
#define DFSR_FS_TRANS_FAULT_PAGE			7
#define DFSR_FS_ACCESS_FAULT_SECTION			3
#define DFSR_FS_ACCESS_FAULT_PAGE			6
#define DFSR_FS_DOMAIN_FAULT_SECTION			9
#define DFSR_FS_DOMAIN_FAULT_PAGE			11
#define DFSR_FS_PERM_FAULT_SECTION			13
#define DFSR_FS_PERM_FAULT_PAGE				15
#define DFSR_FS_DEBUG_EVENT				2
#define DFSR_FS_SYNC_EXT_ABORT				8
#define DFSR_FS_IMP_VALID_LOCKDOWN			20
#define DFSR_FS_IMP_VALID_COPROC_ABORT			26
#define DFSR_FS_MEM_ACCESS_SYNC_PARITY_ERROR		25
#define DFSR_FS_ASYNC_EXT_ABORT				22
#define DFSR_FS_MEM_ACCESS_ASYNC_PARITY_ERROR		24

#endif
