/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief common macros & defines for shared by all C & Assembly code
 */
#ifndef __CPU_DEFINES_H__
#define __CPU_DEFINES_H__

/* Maximum allowed VTLB entries */
#define CPU_VCPU_VTLB_LINE_MASK				0x00000000
#define CPU_VCPU_VTLB_LINE_SHIFT			0
#define CPU_VCPU_VTLB_LINE_COUNT			1
#define CPU_VCPU_VTLB_LINE_ENTRY_COUNT			128
#define CPU_VCPU_VTLB_ENTRY_COUNT			128

/* Coprocessor related macros & defines */
#define CPU_COPROC_COUNT				16

/* GPR related macros & defines */
#define CPU_GPR_COUNT 					30

/* Interrupt or Exception related macros & defines */
#define EXC_HYP_SYNC_SP0			0
#define EXC_HYP_IRQ_SP0				1
#define EXC_HYP_FIQ_SP0				2
#define EXC_HYP_SERROR_SP0			3
#define EXC_HYP_SYNC_SPx			4
#define EXC_HYP_IRQ_SPx				5
#define EXC_HYP_FIQ_SPx				6
#define EXC_HYP_SERROR_SPx			7
#define EXC_GUEST_SYNC_A64			8
#define EXC_GUEST_IRQ_A64			9
#define EXC_GUEST_FIQ_A64			10
#define EXC_GUEST_SERROR_A64			11
#define EXC_GUEST_SYNC_A32			12
#define EXC_GUEST_IRQ_A32			13
#define EXC_GUEST_FIQ_A32			14
#define EXC_GUEST_SERROR_A32			15

/** Placeholder interrupt/exceptions for arm_emulate */
#define CPU_IRQ_LOWVEC_BASE			0x00000000
#define CPU_IRQ_HIGHVEC_BASE			0xFFFF0000
#define CPU_RESET_IRQ				0
#define CPU_UNDEF_INST_IRQ			1
#define CPU_SOFT_IRQ				2
#define CPU_PREFETCH_ABORT_IRQ			3
#define CPU_DATA_ABORT_IRQ			4
#define CPU_HYP_TRAP_IRQ			5
#define CPU_EXTERNAL_IRQ			6
#define CPU_EXTERNAL_FIQ			7
#define CPU_IRQ_NR				8

/* PSR related macros & defines */
#define PSR_MODE_MASK				0x0000001f

#define PSR_EL_MASK				0x0000000C
#define PSR_EL_0				0x00000000
#define PSR_EL_1				0x00000004
#define PSR_EL_2				0x00000008
#define PSR_EL_3				0x0000000C
#define PSR_MODE64_MASK				0x0000000f
#define PSR_MODE64_EL0t				0x00000000
#define PSR_MODE64_EL1t				0x00000004
#define PSR_MODE64_EL1h				0x00000005
#define PSR_MODE64_EL2t				0x00000008
#define PSR_MODE64_EL2h				0x00000009
#define PSR_MODE64_EL3t				0x0000000c
#define PSR_MODE64_EL3h				0x0000000d

#define PSR_MODE32				0x00000010
#define PSR_MODE32_MASK				0x0000001f
#define PSR_MODE32_USER				0x00000010
#define PSR_MODE32_FIQ				0x00000011
#define PSR_MODE32_IRQ				0x00000012
#define PSR_MODE32_SUPERVISOR			0x00000013
#define PSR_MODE32_MONITOR			0x00000016
#define PSR_MODE32_ABORT			0x00000017
#define PSR_MODE32_HYPERVISOR			0x0000001a
#define PSR_MODE32_UNDEFINED			0x0000001b
#define PSR_MODE32_SYSTEM			0x0000001f
#define PSR_FIQ_DISABLED			(1 << 6)
#define PSR_IRQ_DISABLED			(1 << 7)
#define PSR_ASYNC_ABORT_DISABLED		(1 << 8)
#define PSR_MODE64_DEBUG_DISABLED		(1 << 9)
#define PSR_MODE32_BE_ENABLED			(1 << 9)
#define PSR_IL_MASK				0x00100000
#define PSR_IL_SHIFT				20
#define PSR_SS_MASK				0x00200000
#define PSR_SS_SHIFT				21
#define PSR_OVERFLOW_MASK			(1 << 28)
#define PSR_OVERFLOW_SHIFT			28
#define PSR_CARRY_MASK				(1 << 29)
#define PSR_CARRY_SHIFT				29
#define PSR_ZERO_MASK				(1 << 30)
#define PSR_ZERO_SHIFT				30
#define PSR_NEGATIVE_MASK			(1 << 31)
#define PSR_NEGATIVE_SHIFT			31

/* Fields of AArch32-PSR which will be RES0 in Aarch64 */
/* Section 3.8.8 Unused fields of SPSR - Exception Model */
#define PSR_THUMB_ENABLED			(1 << 5)
#define PSR_IT2_MASK				0x0000FC00
#define PSR_IT2_SHIFT				10
#define PSR_GE_MASK				0x000F0000
#define PSR_GE_SHIFT				16
#define PSR_JAZZLE_ENABLED			(1 << 24)
#define PSR_IT1_MASK				0x06000000
#define PSR_IT1_SHIFT				25
#define PSR_CUMMULATE_MASK			(1 << 27)
#define PSR_CUMMULATE_SHIFT			27

#define PSR_NZCV_MASK				(PSR_NEGATIVE_MASK |\
						PSR_ZERO_MASK |\
						PSR_CARRY_MASK |\
						PSR_OVERFLOW_MASK)
#define PSR_IT_MASK				(PSR_IT2_MASK |\
						PSR_IT1_MASK)
#define PSR_USERBITS_MASK			(PSR_NZCV_MASK |\
						PSR_CUMMULATE_MASK |\
						PSR_GE_MASK |\
						PSR_IT_MASK |\
						PSR_THUMB_ENABLED)
#define PSR_PRIVBITS_MASK			(~PSR_USERBITS_MASK)
#define PSR_ALLBITS_MASK			0xFFFFFFFF

#define CPSR_MODE_MASK				PSR_MODE32_MASK	
#define CPSR_MODE_USER				PSR_MODE32_USER
#define CPSR_MODE_FIQ				PSR_MODE32_FIQ
#define CPSR_MODE_IRQ				PSR_MODE32_IRQ
#define CPSR_MODE_SUPERVISOR			PSR_MODE32_SUPERVISOR
#define CPSR_MODE_MONITOR			PSR_MODE32_MONITOR
#define CPSR_MODE_ABORT				PSR_MODE32_ABORT
#define CPSR_MODE_HYPERVISOR			PSR_MODE32_HYPERVISOR
#define CPSR_MODE_UNDEFINED			PSR_MODE32_UNDEFINED
#define CPSR_MODE_SYSTEM			PSR_MODE32_SYSTEM
#define CPSR_THUMB_ENABLED			PSR_THUMB_ENABLED
#define CPSR_FIQ_DISABLED			PSR_FIQ_DISABLED
#define CPSR_IRQ_DISABLED			PSR_IRQ_DISABLED
#define CPSR_ASYNC_ABORT_DISABLED		PSR_ASYNC_ABORT_DISABLED
#define CPSR_BE_ENABLED				PSR_MODE32_BE_ENABLED
#define CPSR_IT2_MASK				PSR_IT2_MASK		
#define CPSR_IT2_SHIFT			        PSR_IT2_SHIFT		
#define CPSR_GE_MASK			        PSR_GE_MASK		
#define CPSR_GE_SHIFT			        PSR_GE_SHIFT		
#define CPSR_JAZZLE_ENABLED		        PSR_JAZZLE_ENABLED	
#define CPSR_IT1_MASK			        PSR_IT1_MASK		
#define CPSR_IT1_SHIFT			        PSR_IT1_SHIFT		
#define CPSR_CUMMULATE_MASK		        PSR_CUMMULATE_MASK	
#define CPSR_CUMMULATE_SHIFT		        PSR_CUMMULATE_SHIFT	
#define CPSR_OVERFLOW_MASK		        PSR_OVERFLOW_MASK	
#define CPSR_OVERFLOW_SHIFT		        PSR_OVERFLOW_SHIFT	
#define CPSR_CARRY_MASK			        PSR_CARRY_MASK		
#define CPSR_CARRY_SHIFT		        PSR_CARRY_SHIFT	
#define CPSR_ZERO_MASK			        PSR_ZERO_MASK		
#define CPSR_ZERO_SHIFT			        PSR_ZERO_SHIFT		
#define CPSR_NEGATIVE_MASK		        PSR_NEGATIVE_MASK	
#define CPSR_NEGATIVE_SHIFT		        PSR_NEGATIVE_SHIFT	

#define CPSR_NZCV_MASK				PSR_NZCV_MASK
#define CPSR_IT_MASK				PSR_IT_MASK	
#define CPSR_USERBITS_MASK			PSR_USERBITS_MASK
#define CPSR_PRIVBITS_MASK			PSR_PRIVBITS_MASK
#define CPSR_ALLBITS_MASK			PSR_ALLBITS_MASK

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

/* MPIDR related macros & defines */
#define MPIDR_HWID_BITMASK				0xFF00FFFFFF
#define MPIDR_INVALID					(~MPIDR_HWID_BITMASK)
#define MPIDR_LEVEL_BITS_SHIFT				3
#define MPIDR_LEVEL_BITS				\
					(1 << MPIDR_LEVEL_BITS_SHIFT)
#define MPIDR_LEVEL_MASK				\
					((1 << MPIDR_LEVEL_BITS) - 1)
#define MPIDR_LEVEL_SHIFT(level)			\
			(((1 << level) >> 1) << MPIDR_LEVEL_BITS_SHIFT)
#define MPIDR_AFFINITY_LEVEL(mpidr, level)		\
		((mpidr >> MPIDR_LEVEL_SHIFT(level)) & MPIDR_LEVEL_MASK)

/* CTR related macros & defines */
#define CTR_FORMAT_MASK					0xE0000000
#define CTR_FORMAT_SHIFT				29
#define CTR_FORMAT_V6					0x0
#define CTR_FORMAT_V7					0x4
#define CTR_CWG_MASK					0x0F000000
#define CTR_CWG_SHIFT					24
#define CTR_ERG_MASK					0x00F00000
#define CTR_ERG_SHIFT					20
#define CTR_DMINLINE_MASK				0x000F0000
#define CTR_DMINLINE_SHIFT				20
#define CTR_L1IP_MASK					0x0000C000
#define CTR_L1IP_SHIFT					14
#define CTR_IMINLINE_MASK				0x0000000F
#define CTR_IMINLINE_SHIFT				0
#define CTR_V6_CTYPE_MASK				0x1E000000
#define CTR_V6_CTYPE_SHIFT				25
#define CTR_V6_S_MASK					0x01000000
#define CTR_V6_S_SHIFT					24
#define CTR_V6_DSIZE_MASK				0x00FFF000
#define CTR_V6_DSIZE_SHIFT				12
#define CTR_V6_ISIZE_MASK				0x00000FFF
#define CTR_V6_ISIZE_SHIFT				0

/* CLIDR related macros & defines */
#define CLIDR_LOUU_MASK					0x38000000
#define CLIDR_LOUU_SHIFT				27
#define CLIDR_LOC_MASK					0x07000000
#define CLIDR_LOC_SHIFT					24
#define CLIDR_LOUIS_MASK				0x00E00000
#define CLIDR_LOUIS_SHIFT				21
#define CLIDR_CTYPE7_MASK				0x001C0000
#define CLIDR_CTYPE7_SHIFT				18
#define CLIDR_CTYPE6_MASK				0x00038000
#define CLIDR_CTYPE6_SHIFT				15
#define CLIDR_CTYPE5_MASK				0x00007000
#define CLIDR_CTYPE5_SHIFT				12
#define CLIDR_CTYPE4_MASK				0x00000E00
#define CLIDR_CTYPE4_SHIFT				9
#define CLIDR_CTYPE3_MASK				0x000001C0
#define CLIDR_CTYPE3_SHIFT				6
#define CLIDR_CTYPE2_MASK				0x00000038
#define CLIDR_CTYPE2_SHIFT				3
#define CLIDR_CTYPE1_MASK				0x00000007
#define CLIDR_CTYPE1_SHIFT				0
#define CLIDR_CTYPE_NOCACHE				0x0
#define CLIDR_CTYPE_ICACHE				0x1
#define CLIDR_CTYPE_DCACHE				0x2
#define CLIDR_CTYPE_SPLITCACHE				0x3
#define CLIDR_CTYPE_UNICACHE				0x4
#define CLIDR_CTYPE_RESERVED1				0x5
#define CLIDR_CTYPE_RESERVED2				0x6
#define CLIDR_CTYPE_RESERVED3				0x7

/* CSSELR related macros & defines */
#define CSSELR_LEVEL_MASK				0x0000000E
#define CSSELR_LEVEL_SHIFT				1
#define CSSELR_IND_MASK					0x00000001
#define CSSELR_IND_SHIFT				0

/* CSSIDR related macros & defines */
#define CCSIDR_WT_MASK					0x80000000
#define CCSIDR_WT_SHIFT					31
#define CCSIDR_WB_MASK					0x40000000
#define CCSIDR_WB_SHIFT					30
#define CCSIDR_RA_MASK					0x20000000
#define CCSIDR_RA_SHIFT					29
#define CCSIDR_WA_MASK					0x10000000
#define CCSIDR_WA_SHIFT					28
#define CCSIDR_NUMSETS_MASK				0x0FFFE000
#define CCSIDR_NUMSETS_SHIFT				13
#define CCSIDR_ASSOC_MASK				0x00001FF8
#define CCSIDR_ASSOC_SHIFT				3
#define CCSIDR_LINESZ_MASK				0x00000007
#define CCSIDR_LINESZ_SHIFT				0

/* HCR_EL2 */
#define HCR_INITVAL					0x000000000
#define HCR_ID_MASK					0x200000000
#define HCR_ID_SHIFT					33         
#define HCR_CD_MASK					0x100000000
#define HCR_CD_SHIFT					32         
#define HCR_RW_MASK					0x080000000
#define HCR_RW_SHIFT					31         
#define HCR_TRVM_MASK					0x040000000
#define HCR_TRVM_SHIFT					30         
#define HCR_HCD_MASK					0x020000000
#define HCR_HCD_SHIFT					29         
#define HCR_TDZ_MASK					0x010000000
#define HCR_TDZ_SHIFT					28         
#define HCR_TGE_MASK					0x008000000
#define HCR_TGE_SHIFT					27         
#define HCR_TVM_MASK					0x004000000
#define HCR_TVM_SHIFT					26         
#define HCR_TTLB_MASK					0x002000000
#define HCR_TTLB_SHIFT					25         
#define HCR_TPU_MASK					0x001000000
#define HCR_TPU_SHIFT					24         
#define HCR_TPC_MASK					0x000800000
#define HCR_TPC_SHIFT					23         
#define HCR_TSW_MASK					0x000400000
#define HCR_TSW_SHIFT					22         
#define HCR_TACR_MASK					0x000200000
#define HCR_TACR_SHIFT					21         
#define HCR_TIDCP_MASK					0x000100000
#define HCR_TIDCP_SHIFT					20         
#define HCR_TSC_MASK					0x000080000
#define HCR_TSC_SHIFT					19         
#define HCR_TID3_MASK					0x000040000
#define HCR_TID3_SHIFT					18         
#define HCR_TID2_MASK					0x000020000
#define HCR_TID2_SHIFT					17         
#define HCR_TID1_MASK					0x000010000
#define HCR_TID1_SHIFT					16         
#define HCR_TID0_MASK					0x000008000
#define HCR_TID0_SHIFT					15         
#define HCR_TWE_MASK					0x000004000
#define HCR_TWE_SHIFT					14         
#define HCR_TWI_MASK					0x000002000
#define HCR_TWI_SHIFT					13         
#define HCR_DC_MASK					0x000001000
#define HCR_DC_SHIFT					12         
#define HCR_BSU_MASK					0x000000C00
#define HCR_BSU_SHIFT					10         
#define HCR_FB_MASK					0x000000200
#define HCR_FB_SHIFT					9          
#define HCR_VSE_MASK					0x000000100
#define HCR_VSE_SHIFT					8          
#define HCR_VI_MASK					0x000000080
#define HCR_VI_SHIFT					7          
#define HCR_VF_MASK					0x000000040
#define HCR_VF_SHIFT					6          
#define HCR_AMO_MASK					0x000000020
#define HCR_AMO_SHIFT					5          
#define HCR_IMO_MASK					0x000000010
#define HCR_IMO_SHIFT					4          
#define HCR_FMO_MASK					0x000000008
#define HCR_FMO_SHIFT					3          
#define HCR_PTW_MASK					0x000000004
#define HCR_PTW_SHIFT					2          
#define HCR_SWIO_MASK					0x000000002
#define HCR_SWIO_SHIFT					1          
#define HCR_VM_MASK					0x000000001
#define HCR_VM_SHIFT					0

/* CPTR_EL2 */
#define CPTR_INITVAL					0x00000000LU
#define CPTR_TCPAC_MASK					0x80000000LU
#define CPTR_TCPAC_SHIFT				31
#define CPTR_TTA_MASK					0x00100000LU
#define CPTR_TTA_SHIFT					20
#define CPTR_TFP_MASK					0x00000400LU
#define CPTR_TFP_SHIFT					10
#define CPTR_RES1_MASK					0x000033FFLU

/* HSTR_EL2 */
#define HSTR_INITVAL					0x00000000LU
#define HSTR_TTEE_MASK					0x00010000LU
#define HSTR_TTEE_SHIFT					16
#define HSTR_T_MASK					0x0000BFEFLU
#define HSTR_T_SHIFT					0
#define HSTR_T15_MASK					0x00008000LU
#define HSTR_T15_SHIFT					15
#define HSTR_T13_MASK					0x00002000LU
#define HSTR_T13_SHIFT					13
#define HSTR_T12_MASK					0x00001000LU
#define HSTR_T12_SHIFT					12
#define HSTR_T11_MASK					0x00000800LU
#define HSTR_T11_SHIFT					11
#define HSTR_T10_MASK					0x00000400LU
#define HSTR_T10_SHIFT					10
#define HSTR_T9_MASK					0x00000200LU
#define HSTR_T9_SHIFT					9
#define HSTR_T8_MASK					0x00000100LU
#define HSTR_T8_SHIFT					8
#define HSTR_T7_MASK					0x00000080LU
#define HSTR_T7_SHIFT					7
#define HSTR_T6_MASK					0x00000040LU
#define HSTR_T6_SHIFT					6
#define HSTR_T5_MASK					0x00000020LU
#define HSTR_T5_SHIFT					5
#define HSTR_T3_MASK					0x00000008LU
#define HSTR_T3_SHIFT					3
#define HSTR_T2_MASK					0x00000004LU
#define HSTR_T2_SHIFT					2
#define HSTR_T1_MASK					0x00000002LU
#define HSTR_T1_SHIFT					1
#define HSTR_T0_MASK					0x00000001LU
#define HSTR_T0_SHIFT					0

/* HPFAR_EL2 */
#define HPFAR_INITVAL					0x00000000UL
#define HPFAR_FIPA_MASK					0xFFFFFFFFFFFFFFF0UL
#define HPFAR_FIPA_SHIFT				4
#define HPFAR_FIPA_PAGE_MASK				0x00000FFFUL
#define HPFAR_FIPA_PAGE_SHIFT				12

/* ESR_EL2 */
#define ESR_INITVAL					0x00000000LU
#define ESR_EC_MASK					0xFC000000LU
#define ESR_EC_SHIFT					26
#define ESR_IL_MASK					0x02000000LU
#define ESR_IL_SHIFT					25
#define ESR_ISS_MASK					0x01FFFFFFLU
#define ESR_ISS_SHIFT					0

/* Exception Class (EC) Values */
#define EC_UNKNOWN					0x00
#define EC_TRAP_WFI_WFE					0x01
#define EC_TRAP_MCR_MRC_CP15_A32			0x03
#define EC_TRAP_MCRR_MRRC_CP15_A32			0x04
#define EC_TRAP_MCR_MRC_CP14_A32			0x05
#define EC_TRAP_LDC_STC_CP14_A32			0x06
#define EC_SIMD_FPU					0x07
#define EC_TRAP_MRC_VMRS_CP10_A32			0x08
#define EC_TRAP_MCRR_MRRC_CP14_A32			0x0C
#define EC_TRAP_IL					0x0E
#define EC_TRAP_SVC_A32					0x11
#define EC_TRAP_HVC_A32					0x12
#define EC_TRAP_SMC_A32					0x13
#define EC_TRAP_SVC_A64					0x15
#define EC_TRAP_HVC_A64					0x16
#define EC_TRAP_SMC_A64					0x17
#define EC_TRAP_MSR_MRS_SYSTEM				0x18
#define EC_TRAP_LWREL_INST_ABORT			0x20
#define EC_CUREL_INST_ABORT				0x21
#define EC_PC_UNALIGNED					0x22
#define EC_TRAP_LWREL_DATA_ABORT			0x24
#define EC_CUREL_DATA_ABORT				0x25
#define EC_SP_UNALIGNED					0x26
#define EC_FPEXC_A32					0x28
#define EC_FPEXC_A64					0x2C
#define EC_SERROR					0x2F
#define EC_DBG_EXC_MASK					0x30

/* Condition Field ISS Encodings */
#define ISS_CV_MASK					0x01000000
#define ISS_CV_SHIFT					24
#define ISS_COND_MASK					0x00F00000
#define ISS_COND_SHIFT					20

/* MRS/MSR Trap ISS Encodings */
#define ISS_SYSREG_ENC(op0, op2, op1, crn, crm)	(((op0) << 20) | \
				((op2) << 17) | ((op1) << 14)  | \
		 		((crn) << 10) | ((crm) << 1))	 

#define ISS_SYSREG_MASK					0xfffffc1e
#define ISS_RT_MASK					0x000003e0
#define ISS_RT_SHIFT					5
#define ISS_DIR_MASK					0x00000001
#define ISS_SYSREG_READ					0x00000001

#define ISS_CPACR_EL1					ISS_SYSREG_ENC(3,2,0,1,0)
#define ISS_CNTFRQ_EL0					ISS_SYSREG_ENC(3,0,3,14,0)
#define ISS_CNTPCT_EL0					ISS_SYSREG_ENC(3,1,3,14,0)
#define ISS_CNTVCT_EL0					ISS_SYSREG_ENC(3,2,3,14,0)
#define ISS_CNTKCTL_EL1					ISS_SYSREG_ENC(3,0,0,14,1)
#define ISS_CNTP_TVAL_EL0				ISS_SYSREG_ENC(3,0,3,14,2)
#define ISS_CNTP_CTL_EL0				ISS_SYSREG_ENC(3,1,3,14,2)
#define ISS_CNTP_CVAL_EL0				ISS_SYSREG_ENC(3,2,3,14,2)
#define ISS_CNTV_TVAL_EL0				ISS_SYSREG_ENC(3,0,3,14,3)
#define ISS_CNTV_CTL_EL0				ISS_SYSREG_ENC(3,1,3,14,3)
#define ISS_CNTV_CVAL_EL0				ISS_SYSREG_ENC(3,2,3,14,3)
#define ISS_ACTLR_EL1					ISS_SYSREG_ENC(3,1,0,1,0)
#define ISS_ACTLR_EL2					ISS_SYSREG_ENC(3,1,0,1,4)
#define ISS_ACTLR_EL3					ISS_SYSREG_ENC(3,1,0,1,6)

/* WFI/WFE ISS Encodings */
#define ISS_WFI_WFE_TI_MASK				0x00000001
#define ISS_WFI_WFE_TI_SHIFT				0

/* MCR/MRC ISS Encodings */
#define ISS_MCR_MRC_OPC2_MASK				0x000E0000
#define ISS_MCR_MRC_OPC2_SHIFT				17
#define ISS_MCR_MRC_OPC1_MASK				0x0001C000
#define ISS_MCR_MRC_OPC1_SHIFT				14
#define ISS_MCR_MRC_CRN_MASK				0x00003C00
#define ISS_MCR_MRC_CRN_SHIFT				10
#define ISS_MCR_MRC_RT_MASK				0x000001E0
#define ISS_MCR_MRC_RT_SHIFT				5
#define ISS_MCR_MRC_CRM_MASK				0x0000001E
#define ISS_MCR_MRC_CRM_SHIFT				1
#define ISS_MCR_MRC_DIR_MASK				0x00000001
#define ISS_MCR_MRC_DIR_SHIFT				0

/* Instruction/Data Abort ISS Encodings */
#define ISS_ABORT_ISV_MASK				0x01000000
#define ISS_ABORT_ISV_SHIFT				24
#define ISS_ABORT_SAS_MASK				0x00C00000
#define ISS_ABORT_SAS_SHIFT				22
#define ISS_ABORT_SSE_MASK				0x00200000
#define ISS_ABORT_SSE_SHIFT				21
#define ISS_ABORT_SRT_MASK				0x001F0000
#define ISS_ABORT_SRT_SHIFT				16
#define ISS_ABORT_SF_MASK				0x00008000
#define ISS_ABORT_SF_SHIFT				15
#define ISS_ABORT_AR_MASK				0x00004000
#define ISS_ABORT_AR_SHIFT				14
#define ISS_ABORT_EA_MASK				0x00000200
#define ISS_ABORT_EA_SHIFT				9
#define ISS_ABORT_CM_MASK				0x00000100
#define ISS_ABORT_CM_SHIFT				8
#define ISS_ABORT_S1PTW_MASK				0x00000080
#define ISS_ABORT_S1PTW_SHIFT				7
#define ISS_ABORT_WNR_MASK				0x00000040
#define ISS_ABORT_WNR_SHIFT				6
#define ISS_ABORT_FSC_MASK				0x0000003F
#define ISS_ABORT_FSC_SHIFT				0

/* Fault Status (IFSC/DFSC) Encodings */
#define FSC_TRANS_FAULT_LEVEL0				0x04
#define FSC_TRANS_FAULT_LEVEL1				0x05
#define FSC_TRANS_FAULT_LEVEL2				0x06
#define FSC_TRANS_FAULT_LEVEL3				0x07
#define FSC_ACCESS_FAULT_LEVEL0				0x08
#define FSC_ACCESS_FAULT_LEVEL1				0x09
#define FSC_ACCESS_FAULT_LEVEL2				0x0A
#define FSC_ACCESS_FAULT_LEVEL3				0x0B
#define FSC_PERM_FAULT_LEVEL0				0x0C
#define FSC_PERM_FAULT_LEVEL1				0x0D
#define FSC_PERM_FAULT_LEVEL2				0x0E
#define FSC_PERM_FAULT_LEVEL3				0x0F
#define FSC_SYNC_EXTERNAL_ABORT				0x10
#define FSC_ASYNC_EXTERNAL_ABORT			0x11
#define FSC_SYNC_TWALK_EXTERNAL_ABORT_LEVEL0		0x14
#define FSC_SYNC_TWALK_EXTERNAL_ABORT_LEVEL1		0x15
#define FSC_SYNC_TWALK_EXTERNAL_ABORT_LEVEL2		0x16
#define FSC_SYNC_TWALK_EXTERNAL_ABORT_LEVEL3		0x17
#define FSC_SYNC_PARITY_ERROR				0x18
#define FSC_ASYNC_PARITY_ERROR				0x19
#define FSC_SYNC_TWALK_PARITY_ERROR_LEVEL0		0x1C
#define FSC_SYNC_TWALK_PARITY_ERROR_LEVEL1		0x1D
#define FSC_SYNC_TWALK_PARITY_ERROR_LEVEL2		0x1E
#define FSC_SYNC_TWALK_PARITY_ERROR_LEVEL3		0x1F
#define FSC_ALIGN_FAULT					0x21
#define FSC_DEBUG_EVENT					0x22
#define FSC_TLB_CONFLICT_ABORT				0x30
#define FSC_DOMAIN_FAULT_LEVEL0				0x3C
#define FSC_DOMAIN_FAULT_LEVEL1				0x3D
#define FSC_DOMAIN_FAULT_LEVEL2				0x3E
#define FSC_DOMAIN_FAULT_LEVEL3				0x3F

/* HTTBR */
#define HTTBR_INITVAL					0x0000000000000000ULL
#define HTTBR_BADDR_MASK				0x000000FFFFFFF000ULL
#define HTTBR_BADDR_SHIFT				12

/* TCR_EL2 */
#define TCR_INITVAL					0x80800000
#define TCR_TBI_MASK					0x00100000
#define TCR_TBI_SHIFT					20
#define TCR_PS_MASK					0x00070000
#define TCR_PS_SHIFT					16
#define TCR_TG0_MASK					0x00004000
#define TCR_TG0_SHIFT					14
#define TCR_SH0_MASK					0x00003000
#define TCR_SH0_SHIFT					12
#define TCR_ORGN0_MASK					0x00000C00
#define TCR_ORGN0_SHIFT					10
#define TCR_IRGN0_MASK					0x00000300
#define TCR_IRGN0_SHIFT					8
#define TCR_T0SZ_MASK					0x0000003f
#define TCR_T0SZ_SHIFT					0

#define TCR_PS_32BITS					(0 << TCR_PS_SHIFT)
#define TCR_PS_36BITS					(1 << TCR_PS_SHIFT)
#define TCR_PS_40BITS					(2 << TCR_PS_SHIFT)
#define TCR_PS_42BITS					(3 << TCR_PS_SHIFT)
#define TCR_PS_44BITS					(4 << TCR_PS_SHIFT)
#define TCR_PS_48BITS					(5 << TCR_PS_SHIFT)
#define TCR_T0SZ_VAL(in_bits)				((64 - (in_bits)) & TCR_T0SZ_MASK)

/* VTTBR */
#define VTTBR_INITVAL					0x0000000000000000ULL
#define VTTBR_VMID_MASK					0x00FF000000000000ULL
#define VTTBR_VMID_SHIFT				48
#define VTTBR_BADDR_MASK				0x000000FFFFFFF000ULL
#define VTTBR_BADDR_SHIFT				12

/* VTCR_EL2 */
#define VTCR_INITVAL					0x80000000
#define VTCR_PS_MASK					0x00070000
#define VTCR_PS_SHIFT					16
#define VTCR_TG0_MASK					0x00004000
#define VTCR_TG0_SHIFT					14
#define VTCR_SH0_MASK					0x00003000
#define VTCR_SH0_SHIFT					12
#define VTCR_ORGN0_MASK					0x00000C00
#define VTCR_ORGN0_SHIFT				10
#define VTCR_IRGN0_MASK					0x00000300
#define VTCR_IRGN0_SHIFT				8
#define VTCR_SL0_MASK					0x000000C0
#define VTCR_SL0_SHIFT					6
#define VTCR_T0SZ_MASK					0x0000003f
#define VTCR_T0SZ_SHIFT					0

#define VTCR_PS_32BITS					(0 << VTCR_PS_SHIFT)
#define VTCR_PS_36BITS					(1 << VTCR_PS_SHIFT)
#define VTCR_PS_40BITS					(2 << VTCR_PS_SHIFT)
#define VTCR_PS_42BITS					(3 << VTCR_PS_SHIFT)
#define VTCR_PS_44BITS					(4 << VTCR_PS_SHIFT)
#define VTCR_PS_48BITS					(5 << VTCR_PS_SHIFT)
#define VTCR_SL0_L2					(0 << VTCR_SL0_SHIFT) /* Starting-level: 2 */
#define VTCR_SL0_L1					(1 << VTCR_SL0_SHIFT) /* Starting-level: 1 */
#define VTCR_SL0_L0					(2 << VTCR_SL0_SHIFT) /* Starting-level: 0 */
#define VTCR_T0SZ_VAL(in_bits)				((64 - (in_bits)) & VTCR_T0SZ_MASK)

/* MAIR_EL2 encodings */
#define AINDEX_SO					0
#define AINDEX_NORMAL_WT				1
#define AINDEX_NORMAL_WB				2
#define AINDEX_NORMAL_UC				3
#define MAIR_INITVAL					0x0000000044FFBB00

/* VPIDR */
#define VPIDR_INITVAL					0x00000000

/* VMPIDR */
#define VMPIDR_INITVAL					0x00000000

/* DBGVIDSR */
#define DBGVIDSR_INITVAL				0x00000000
#define DBGVIDSR_NS_MASK				0x80000000
#define DBGVIDSR_NS_SHIFT				31
#define DBGVIDSR_H_MASK					0x40000000
#define DBGVIDSR_H_SHIFT				30
#define DBGVIDSR_VMID_MASK				0x000000FF
#define DBGVIDSR_VMID_SHIFT				0

/* SCTLR_ELx */
#define SCTLR_INITVAL					0x30C50878
#define SCTLR_TE_MASK					0x40000000
#define SCTLR_TE_SHIFT					30
#define SCTLR_EE_MASK					0x02000000
#define SCTLR_EE_SHIFT					25
#define SCTLR_FI_MASK					0x00200000
#define SCTLR_FI_SHIFT					21
#define SCTLR_WXN_MASK					0x00080000
#define SCTLR_WXN_SHIFT					19
#define SCTLR_I_MASK					0x00001000
#define SCTLR_I_SHIFT					12
#define SCTLR_SA0_MASK					0x00000010
#define SCTLR_SA0_SHIFT					4
#define SCTLR_SA_MASK					0x00000008
#define SCTLR_SA_SHIFT					3
#define SCTLR_C_MASK					0x00000004
#define SCTLR_C_SHIFT					2
#define SCTLR_A_MASK					0x00000002
#define SCTLR_A_SHIFT					1
#define SCTLR_M_MASK					0x00000001
#define SCTLR_M_SHIFT					0

/* DBGBXVR */
#define DBGBXVR_INITVAL					0x00000000
#define DBGBXVR_VMID_MASK				0x000000FF
#define DBGBXVR_VMID_SHIFT				0

/* PAR_EL1 common fields */
#define PAR_PA_MASK					0x0000FFFFFFFFF000ULL
#define PAR_PA_SHIFT					12
#define PAR_F_MASK					0x0000000000000001ULL
#define PAR_F_SHIFT					0

/* PAR_EL1 @ fault */
#define PAR_STAGE_MASK					0x0000000000000200ULL
#define PAR_STAGE_SHIFT					9
#define PAR_STAGE1					0
#define PAR_STAGE2					1
#define PAR_PTW_MASK					0x0000000000000100ULL
#define PAR_PTW_SHIFT					8
#define PAR_FS_MASK					0x000000000000007EULL
#define PAR_FS_SHIFT					1
#define PAR_F_FAULT					0x0000000000000001ULL

/* PAR_EL1 @ address-translation */
#define PAR_MAIR_MASK					0xFF00000000000000ULL
#define PAR_MAIR_SHIFT					56
#define PAR64_NS_MASK					0x0000000000000200ULL
#define PAR64_NS_SHIFT					9
#define PAR64_SH_MASK					0x0000000000000180ULL
#define PAR64_SH_SHIFT					7

/* MIDR */
#define MIDR_IMPLEMENTER_MASK				0xFF000000
#define MIDR_IMPLEMENTER_SHIFT				24
#define MIDR_VARIANT_MASK				0x00F00000
#define MIDR_VARIANT_SHIFT				20
#define MIDR_ARCHITECTURE_MASK				0x000F0000
#define MIDR_ARCHITECTURE_SHIFT				16
#define MIDR_PARTNUM_MASK				0x0000FFF0
#define MIDR_PARTNUM_SHIFT				4
#define MIDR_REVISON_MASK				0x0000000F
#define MIDR_REVISON_SHIFT				0

/* ID_PFR0_EL1 */
#define ID_PFR0_THUMBEE_MASK				0x0000f000
#define ID_PFR0_THUMBEE_SHIFT				12
#define ID_PFR0_JAZELLE_MASK				0x00000f00
#define ID_PFR0_JAZELLE_SHIFT				8
#define ID_PFR0_THUMB2					0x00000030
#define ID_PFR0_THUMB_MASK				0x000000f0
#define ID_PFR0_THUMB_SHIFT				4
#define ID_PFR0_ARM_MASK				0x0000000f
#define ID_PFR0_ARM_SHIFT				0

/* ID_PFR1_EL1 */
#define ID_PFR1_GENTIMER_MASK				0x000f0000
#define ID_PFR1_GENTIMER_SHIFT				16
#define ID_PFR1_VIRTEXT_MASK				0x0000f000
#define ID_PFR1_VIRTEXT_SHIFT				12
#define ID_PFR1_2STACK_MASK				0x00000f00
#define ID_PFR1_2STACK_SHIFT				8
#define ID_PFR1_SECURE_MASK				0x000000f0
#define ID_PFR1_SECURE_SHIFT				4
#define ID_PFR1_PROGMODEL_MASK				0x0000000f
#define ID_PFR1_PROGMODEL_SHIFT				0

/* ID_AA64PFR0_EL1 */
#define ID_AA64PFR0_ASIMD_MASK				0x00020000
#define ID_AA64PFR0_ASIMD_SHIFT				17
#define ID_AA64PFR0_FPU_MASK				0x00010000
#define ID_AA64PFR0_FPU_SHIFT				16
#define ID_AA64PFR0_EL3_A32				0x00002000
#define ID_AA64PFR0_EL3_MASK				0x0000f000
#define ID_AA64PFR0_EL3_SHIFT				12
#define ID_AA64PFR0_EL2_A32				0x00000200
#define ID_AA64PFR0_EL2_MASK				0x00000f00
#define ID_AA64PFR0_EL2_SHIFT				8
#define ID_AA64PFR0_EL1_A32				0x00000020
#define ID_AA64PFR0_EL1_MASK				0x000000f0
#define ID_AA64PFR0_EL1_SHIFT				4
#define ID_AA64PFR0_EL0_A32				0x00000002
#define ID_AA64PFR0_EL0_MASK				0x0000000f
#define ID_AA64PFR0_EL0_SHIFT				0

#endif
