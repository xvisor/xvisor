/*
 * Copyright (c) 2009-2019 Stanislav Shwartsman
 * Copyright (c) 2021 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __VMCS_AUDITOR_H
#define __VMCS_AUDITOR_H

#define BX_SUPPORT_X86_64 1
#define BX_SUPPORT_VMX 2

#define BX_CPU_ID (0)

#define FMT_LL "%I64"
#define FMT_TICK "%011I64u"
#define FMT_ADDRX64 "%016I64x"
#define FMT_PHY_ADDRX64 "%012I64x"

typedef unsigned char      Bit8u;
typedef   signed char      Bit8s;
typedef unsigned short     Bit16u;
typedef   signed short     Bit16s;
typedef unsigned int       Bit32u;
typedef   signed int       Bit32s;
typedef unsigned long   Bit64u;
typedef   signed long   Bit64s;
typedef unsigned long uint64_t;

typedef Bit64u bx_phy_address;
typedef Bit32u bx_bool;

#if BX_SUPPORT_X86_64
typedef Bit64u bx_address;
#else
typedef Bit32u bx_address;
#endif

typedef unsigned char BOOLEAN;
typedef uint64_t UINT64;
typedef Bit32u UINT32;
typedef Bit32s INT32;

/////////////////////////////////////////////////////////////////////////
// $Id: vmx.h 13126 2017-03-17 17:35:15Z sshwarts $
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2009-2017 Stanislav Shwartsman
//          Written by Stanislav Shwartsman [sshwarts at sourceforge net]
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA B 02110-1301 USA
//
/////////////////////////////////////////////////////////////////////////

#ifndef _BX_VMX_INTEL_H_
#define _BX_VMX_INTEL_H_

#define VMX_VMCS_AREA_SIZE   4096


typedef struct bx_VMX_Cap
{
	//
	// VMX Capabilities
	//

	Bit32u vmx_pin_vmexec_ctrl_supported_bits;
	Bit32u vmx_proc_vmexec_ctrl_supported_bits;
	Bit32u vmx_vmexec_ctrl2_supported_bits;
	Bit32u vmx_vmexit_ctrl_supported_bits;
	Bit32u vmx_vmentry_ctrl_supported_bits;
#if BX_SUPPORT_VMX >= 2
	Bit64u vmx_ept_vpid_cap_supported_bits;
	Bit64u vmx_vmfunc_supported_bits;
#endif
} VMX_CAP;


#define BX_CONST64(x)  (x##ULL)

// VMCS pointer is always 64-bit variable
static const Bit64u BX_INVALID_VMCSPTR = BX_CONST64(0xFFFFFFFFFFFFFFFF);

#define POOLTAG 0x48564653 // [H]yper[V]isor [F]rom [S]cratch (HVFS)



// bits supported in IA32_FEATURE_CONTROL MSR
#define BX_IA32_FEATURE_CONTROL_LOCK_BIT       0x1
#define BX_IA32_FEATURE_CONTROL_VMX_ENABLE_BIT 0x4

#define BX_IA32_FEATURE_CONTROL_BITS \
   (BX_IA32_FEATURE_CONTROL_LOCK_BIT | BX_IA32_FEATURE_CONTROL_VMX_ENABLE_BIT)

// VMX error codes
typedef enum {
	VMXERR_NO_ERROR = 0,
	VMXERR_VMCALL_IN_VMX_ROOT_OPERATION = 1,
	VMXERR_VMCLEAR_WITH_INVALID_ADDR = 2,
	VMXERR_VMCLEAR_WITH_VMXON_VMCS_PTR = 3,
	VMXERR_VMLAUNCH_NON_CLEAR_VMCS = 4,
	VMXERR_VMRESUME_NON_LAUNCHED_VMCS = 5,
	VMXERR_VMRESUME_VMCS_CORRUPTED = 6,
	VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD = 7,
	VMXERR_VMENTRY_INVALID_VM_HOST_STATE_FIELD = 8,
	VMXERR_VMPTRLD_INVALID_PHYSICAL_ADDRESS = 9,
	VMXERR_VMPTRLD_WITH_VMXON_PTR = 10,
	VMXERR_VMPTRLD_INCORRECT_VMCS_REVISION_ID = 11,
	VMXERR_UNSUPPORTED_VMCS_COMPONENT_ACCESS = 12,
	VMXERR_VMWRITE_READ_ONLY_VMCS_COMPONENT = 13,
	VMXERR_RESERVED14 = 14,
	VMXERR_VMXON_IN_VMX_ROOT_OPERATION = 15,
	VMXERR_VMENTRY_INVALID_EXECUTIVE_VMCS = 16,
	VMXERR_VMENTRY_NON_LAUNCHED_EXECUTIVE_VMCS = 17,
	VMXERR_VMENTRY_NOT_VMXON_EXECUTIVE_VMCS = 18,
	VMXERR_VMCALL_NON_CLEAR_VMCS = 19,
	VMXERR_VMCALL_INVALID_VMEXIT_FIELD = 20,
	VMXERR_RESERVED21 = 21,
	VMXERR_VMCALL_INVALID_MSEG_REVISION_ID = 22,
	VMXERR_VMXOFF_WITH_CONFIGURED_SMM_MONITOR = 23,
	VMXERR_VMCALL_WITH_INVALID_SMM_MONITOR_FEATURES = 24,
	VMXERR_VMENTRY_INVALID_VM_CONTROL_FIELD_IN_EXECUTIVE_VMCS = 25,
	VMXERR_VMENTRY_MOV_SS_BLOCKING = 26,
	VMXERR_RESERVED27 = 27,
	VMXERR_INVALID_INVEPT_INVVPID = 28
}VMX_error_code;

enum VMX_vmexit_reason {
	VMX_VMEXIT_EXCEPTION_NMI = 0,
	VMX_VMEXIT_EXTERNAL_INTERRUPT = 1,
	VMX_VMEXIT_TRIPLE_FAULT = 2,
	VMX_VMEXIT_INIT = 3,
	VMX_VMEXIT_SIPI = 4,
	VMX_VMEXIT_IO_SMI = 5,
	VMX_VMEXIT_SMI = 6,
	VMX_VMEXIT_INTERRUPT_WINDOW = 7,
	VMX_VMEXIT_NMI_WINDOW = 8,
	VMX_VMEXIT_TASK_SWITCH = 9,
	VMX_VMEXIT_CPUID = 10,
	VMX_VMEXIT_GETSEC = 11,
	VMX_VMEXIT_HLT = 12,
	VMX_VMEXIT_INVD = 13,
	VMX_VMEXIT_INVLPG = 14,
	VMX_VMEXIT_RDPMC = 15,
	VMX_VMEXIT_RDTSC = 16,
	VMX_VMEXIT_RSM = 17,
	VMX_VMEXIT_VMCALL = 18,
	VMX_VMEXIT_VMCLEAR = 19,
	VMX_VMEXIT_VMLAUNCH = 20,
	VMX_VMEXIT_VMPTRLD = 21,
	VMX_VMEXIT_VMPTRST = 22,
	VMX_VMEXIT_VMREAD = 23,
	VMX_VMEXIT_VMRESUME = 24,
	VMX_VMEXIT_VMWRITE = 25,
	VMX_VMEXIT_VMXOFF = 26,
	VMX_VMEXIT_VMXON = 27,
	VMX_VMEXIT_CR_ACCESS = 28,
	VMX_VMEXIT_DR_ACCESS = 29,
	VMX_VMEXIT_IO_INSTRUCTION = 30,
	VMX_VMEXIT_RDMSR = 31,
	VMX_VMEXIT_WRMSR = 32,
	VMX_VMEXIT_VMENTRY_FAILURE_GUEST_STATE = 33,
	VMX_VMEXIT_VMENTRY_FAILURE_MSR = 34,
	VMX_VMEXIT_RESERVED35 = 35,
	VMX_VMEXIT_MWAIT = 36,
	VMX_VMEXIT_MONITOR_TRAP_FLAG = 37,
	VMX_VMEXIT_RESERVED38 = 38,
	VMX_VMEXIT_MONITOR = 39,
	VMX_VMEXIT_PAUSE = 40,
	VMX_VMEXIT_VMENTRY_FAILURE_MCA = 41, // will never happen in Bochs
	VMX_VMEXIT_RESERVED42 = 42,
	VMX_VMEXIT_TPR_THRESHOLD = 43,
	VMX_VMEXIT_APIC_ACCESS = 44,
	VMX_VMEXIT_VIRTUALIZED_EOI = 45,
	VMX_VMEXIT_GDTR_IDTR_ACCESS = 46,
	VMX_VMEXIT_LDTR_TR_ACCESS = 47,
	VMX_VMEXIT_EPT_VIOLATION = 48,
	VMX_VMEXIT_EPT_MISCONFIGURATION = 49,
	VMX_VMEXIT_INVEPT = 50,
	VMX_VMEXIT_RDTSCP = 51,
	VMX_VMEXIT_VMX_PREEMPTION_TIMER_EXPIRED = 52,
	VMX_VMEXIT_INVVPID = 53,
	VMX_VMEXIT_WBINVD = 54,
	VMX_VMEXIT_XSETBV = 55,
	VMX_VMEXIT_APIC_WRITE = 56,
	VMX_VMEXIT_RDRAND = 57,
	VMX_VMEXIT_INVPCID = 58,
	VMX_VMEXIT_VMFUNC = 59,
	VMX_VMEXIT_ENCLS = 60,
	VMX_VMEXIT_RDSEED = 61,
	VMX_VMEXIT_PML_LOGFULL = 62,
	VMX_VMEXIT_XSAVES = 63,
	VMX_VMEXIT_XRSTORS = 64,
	VMX_VMEXIT_LAST_REASON
};

#define IS_TRAP_LIKE_VMEXIT(reason) \
      (reason == VMX_VMEXIT_TPR_THRESHOLD || \
       reason == VMX_VMEXIT_VIRTUALIZED_EOI || \
       reason == VMX_VMEXIT_APIC_WRITE)

// VMexit on CR register access
enum {
	VMX_VMEXIT_CR_ACCESS_CR_WRITE = 0,
	VMX_VMEXIT_CR_ACCESS_CR_READ,
	VMX_VMEXIT_CR_ACCESS_CLTS,
	VMX_VMEXIT_CR_ACCESS_LMSW
};

// VMENTRY error on loading guest state qualification
enum VMX_vmentry_error {
	VMENTER_ERR_NO_ERROR = 0,
	VMENTER_ERR_GUEST_STATE_PDPTR_LOADING = 2,
	VMENTER_ERR_GUEST_STATE_INJECT_NMI_BLOCKING_EVENTS = 3,
	VMENTER_ERR_GUEST_STATE_LINK_POINTER = 4
};

// VMABORT error code
enum VMX_vmabort_code {
	VMABORT_SAVING_GUEST_MSRS_FAILURE,
	VMABORT_HOST_PDPTR_CORRUPTED,
	VMABORT_VMEXIT_VMCS_CORRUPTED,
	VMABORT_LOADING_HOST_MSRS,
	VMABORT_VMEXIT_MACHINE_CHECK_ERROR
};

// VMX APIC ACCESS VMEXIT qualification
#define VMX_APIC_READ_INSTRUCTION_EXECUTION   0x0000
#define VMX_APIC_WRITE_INSTRUCTION_EXECUTION  0x1000
#define VMX_APIC_INSTRUCTION_FETCH            0x2000 /* won't happen because cpu::prefetch will crash */
#define VMX_APIC_ACCESS_DURING_EVENT_DELIVERY 0x3000

// VM Functions List
enum VMFunctions {
	VMX_VMFUNC_EPTP_SWITCHING = 0
};

#define VMX_VMFUNC_EPTP_SWITCHING_MASK (BX_CONST64(1) << VMX_VMFUNC_EPTP_SWITCHING)

// =============
//  VMCS fields
// =============

/* VMCS 16-bit control fields */
/* binary 0000_00xx_xxxx_xxx0 */
#define VMCS_16BIT_CONTROL_VPID                            0x00000000 /* VPID */
#define VMCS_16BIT_CONTROL_POSTED_INTERRUPT_VECTOR         0x00000002 /* Posted Interrupts */
#define VMCS_16BIT_CONTROL_EPTP_INDEX                      0x00000004 /* #VE Exception */

/* VMCS 16-bit guest-state fields */
/* binary 0000_10xx_xxxx_xxx0 */
#define VMCS_16BIT_GUEST_ES_SELECTOR                       0x00000800
#define VMCS_16BIT_GUEST_CS_SELECTOR                       0x00000802
#define VMCS_16BIT_GUEST_SS_SELECTOR                       0x00000804
#define VMCS_16BIT_GUEST_DS_SELECTOR                       0x00000806
#define VMCS_16BIT_GUEST_FS_SELECTOR                       0x00000808
#define VMCS_16BIT_GUEST_GS_SELECTOR                       0x0000080A
#define VMCS_16BIT_GUEST_LDTR_SELECTOR                     0x0000080C
#define VMCS_16BIT_GUEST_TR_SELECTOR                       0x0000080E
#define VMCS_16BIT_GUEST_INTERRUPT_STATUS                  0x00000810 /* Virtual Interrupt Delivery */
#define VMCS_16BIT_GUEST_PML_INDEX                         0x00000812 /* Page Modification Logging */

/* VMCS 16-bit host-state fields */
/* binary 0000_11xx_xxxx_xxx0 */
#define VMCS_16BIT_HOST_ES_SELECTOR                        0x00000C00
#define VMCS_16BIT_HOST_CS_SELECTOR                        0x00000C02
#define VMCS_16BIT_HOST_SS_SELECTOR                        0x00000C04
#define VMCS_16BIT_HOST_DS_SELECTOR                        0x00000C06
#define VMCS_16BIT_HOST_FS_SELECTOR                        0x00000C08
#define VMCS_16BIT_HOST_GS_SELECTOR                        0x00000C0A
#define VMCS_16BIT_HOST_TR_SELECTOR                        0x00000C0C

/* VMCS 64-bit control fields */
/* binary 0010_00xx_xxxx_xxx0 */
#define VMCS_64BIT_CONTROL_IO_BITMAP_A                     0x00002000
#define VMCS_64BIT_CONTROL_IO_BITMAP_A_HI                  0x00002001
#define VMCS_64BIT_CONTROL_IO_BITMAP_B                     0x00002002
#define VMCS_64BIT_CONTROL_IO_BITMAP_B_HI                  0x00002003
#define VMCS_64BIT_CONTROL_MSR_BITMAPS                     0x00002004
#define VMCS_64BIT_CONTROL_MSR_BITMAPS_HI                  0x00002005
#define VMCS_64BIT_CONTROL_VMEXIT_MSR_STORE_ADDR           0x00002006
#define VMCS_64BIT_CONTROL_VMEXIT_MSR_STORE_ADDR_HI        0x00002007
#define VMCS_64BIT_CONTROL_VMEXIT_MSR_LOAD_ADDR            0x00002008
#define VMCS_64BIT_CONTROL_VMEXIT_MSR_LOAD_ADDR_HI         0x00002009
#define VMCS_64BIT_CONTROL_VMENTRY_MSR_LOAD_ADDR           0x0000200A
#define VMCS_64BIT_CONTROL_VMENTRY_MSR_LOAD_ADDR_HI        0x0000200B
#define VMCS_64BIT_CONTROL_EXECUTIVE_VMCS_PTR              0x0000200C
#define VMCS_64BIT_CONTROL_EXECUTIVE_VMCS_PTR_HI           0x0000200D
#define VMCS_64BIT_CONTROL_PML_ADDRESS                     0x0000200E /* Page Modification Logging */
#define VMCS_64BIT_CONTROL_PML_ADDRESS_HI                  0x0000200F
#define VMCS_64BIT_CONTROL_TSC_OFFSET                      0x00002010
#define VMCS_64BIT_CONTROL_TSC_OFFSET_HI                   0x00002011
#define VMCS_64BIT_CONTROL_VIRTUAL_APIC_PAGE_ADDR          0x00002012 /* TPR shadow */
#define VMCS_64BIT_CONTROL_VIRTUAL_APIC_PAGE_ADDR_HI       0x00002013
#define VMCS_64BIT_CONTROL_APIC_ACCESS_ADDR                0x00002014 /* APIC virtualization */
#define VMCS_64BIT_CONTROL_APIC_ACCESS_ADDR_HI             0x00002015
#define VMCS_64BIT_CONTROL_POSTED_INTERRUPT_DESC_ADDR      0x00002016 /* Posted Interrupts */
#define VMCS_64BIT_CONTROL_POSTED_INTERRUPT_DESC_ADDR_HI   0x00002017
#define VMCS_64BIT_CONTROL_VMFUNC_CTRLS                    0x00002018 /* VM Functions */
#define VMCS_64BIT_CONTROL_VMFUNC_CTRLS_HI                 0x00002019
#define VMCS_64BIT_CONTROL_EPTPTR                          0x0000201A /* EPT */
#define VMCS_64BIT_CONTROL_EPTPTR_HI                       0x0000201B
#define VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP0                0x0000201C /* Virtual Interrupt Delivery */
#define VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP0_HI             0x0000201D
#define VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP1                0x0000201E
#define VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP1_HI             0x0000201F
#define VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP2                0x00002020
#define VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP2_HI             0x00002021
#define VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP3                0x00002022
#define VMCS_64BIT_CONTROL_EOI_EXIT_BITMAP3_HI             0x00002023
#define VMCS_64BIT_CONTROL_EPTP_LIST_ADDRESS               0x00002024 /* VM Functions - EPTP switching */
#define VMCS_64BIT_CONTROL_EPTP_LIST_ADDRESS_HI            0x00002025
#define VMCS_64BIT_CONTROL_VMREAD_BITMAP_ADDR              0x00002026 /* VMCS Shadowing */
#define VMCS_64BIT_CONTROL_VMREAD_BITMAP_ADDR_HI           0x00002027
#define VMCS_64BIT_CONTROL_VMWRITE_BITMAP_ADDR             0x00002028 /* VMCS Shadowing */
#define VMCS_64BIT_CONTROL_VMWRITE_BITMAP_ADDR_HI          0x00002029
#define VMCS_64BIT_CONTROL_VE_EXCEPTION_INFO_ADDR          0x0000202A /* #VE Exception */
#define VMCS_64BIT_CONTROL_VE_EXCEPTION_INFO_ADDR_HI       0x0000202B
#define VMCS_64BIT_CONTROL_XSS_EXITING_BITMAP              0x0000202C /* XSAVES */
#define VMCS_64BIT_CONTROL_XSS_EXITING_BITMAP_HI           0x0000202D
#define VMCS_64BIT_CONTROL_ENCLS_EXITING_BITMAP            0x0000202E /* ENCLS/SGX */
#define VMCS_64BIT_CONTROL_ENCLS_EXITING_BITMAP_HI         0x0000202F
#define VMCS_64BIT_CONTROL_TSC_MULTIPLIER                  0x00002032 /* TSC Scaling */
#define VMCS_64BIT_CONTROL_TSC_MULTIPLIER_HI               0x00002033

/* VMCS 64-bit read only data fields */
/* binary 0010_01xx_xxxx_xxx0 */
#define VMCS_64BIT_GUEST_PHYSICAL_ADDR                     0x00002400 /* EPT */
#define VMCS_64BIT_GUEST_PHYSICAL_ADDR_HI                  0x00002401

/* VMCS 64-bit guest state fields */
/* binary 0010_10xx_xxxx_xxx0 */
#define VMCS_64BIT_GUEST_LINK_POINTER                      0x00002800
#define VMCS_64BIT_GUEST_LINK_POINTER_HI                   0x00002801
#define VMCS_64BIT_GUEST_IA32_DEBUGCTL                     0x00002802
#define VMCS_64BIT_GUEST_IA32_DEBUGCTL_HI                  0x00002803
#define VMCS_64BIT_GUEST_IA32_PAT                          0x00002804 /* PAT */
#define VMCS_64BIT_GUEST_IA32_PAT_HI                       0x00002805
#define VMCS_64BIT_GUEST_IA32_EFER                         0x00002806 /* EFER */
#define VMCS_64BIT_GUEST_IA32_EFER_HI                      0x00002807
#define VMCS_64BIT_GUEST_IA32_PERF_GLOBAL_CTRL             0x00002808 /* Perf Global Ctrl */
#define VMCS_64BIT_GUEST_IA32_PERF_GLOBAL_CTRL_HI          0x00002809
#define VMCS_64BIT_GUEST_IA32_PDPTE0                       0x0000280A /* EPT */
#define VMCS_64BIT_GUEST_IA32_PDPTE0_HI                    0x0000280B
#define VMCS_64BIT_GUEST_IA32_PDPTE1                       0x0000280C
#define VMCS_64BIT_GUEST_IA32_PDPTE1_HI                    0x0000280D
#define VMCS_64BIT_GUEST_IA32_PDPTE2                       0x0000280E
#define VMCS_64BIT_GUEST_IA32_PDPTE2_HI                    0x0000280F
#define VMCS_64BIT_GUEST_IA32_PDPTE3                       0x00002810
#define VMCS_64BIT_GUEST_IA32_PDPTE3_HI                    0x00002811
#define VMCS_64BIT_GUEST_IA32_BNDCFGS                      0x00002812 /* MPX */
#define VMCS_64BIT_GUEST_IA32_BNDCFGS_HI                   0x00002813

/* VMCS 64-bit host state fields */
/* binary 0010_11xx_xxxx_xxx0 */
#define VMCS_64BIT_HOST_IA32_PAT                           0x00002C00 /* PAT */
#define VMCS_64BIT_HOST_IA32_PAT_HI                        0x00002C01
#define VMCS_64BIT_HOST_IA32_EFER                          0x00002C02 /* EFER */
#define VMCS_64BIT_HOST_IA32_EFER_HI                       0x00002C03
#define VMCS_64BIT_HOST_IA32_PERF_GLOBAL_CTRL              0x00002C04 /* Perf Global Ctrl */
#define VMCS_64BIT_HOST_IA32_PERF_GLOBAL_CTRL_HI           0x00002C05

/* VMCS 32_bit control fields */
/* binary 0100_00xx_xxxx_xxx0 */
#define VMCS_32BIT_CONTROL_PIN_BASED_EXEC_CONTROLS         0x00004000
#define VMCS_32BIT_CONTROL_PROCESSOR_BASED_VMEXEC_CONTROLS 0x00004002
#define VMCS_32BIT_CONTROL_EXECUTION_BITMAP                0x00004004
#define VMCS_32BIT_CONTROL_PAGE_FAULT_ERR_CODE_MASK        0x00004006
#define VMCS_32BIT_CONTROL_PAGE_FAULT_ERR_CODE_MATCH       0x00004008
#define VMCS_32BIT_CONTROL_CR3_TARGET_COUNT                0x0000400A
#define VMCS_32BIT_CONTROL_VMEXIT_CONTROLS                 0x0000400C
#define VMCS_32BIT_CONTROL_VMEXIT_MSR_STORE_COUNT          0x0000400E
#define VMCS_32BIT_CONTROL_VMEXIT_MSR_LOAD_COUNT           0x00004010
#define VMCS_32BIT_CONTROL_VMENTRY_CONTROLS                0x00004012
#define VMCS_32BIT_CONTROL_VMENTRY_MSR_LOAD_COUNT          0x00004014
#define VMCS_32BIT_CONTROL_VMENTRY_INTERRUPTION_INFO       0x00004016
#define VMCS_32BIT_CONTROL_VMENTRY_EXCEPTION_ERR_CODE      0x00004018
#define VMCS_32BIT_CONTROL_VMENTRY_INSTRUCTION_LENGTH      0x0000401A
#define VMCS_32BIT_CONTROL_TPR_THRESHOLD                   0x0000401C /* TPR shadow */
#define VMCS_32BIT_CONTROL_SECONDARY_VMEXEC_CONTROLS       0x0000401E
#define VMCS_32BIT_CONTROL_PAUSE_LOOP_EXITING_GAP          0x00004020 /* PAUSE loop exiting */
#define VMCS_32BIT_CONTROL_PAUSE_LOOP_EXITING_WINDOW       0x00004022 /* PAUSE loop exiting */

/* VMCS 32-bit read only data fields */
/* binary 0100_01xx_xxxx_xxx0 */
#define VMCS_32BIT_INSTRUCTION_ERROR                       0x00004400
#define VMCS_32BIT_VMEXIT_REASON                           0x00004402
#define VMCS_32BIT_VMEXIT_INTERRUPTION_INFO                0x00004404
#define VMCS_32BIT_VMEXIT_INTERRUPTION_ERR_CODE            0x00004406
#define VMCS_32BIT_IDT_VECTORING_INFO                      0x00004408
#define VMCS_32BIT_IDT_VECTORING_ERR_CODE                  0x0000440A
#define VMCS_32BIT_VMEXIT_INSTRUCTION_LENGTH               0x0000440C
#define VMCS_32BIT_VMEXIT_INSTRUCTION_INFO                 0x0000440E

/* VMCS 32-bit guest-state fields */
/* binary 0100_10xx_xxxx_xxx0 */
#define VMCS_32BIT_GUEST_ES_LIMIT                          0x00004800
#define VMCS_32BIT_GUEST_CS_LIMIT                          0x00004802
#define VMCS_32BIT_GUEST_SS_LIMIT                          0x00004804
#define VMCS_32BIT_GUEST_DS_LIMIT                          0x00004806
#define VMCS_32BIT_GUEST_FS_LIMIT                          0x00004808
#define VMCS_32BIT_GUEST_GS_LIMIT                          0x0000480A
#define VMCS_32BIT_GUEST_LDTR_LIMIT                        0x0000480C
#define VMCS_32BIT_GUEST_TR_LIMIT                          0x0000480E
#define VMCS_32BIT_GUEST_GDTR_LIMIT                        0x00004810
#define VMCS_32BIT_GUEST_IDTR_LIMIT                        0x00004812
#define VMCS_32BIT_GUEST_ES_ACCESS_RIGHTS                  0x00004814
#define VMCS_32BIT_GUEST_CS_ACCESS_RIGHTS                  0x00004816
#define VMCS_32BIT_GUEST_SS_ACCESS_RIGHTS                  0x00004818
#define VMCS_32BIT_GUEST_DS_ACCESS_RIGHTS                  0x0000481A
#define VMCS_32BIT_GUEST_FS_ACCESS_RIGHTS                  0x0000481C
#define VMCS_32BIT_GUEST_GS_ACCESS_RIGHTS                  0x0000481E
#define VMCS_32BIT_GUEST_LDTR_ACCESS_RIGHTS                0x00004820
#define VMCS_32BIT_GUEST_TR_ACCESS_RIGHTS                  0x00004822
#define VMCS_32BIT_GUEST_INTERRUPTIBILITY_STATE            0x00004824
#define VMCS_32BIT_GUEST_ACTIVITY_STATE                    0x00004826
#define VMCS_32BIT_GUEST_SMBASE                            0x00004828
#define VMCS_32BIT_GUEST_IA32_SYSENTER_CS_MSR              0x0000482A
#define VMCS_32BIT_GUEST_PREEMPTION_TIMER_VALUE            0x0000482E /* VMX preemption timer */

/* VMCS 32-bit host-state fields */
/* binary 0100_11xx_xxxx_xxx0 */
#define VMCS_32BIT_HOST_IA32_SYSENTER_CS_MSR               0x00004C00

/* VMCS natural width control fields */
/* binary 0110_00xx_xxxx_xxx0 */
#define VMCS_CONTROL_CR0_GUEST_HOST_MASK                   0x00006000
#define VMCS_CONTROL_CR4_GUEST_HOST_MASK                   0x00006002
#define VMCS_CONTROL_CR0_READ_SHADOW                       0x00006004
#define VMCS_CONTROL_CR4_READ_SHADOW                       0x00006006
#define VMCS_CR3_TARGET0                                   0x00006008
#define VMCS_CR3_TARGET1                                   0x0000600A
#define VMCS_CR3_TARGET2                                   0x0000600C
#define VMCS_CR3_TARGET3                                   0x0000600E

/* VMCS natural width read only data fields */
/* binary 0110_01xx_xxxx_xxx0 */
#define VMCS_VMEXIT_QUALIFICATION                          0x00006400
#define VMCS_IO_RCX                                        0x00006402
#define VMCS_IO_RSI                                        0x00006404
#define VMCS_IO_RDI                                        0x00006406
#define VMCS_IO_RIP                                        0x00006408
#define VMCS_GUEST_LINEAR_ADDR                             0x0000640A

/* VMCS natural width guest state fields */
/* binary 0110_10xx_xxxx_xxx0 */
#define VMCS_GUEST_CR0                                     0x00006800
#define VMCS_GUEST_CR3                                     0x00006802
#define VMCS_GUEST_CR4                                     0x00006804
#define VMCS_GUEST_ES_BASE                                 0x00006806
#define VMCS_GUEST_CS_BASE                                 0x00006808
#define VMCS_GUEST_SS_BASE                                 0x0000680A
#define VMCS_GUEST_DS_BASE                                 0x0000680C
#define VMCS_GUEST_FS_BASE                                 0x0000680E
#define VMCS_GUEST_GS_BASE                                 0x00006810
#define VMCS_GUEST_LDTR_BASE                               0x00006812
#define VMCS_GUEST_TR_BASE                                 0x00006814
#define VMCS_GUEST_GDTR_BASE                               0x00006816
#define VMCS_GUEST_IDTR_BASE                               0x00006818
#define VMCS_GUEST_DR7                                     0x0000681A
#define VMCS_GUEST_RSP                                     0x0000681C
#define VMCS_GUEST_RIP                                     0x0000681E
#define VMCS_GUEST_RFLAGS                                  0x00006820
#define VMCS_GUEST_PENDING_DBG_EXCEPTIONS                  0x00006822
#define VMCS_GUEST_IA32_SYSENTER_ESP_MSR                   0x00006824
#define VMCS_GUEST_IA32_SYSENTER_EIP_MSR                   0x00006826

/* VMCS natural width host state fields */
/* binary 0110_11xx_xxxx_xxx0 */
#define VMCS_HOST_CR0                                      0x00006C00
#define VMCS_HOST_CR3                                      0x00006C02
#define VMCS_HOST_CR4                                      0x00006C04
#define VMCS_HOST_FS_BASE                                  0x00006C06
#define VMCS_HOST_GS_BASE                                  0x00006C08
#define VMCS_HOST_TR_BASE                                  0x00006C0A
#define VMCS_HOST_GDTR_BASE                                0x00006C0C
#define VMCS_HOST_IDTR_BASE                                0x00006C0E
#define VMCS_HOST_IA32_SYSENTER_ESP_MSR                    0x00006C10
#define VMCS_HOST_IA32_SYSENTER_EIP_MSR                    0x00006C12
#define VMCS_HOST_RSP                                      0x00006C14
#define VMCS_HOST_RIP                                      0x00006C16

#define VMX_HIGHEST_VMCS_ENCODING   (0x34)

// ===============================
//  VMCS fields encoding/decoding
// ===============================

// extract VMCS field using its encoding
#define VMCS_FIELD(encoding)        ((encoding) & 0x3ff)

// check if the VMCS field encoding corresponding to HI part of 64-bit value
#define IS_VMCS_FIELD_HI(encoding)  ((encoding) & 1)

// bits 11:10 of VMCS field encoding indicate field's type
#define VMCS_FIELD_TYPE(encoding)   (((encoding) >> 10) & 3)

#define VMCS_FIELD_TYPE_CONTROL        0x0
#define VMCS_FIELD_TYPE_READ_ONLY      0x1
#define VMCS_FIELD_TYPE_GUEST_STATE    0x2
#define VMCS_FIELD_TYPE_HOST_STATE     0x3

// bits 14:13 of VMCS field encoding indicate field's width
#define VMCS_FIELD_WIDTH(encoding)  (((encoding) >> 13) & 3)

#define VMCS_FIELD_WIDTH_16BIT         0x0
#define VMCS_FIELD_WIDTH_64BIT         0x1
#define VMCS_FIELD_WIDTH_32BIT         0x2
#define VMCS_FIELD_WIDTH_NATURAL_WIDTH 0x3

#define VMCS_FIELD_INDEX(encoding) \
    ((VMCS_FIELD_WIDTH(encoding) << 2) + VMCS_FIELD_TYPE(encoding))

#define VMCS_ENCODING_RESERVED_BITS (0xffff9000)

// =============
//  VMCS layout
// =============

#define BX_VMX_VMCS_REVISION_ID 0x2B /* better to be unique bochs VMCS revision id */


#define VMCS_LAUNCH_STATE_FIELD_ENCODING         (0xfffffffe)
#define VMCS_VMX_ABORT_FIELD_ENCODING            (0xfffffffc)
#define VMCS_REVISION_ID_FIELD_ENCODING          (0xfffffffa)

#define VMCS_REVISION_ID_FIELD_ADDR              (0x0000)
#define VMCS_VMX_ABORT_FIELD_ADDR                (0x0004)
#define VMCS_LAUNCH_STATE_FIELD_ADDR             (0x0008)

#define VMCS_DATA_OFFSET                         (0x0010)

#if ((VMCS_DATA_OFFSET + 4*(64*15 + VMX_HIGHEST_VMCS_ENCODING)) > VMX_VMCS_AREA_SIZE)
#error "VMCS area size exceeded !"
#endif

// =============
//  VMCS state
// =============

enum VMX_state {
	VMCS_STATE_CLEAR = 0,
};

// ================
//  VMCS structure
// ================

typedef struct { /* bx_selector_t */
	Bit16u value;   /* the 16bit value of the selector */
	/* the following fields are extracted from the value field in protected
	   mode only.  They're used for sake of efficiency */
	Bit16u index;   /* 13bit index extracted from value in protected mode */
	Bit8u  ti;      /* table indicator bit extracted from value */
	Bit8u  rpl;     /* RPL extracted from value */
} bx_selector_t;



typedef struct
{

#define SegValidCache  (0x01)
#define SegAccessROK   (0x02)
#define SegAccessWOK   (0x04)
#define SegAccessROK4G (0x08)
#define SegAccessWOK4G (0x10)
	unsigned valid;        // Holds above values, Or'd together. Used to
						   // hold only 0 or 1 once.

	bx_bool p;             /* present */
	Bit8u   dpl;           /* descriptor privilege level 0..3 */
	bx_bool segment;       /* 0 = system/gate, 1 = data/code segment */
	Bit8u   type;          /* For system & gate descriptors:
							*  0 = invalid descriptor (reserved)
							*  1 = 286 available Task State Segment (TSS)
							*  2 = LDT descriptor
							*  3 = 286 busy Task State Segment (TSS)
							*  4 = 286 call gate
							*  5 = task gate
							*  6 = 286 interrupt gate
							*  7 = 286 trap gate
							*  8 = (reserved)
							*  9 = 386 available TSS
							* 10 = (reserved)
							* 11 = 386 busy TSS
							* 12 = 386 call gate
							* 13 = (reserved)
							* 14 = 386 interrupt gate
							* 15 = 386 trap gate */

							// For system & gate descriptors:

#define BX_GATE_TYPE_NONE                       (0x0)
#define BX_SYS_SEGMENT_AVAIL_286_TSS            (0x1)
#define BX_SYS_SEGMENT_LDT                      (0x2)
#define BX_SYS_SEGMENT_BUSY_286_TSS             (0x3)
#define BX_286_CALL_GATE                        (0x4)
#define BX_TASK_GATE                            (0x5)
#define BX_286_INTERRUPT_GATE                   (0x6)
#define BX_286_TRAP_GATE                        (0x7)
											  /* 0x8 reserved */
#define BX_SYS_SEGMENT_AVAIL_386_TSS            (0x9)
											  /* 0xa reserved */
#define BX_SYS_SEGMENT_BUSY_386_TSS             (0xb)
#define BX_386_CALL_GATE                        (0xc)
											  /* 0xd reserved */
#define BX_386_INTERRUPT_GATE                   (0xe)
#define BX_386_TRAP_GATE                        (0xf)

// For data/code descriptors:

#define BX_DATA_READ_ONLY                       (0x0)
#define BX_DATA_READ_ONLY_ACCESSED              (0x1)
#define BX_DATA_READ_WRITE                      (0x2)
#define BX_DATA_READ_WRITE_ACCESSED             (0x3)
#define BX_DATA_READ_ONLY_EXPAND_DOWN           (0x4)
#define BX_DATA_READ_ONLY_EXPAND_DOWN_ACCESSED  (0x5)
#define BX_DATA_READ_WRITE_EXPAND_DOWN          (0x6)
#define BX_DATA_READ_WRITE_EXPAND_DOWN_ACCESSED (0x7)
#define BX_CODE_EXEC_ONLY                       (0x8)
#define BX_CODE_EXEC_ONLY_ACCESSED              (0x9)
#define BX_CODE_EXEC_READ                       (0xa)
#define BX_CODE_EXEC_READ_ACCESSED              (0xb)
#define BX_CODE_EXEC_ONLY_CONFORMING            (0xc)
#define BX_CODE_EXEC_ONLY_CONFORMING_ACCESSED   (0xd)
#define BX_CODE_EXEC_READ_CONFORMING            (0xe)
#define BX_CODE_EXEC_READ_CONFORMING_ACCESSED   (0xf)

	union {
		struct {
			bx_address base;       /* base address: 286=24bits, 386=32bits, long=64 */
			Bit32u  limit_scaled;  /* for efficiency, this contrived field is set to
									* limit for byte granular, and
									* (limit << 12) | 0xfff for page granular seg's
									*/
			bx_bool g;             /* granularity: 0=byte, 1=4K (page) */
			bx_bool d_b;           /* default size: 0=16bit, 1=32bit */
#if BX_SUPPORT_X86_64
			bx_bool l;             /* long mode: 0=compat, 1=64 bit */
#endif
			bx_bool avl;           /* available for use by system */
		} segment;
		struct {
			Bit8u   param_count;   /* 5bits (0..31) #words/dword to copy from caller's
									* stack to called procedure's stack. */
			Bit16u  dest_selector;
			Bit32u  dest_offset;
		} gate;
		struct {                 /* type 5: Task Gate Descriptor */
			Bit16u  tss_selector;  /* TSS segment selector */
		} taskgate;
	} u;

} bx_descriptor_t;



typedef struct {
	bx_selector_t    selector;
	bx_descriptor_t  cache;
} bx_segment_reg_t;

typedef struct {
	bx_address       base;   /* base address: 24bits=286,32bits=386,64bits=x86-64 */
	Bit16u           limit;  /* limit, 16bits */
} bx_global_segment_reg_t;

typedef struct bx_VMCS_GUEST_STATE
{
	bx_address cr0;
	bx_address cr3;
	bx_address cr4;
	bx_address dr7;

	bx_address rip;
	bx_address rsp;
	bx_address rflags;

	bx_segment_reg_t sregs[6];

	bx_global_segment_reg_t gdtr;
	bx_global_segment_reg_t idtr;
	bx_segment_reg_t        ldtr;
	bx_segment_reg_t        tr;

	Bit64u ia32_debugctl_msr;
	bx_address sysenter_esp_msr;
	bx_address sysenter_eip_msr;
	Bit32u sysenter_cs_msr;

	Bit32u smbase;
	Bit32u activity_state;
	Bit32u interruptibility_state;
	Bit32u tmpDR6;

#if BX_SUPPORT_VMX >= 2
#if BX_SUPPORT_X86_64
	Bit64u efer_msr;
#endif
	Bit64u pat_msr;
	Bit64u pdptr[4];
#endif
} VMCS_GUEST_STATE;

#define BX_NOTIFY_PHY_MEMORY_ACCESS(paddr, size, memtype, rw, why, dataptr) {              \
  BX_INSTR_PHY_ACCESS(BX_CPU_ID, (paddr), (size), (memtype), (rw));                        \
  BX_DBG_PHY_MEMORY_ACCESS(BX_CPU_ID, (paddr), (size), (memtype), (rw), (why), (dataptr)); \
}
enum {
	BX_ISA_386 = 0,                 /* 386 or earlier instruction */
	BX_ISA_X87,                     /* FPU (X87) instruction */
	BX_ISA_486,                     /* 486 new instruction */
	BX_ISA_PENTIUM,                 /* Pentium new instruction */
	BX_ISA_P6,                      /* P6 new instruction */
	BX_ISA_MMX,                     /* MMX instruction */
	BX_ISA_3DNOW,                   /* 3DNow! instruction (AMD) */
	BX_ISA_DEBUG_EXTENSIONS,        /* Debug Extensions support */
	BX_ISA_VME,                     /* VME support */
	BX_ISA_PSE,                     /* PSE support */
	BX_ISA_PAE,                     /* PAE support */
	BX_ISA_PGE,                     /* Global Pages support */
	BX_ISA_PSE36,                   /* PSE-36 support */
	BX_ISA_MTRR,                    /* MTRR support */
	BX_ISA_PAT,                     /* PAT support */
	BX_ISA_SYSCALL_SYSRET_LEGACY,   /* SYSCALL/SYSRET in legacy mode (AMD) */
	BX_ISA_SYSENTER_SYSEXIT,        /* SYSENTER/SYSEXIT instruction */
	BX_ISA_CLFLUSH,                 /* CLFLUSH instruction */
	BX_ISA_CLFLUSHOPT,              /* CLFLUSHOPT instruction */
	BX_ISA_CLWB,                    /* CLWB instruction */
	BX_ISA_SSE,                     /* SSE  instruction */
	BX_ISA_SSE2,                    /* SSE2 instruction */
	BX_ISA_SSE3,                    /* SSE3 instruction */
	BX_ISA_SSSE3,                   /* SSSE3 instruction */
	BX_ISA_SSE4_1,                  /* SSE4_1 instruction */
	BX_ISA_SSE4_2,                  /* SSE4_2 instruction */
	BX_ISA_POPCNT,                  /* POPCNT instruction */
	BX_ISA_MONITOR_MWAIT,           /* MONITOR/MWAIT instruction */
	BX_ISA_MONITORX_MWAITX,         /* MONITORX/MWAITX instruction (AMD) */
	BX_ISA_VMX,                     /* VMX instruction */
	BX_ISA_SMX,                     /* SMX instruction */
	BX_ISA_LONG_MODE,               /* Long Mode (x86-64) support */
	BX_ISA_LM_LAHF_SAHF,            /* Long Mode LAHF/SAHF instruction */
	BX_ISA_NX,                      /* No-Execute support */
	BX_ISA_1G_PAGES,                /* 1Gb pages support */
	BX_ISA_CMPXCHG16B,              /* CMPXCHG16B instruction */
	BX_ISA_RDTSCP,                  /* RDTSCP instruction */
	BX_ISA_FFXSR,                   /* EFER.FFXSR support */
	BX_ISA_XSAVE,                   /* XSAVE/XRSTOR extensions instruction */
	BX_ISA_XSAVEOPT,                /* XSAVEOPT instruction */
	BX_ISA_XSAVEC,                  /* XSAVEC instruction */
	BX_ISA_XSAVES,                  /* XSAVES instruction */
	BX_ISA_AES_PCLMULQDQ,           /* AES+PCLMULQDQ instruction */
	BX_ISA_MOVBE,                   /* MOVBE instruction */
	BX_ISA_FSGSBASE,                /* FS/GS BASE access instruction */
	BX_ISA_INVPCID,                 /* INVPCID instruction */
	BX_ISA_AVX,                     /* AVX instruction */
	BX_ISA_AVX2,                    /* AVX2 instruction */
	BX_ISA_AVX_F16C,                /* AVX F16 convert instruction */
	BX_ISA_AVX_FMA,                 /* AVX FMA instruction */
	BX_ISA_ALT_MOV_CR8,             /* LOCK CR0 access CR8 (AMD) */
	BX_ISA_SSE4A,                   /* SSE4A instruction (AMD) */
	BX_ISA_MISALIGNED_SSE,          /* Misaligned SSE (AMD) */
	BX_ISA_LZCNT,                   /* LZCNT instruction */
	BX_ISA_BMI1,                    /* BMI1 instruction */
	BX_ISA_BMI2,                    /* BMI2 instruction */
	BX_ISA_FMA4,                    /* FMA4 instruction (AMD) */
	BX_ISA_XOP,                     /* XOP instruction (AMD) */
	BX_ISA_TBM,                     /* TBM instruction (AMD) */
	BX_ISA_SVM,                     /* SVM instruction (AMD) */
	BX_ISA_RDRAND,                  /* RDRAND instruction */
	BX_ISA_ADX,                     /* ADCX/ADOX instruction */
	BX_ISA_SMAP,                    /* SMAP support */
	BX_ISA_RDSEED,                  /* RDSEED instruction */
	BX_ISA_SHA,                     /* SHA instruction */
	BX_ISA_AVX512,                  /* AVX-512 instruction */
	BX_ISA_AVX512_CD,               /* AVX-512 Conflict Detection instruction */
	BX_ISA_AVX512_PF,               /* AVX-512 Sparse Prefetch instruction */
	BX_ISA_AVX512_ER,               /* AVX-512 Exponential/Reciprocal instruction */
	BX_ISA_AVX512_DQ,               /* AVX-512DQ instruction */
	BX_ISA_AVX512_BW,               /* AVX-512 Byte/Word instruction */
	BX_ISA_AVX512_VL,               /* AVX-512 Vector Length extensions */
	BX_ISA_AVX512_VBMI,             /* AVX-512 Vector Bit Manipulation Instructions */
	BX_ISA_AVX512_IFMA52,           /* AVX-512 IFMA52 Instructions */
	BX_ISA_AVX512_VPOPCNTDQ,        /* AVX-512 VPOPCNTD/VPOPCNTQ Instructions */
	BX_ISA_XAPIC,                   /* XAPIC support */
	BX_ISA_X2APIC,                  /* X2APIC support */
	BX_ISA_XAPIC_EXT,               /* XAPIC Extensions support */
	BX_ISA_PCID,                    /* PCID pages support */
	BX_ISA_SMEP,                    /* SMEP support */
	BX_ISA_TSC_DEADLINE,            /* TSC-Deadline */
	BX_ISA_FCS_FDS_DEPRECATION,     /* FCS/FDS Deprecation */
	BX_ISA_FDP_DEPRECATION,         /* FDP Deprecation - FDP update on unmasked x87 exception only */
	BX_ISA_PKU,                     /* User-Mode Protection Keys */
	BX_ISA_UMIP,                    /* User-Mode Instructions Prevention */
	BX_ISA_RDPID,                   /* RDPID Support */
	BX_ISA_TCE,                     /* Translation Cache Extensions (TCE) support (AMD) */
	BX_ISA_CLZERO,                  /* CLZERO instruction support (AMD) */
	BX_ISA_EXTENSION_LAST
};


#define BX_MSR_EFER             0xc0000080
#define BX_MSR_STAR             0xc0000081
#define BX_MSR_LSTAR            0xc0000082
#define BX_MSR_CSTAR            0xc0000083
#define BX_MSR_FMASK            0xc0000084
#define BX_MSR_FSBASE           0xc0000100
#define BX_MSR_GSBASE           0xc0000101
#define BX_MSR_KERNELGSBASE     0xc0000102
#define BX_MSR_TSC_AUX          0xc0000103

#define BX_SVM_VM_CR_MSR        0xc0010114
#define BX_SVM_IGNNE_MSR        0xc0010115
#define BX_SVM_SMM_CTL_MSR      0xc0010116
#define BX_SVM_HSAVE_PA_MSR     0xc0010117

#define BX_MSR_XSS              0xda0

typedef struct bx_VMCS_HOST_STATE
{
	bx_address cr0;
	bx_address cr3;
	bx_address cr4;

	Bit16u segreg_selector[6];

	bx_address fs_base;
	bx_address gs_base;

	bx_address gdtr_base;
	bx_address idtr_base;

	Bit32u tr_selector;
	bx_address tr_base;

	bx_address rsp;
	bx_address rip;

	bx_address sysenter_esp_msr;
	bx_address sysenter_eip_msr;
	Bit32u sysenter_cs_msr;

#if BX_SUPPORT_VMX >= 2
#if BX_SUPPORT_X86_64
	Bit64u efer_msr;
#endif
	Bit64u pat_msr;
#endif
} VMCS_HOST_STATE;

#if BX_SUPPORT_VMX >= 2

// used for pause loop exiting
typedef struct {
	Bit32u pause_loop_exiting_gap;
	Bit32u pause_loop_exiting_window;
	Bit64u last_pause_time;
	Bit64u first_pause_time;
}VMX_PLE;

#endif

typedef struct bx_VMCS
{
	//
	// VM-Execution Control Fields
	//

#define VMX_VM_EXEC_CTRL1_EXTERNAL_INTERRUPT_VMEXIT   (1 << 0)
#define VMX_VM_EXEC_CTRL1_NMI_EXITING                 (1 << 3)
#define VMX_VM_EXEC_CTRL1_VIRTUAL_NMI                 (1 << 5) /* Virtual NMI */
#define VMX_VM_EXEC_CTRL1_VMX_PREEMPTION_TIMER_VMEXIT (1 << 6) /* VMX preemption timer */
#define VMX_VM_EXEC_CTRL1_PROCESS_POSTED_INTERRUPTS   (1 << 7) /* Posted Interrupts */

#define VMX_VM_EXEC_CTRL1_SUPPORTED_BITS \
    (vmx_pin_vmexec_ctrl_supported_bits)

	Bit32u vmexec_ctrls1;

#define VMX_VM_EXEC_CTRL2_INTERRUPT_WINDOW_VMEXIT   (1 << 2)
#define VMX_VM_EXEC_CTRL2_TSC_OFFSET                (1 << 3)
#define VMX_VM_EXEC_CTRL2_HLT_VMEXIT                (1 << 7)
#define VMX_VM_EXEC_CTRL2_INVLPG_VMEXIT             (1 << 9)
#define VMX_VM_EXEC_CTRL2_MWAIT_VMEXIT              (1 << 10)
#define VMX_VM_EXEC_CTRL2_RDPMC_VMEXIT              (1 << 11)
#define VMX_VM_EXEC_CTRL2_RDTSC_VMEXIT              (1 << 12)
#define VMX_VM_EXEC_CTRL2_CR3_WRITE_VMEXIT          (1 << 15) /* legacy must be '1 */
#define VMX_VM_EXEC_CTRL2_CR3_READ_VMEXIT           (1 << 16) /* legacy must be '1 */
#define VMX_VM_EXEC_CTRL2_CR8_WRITE_VMEXIT          (1 << 19) /* TPR shadow */
#define VMX_VM_EXEC_CTRL2_CR8_READ_VMEXIT           (1 << 20) /* TPR shadow */
#define VMX_VM_EXEC_CTRL2_TPR_SHADOW                (1 << 21) /* TPR shadow */
#define VMX_VM_EXEC_CTRL2_NMI_WINDOW_EXITING        (1 << 22) /* Virtual NMI */
#define VMX_VM_EXEC_CTRL2_DRx_ACCESS_VMEXIT         (1 << 23)
#define VMX_VM_EXEC_CTRL2_IO_VMEXIT                 (1 << 24)
#define VMX_VM_EXEC_CTRL2_IO_BITMAPS                (1 << 25)
#define VMX_VM_EXEC_CTRL2_MONITOR_TRAP_FLAG         (1 << 27) /* Monitor Trap Flag */
#define VMX_VM_EXEC_CTRL2_MSR_BITMAPS               (1 << 28)
#define VMX_VM_EXEC_CTRL2_MONITOR_VMEXIT            (1 << 29)
#define VMX_VM_EXEC_CTRL2_PAUSE_VMEXIT              (1 << 30)
#define VMX_VM_EXEC_CTRL2_SECONDARY_CONTROLS        (1 << 31)

#define VMX_VM_EXEC_CTRL2_SUPPORTED_BITS \
    (vmx_proc_vmexec_ctrl_supported_bits)

	Bit32u vmexec_ctrls2;

#define VMX_VM_EXEC_CTRL3_VIRTUALIZE_APIC_ACCESSES  (1 <<  0) /* APIC virtualization */
#define VMX_VM_EXEC_CTRL3_EPT_ENABLE                (1 <<  1) /* EPT */
#define VMX_VM_EXEC_CTRL3_DESCRIPTOR_TABLE_VMEXIT   (1 <<  2) /* Descriptor Table VMEXIT */
#define VMX_VM_EXEC_CTRL3_RDTSCP                    (1 <<  3)
#define VMX_VM_EXEC_CTRL3_VIRTUALIZE_X2APIC_MODE    (1 <<  4) /* Virtualize X2APIC */
#define VMX_VM_EXEC_CTRL3_VPID_ENABLE               (1 <<  5) /* VPID */
#define VMX_VM_EXEC_CTRL3_WBINVD_VMEXIT             (1 <<  6) /* WBINVD VMEXIT */
#define VMX_VM_EXEC_CTRL3_UNRESTRICTED_GUEST        (1 <<  7) /* Unrestricted Guest */
#define VMX_VM_EXEC_CTRL3_VIRTUALIZE_APIC_REGISTERS (1 <<  8)
#define VMX_VM_EXEC_CTRL3_VIRTUAL_INT_DELIVERY      (1 <<  9)
#define VMX_VM_EXEC_CTRL3_PAUSE_LOOP_VMEXIT         (1 << 10) /* PAUSE loop exiting */
#define VMX_VM_EXEC_CTRL3_RDRAND_VMEXIT             (1 << 11)
#define VMX_VM_EXEC_CTRL3_INVPCID                   (1 << 12)
#define VMX_VM_EXEC_CTRL3_VMFUNC_ENABLE             (1 << 13) /* VM Functions */
#define VMX_VM_EXEC_CTRL3_VMCS_SHADOWING            (1 << 14) /* VMCS Shadowing */
#define VMX_VM_EXEC_CTRL3_SGX_ENCLS_VMEXIT          (1 << 15) /* ENCLS/SGX */
#define VMX_VM_EXEC_CTRL3_RDSEED_VMEXIT             (1 << 16)
#define VMX_VM_EXEC_CTRL3_PML_ENABLE                (1 << 17) /* Page Modification Logging */
#define VMX_VM_EXEC_CTRL3_EPT_VIOLATION_EXCEPTION   (1 << 18) /* #VE Exception */
#define VMX_VM_EXEC_CTRL3_SUPPRESS_GUEST_VMX_TRACE  (1 << 19) /* Processor Trace */
#define VMX_VM_EXEC_CTRL3_XSAVES_XRSTORS            (1 << 20) /* XSAVES */
#define VMX_VM_EXEC_CTRL3_TSC_SCALING               (1 << 25) /* TSC Scaling */

#define VMX_VM_EXEC_CTRL3_SUPPORTED_BITS \
    (vmx_vmexec_ctrl2_supported_bits)

	Bit32u vmexec_ctrls3;

	Bit64u vmcs_linkptr;

	Bit64u tsc_multiplier;

	Bit32u vm_exceptions_bitmap;
	Bit32u vm_pf_mask;
	Bit32u vm_pf_match;
	Bit64u io_bitmap_addr[2];
	bx_phy_address msr_bitmap_addr;

	bx_address vm_cr0_mask;
	bx_address vm_cr0_read_shadow;
	bx_address vm_cr4_mask;
	bx_address vm_cr4_read_shadow;

#define VMX_CR3_TARGET_MAX_CNT 4

	Bit32u vm_cr3_target_cnt;
	bx_address vm_cr3_target_value[VMX_CR3_TARGET_MAX_CNT];

#if BX_SUPPORT_X86_64
	bx_phy_address virtual_apic_page_addr;
	Bit32u vm_tpr_threshold;
	bx_phy_address apic_access_page;
	unsigned apic_access;
#endif

#if BX_SUPPORT_VMX >= 2
	Bit64u eptptr;
	Bit16u vpid;
	Bit64u pml_address;
	Bit16u pml_index;
#endif

#if BX_SUPPORT_VMX >= 2
	VMX_PLE ple;
#endif

#if BX_SUPPORT_VMX >= 2
	Bit8u svi; /* Servicing Virtual Interrupt */
	Bit8u rvi; /* Requesting Virtual Interrupt */
	Bit8u vppr;

	Bit32u eoi_exit_bitmap[8];
#endif

#if BX_SUPPORT_VMX >= 2
	bx_phy_address vmread_bitmap_addr, vmwrite_bitmap_addr;
#endif

#if BX_SUPPORT_VMX >= 2
	bx_phy_address ve_info_addr;
	Bit16u eptp_index;
#endif

#if BX_SUPPORT_VMX >= 2
	Bit64u xss_exiting_bitmap;
#endif

	//
	// VM-Exit Control Fields
	//

#define VMX_VMEXIT_CTRL1_SAVE_DBG_CTRLS             (1 <<  2) /* legacy must be '1 */
#define VMX_VMEXIT_CTRL1_HOST_ADDR_SPACE_SIZE       (1 <<  9)
#define VMX_VMEXIT_CTRL1_LOAD_PERF_GLOBAL_CTRL_MSR  (1 << 12) /* Perf Global Control */
#define VMX_VMEXIT_CTRL1_INTA_ON_VMEXIT             (1 << 15)
#define VMX_VMEXIT_CTRL1_STORE_PAT_MSR              (1 << 18) /* PAT */
#define VMX_VMEXIT_CTRL1_LOAD_PAT_MSR               (1 << 19) /* PAT */
#define VMX_VMEXIT_CTRL1_STORE_EFER_MSR             (1 << 20) /* EFER */
#define VMX_VMEXIT_CTRL1_LOAD_EFER_MSR              (1 << 21) /* EFER */
#define VMX_VMEXIT_CTRL1_STORE_VMX_PREEMPTION_TIMER (1 << 22) /* VMX preemption timer */
#define VMX_VMEXIT_CTRL1_CLEAR_BNDCFGS              (1 << 23) /* MPX */
#define VMX_VMEXIT_CTRL1_SUPPRESS_VMX_PACKETS       (1 << 24) /* Processor Trace */

#define VMX_VMEXIT_CTRL1_SUPPORTED_BITS \
    (vmx_vmexit_ctrl_supported_bits)

	Bit32u vmexit_ctrls;

	Bit32u vmexit_msr_store_cnt;
	bx_phy_address vmexit_msr_store_addr;
	Bit32u vmexit_msr_load_cnt;
	bx_phy_address vmexit_msr_load_addr;

	//
	// VM-Entry Control Fields
	//

#define VMX_VMENTRY_CTRL1_LOAD_DBG_CTRLS                    (1 <<  2) /* legacy must be '1 */
#define VMX_VMENTRY_CTRL1_X86_64_GUEST                      (1 <<  9)
#define VMX_VMENTRY_CTRL1_SMM_ENTER                         (1 << 10)
#define VMX_VMENTRY_CTRL1_DEACTIVATE_DUAL_MONITOR_TREATMENT (1 << 11)
#define VMX_VMENTRY_CTRL1_LOAD_PERF_GLOBAL_CTRL_MSR         (1 << 13) /* Perf Global Ctrl */
#define VMX_VMENTRY_CTRL1_LOAD_PAT_MSR                      (1 << 14) /* PAT */
#define VMX_VMENTRY_CTRL1_LOAD_EFER_MSR                     (1 << 15) /* EFER */
#define VMX_VMENTRY_CTRL1_LOAD_BNDCFGS                      (1 << 16) /* MPX */
#define VMX_VMENTRY_CTRL1_SUPPRESS_VMX_PACKETS              (1 << 17) /* Processor Trace */

#define VMX_VMENTRY_CTRL1_SUPPORTED_BITS \
    (vmx_vmentry_ctrl_supported_bits)

	Bit32u vmentry_ctrls;

	Bit32u vmentry_msr_load_cnt;
	bx_phy_address vmentry_msr_load_addr;

	Bit32u vmentry_interr_info;
	Bit32u vmentry_excep_err_code;
	Bit32u vmentry_instr_length;

	//
	// VM Functions
	//

#if BX_SUPPORT_VMX >= 2

#define VMX_VMFUNC_CTRL1_SUPPORTED_BITS \
    (vmx_vmfunc_supported_bits)

	Bit64u vmfunc_ctrls;
	Bit64u eptp_list_address;

#endif

	//
	// VMCS Hidden and Read-Only Fields
	//
	Bit32u idt_vector_info;
	Bit32u idt_vector_error_code;

	//
	// VMCS Host State
	//

	VMCS_HOST_STATE host_state;

} VMCS_CACHE; 

#define PIN_VMEXIT(ctrl) (BX_CPU_THIS_PTR vmcs.vmexec_ctrls1 & (ctrl))
#define     VMEXIT(ctrl) (BX_CPU_THIS_PTR vmcs.vmexec_ctrls2 & (ctrl))

#define SECONDARY_VMEXEC_CONTROL(ctrl) (BX_CPU_THIS_PTR vmcs.vmexec_ctrls3 & (ctrl))

#define BX_VMX_INTERRUPTS_BLOCKED_BY_STI      (1 << 0)
#define BX_VMX_INTERRUPTS_BLOCKED_BY_MOV_SS   (1 << 1)
#define BX_VMX_INTERRUPTS_BLOCKED_SMI_BLOCKED (1 << 2)
#define BX_VMX_INTERRUPTS_BLOCKED_NMI_BLOCKED (1 << 3)

#define BX_VMX_INTERRUPTIBILITY_STATE_MASK \
    (BX_VMX_INTERRUPTS_BLOCKED_BY_STI | BX_VMX_INTERRUPTS_BLOCKED_BY_MOV_SS | \
     BX_VMX_INTERRUPTS_BLOCKED_SMI_BLOCKED | \
     BX_VMX_INTERRUPTS_BLOCKED_NMI_BLOCKED)

//
// IA32_VMX_BASIC MSR (0x480)
// --------------

#define BX_VMCS_SHADOW_BIT_MASK (0x80000000)

//
// 30:00 VMCS revision id
// 31:31 shadow VMCS indicator
// -----------------------------
// 32:47 VMCS region size, 0 <= size <= 4096
// 48:48 use 32-bit physical address, set when x86_64 disabled
// 49:49 support of dual-monitor treatment of SMI and SMM
// 53:50 memory type used for VMCS access
// 54:54 logical processor reports information in the VM-exit 
//       instruction-information field on VM exits due to
//       execution of INS/OUTS
// 55:55 set if any VMX controls that default to `1 may be
//       cleared to `0, also indicates that IA32_VMX_TRUE_PINBASED_CTLS,
//       IA32_VMX_TRUE_PROCBASED_CTLS, IA32_VMX_TRUE_EXIT_CTLS and
//       IA32_VMX_TRUE_ENTRY_CTLS MSRs are supported.
// 56:63 reserved, must be zero
//

#define VMX_MSR_VMX_BASIC_LO (BX_CPU_THIS_PTR vmcs_map->get_vmcs_revision_id())
#define VMX_MSR_VMX_BASIC_HI \
     (VMX_VMCS_AREA_SIZE | ((!is_cpu_extension_supported(BX_ISA_LONG_MODE)) << 16) | \
     (BX_MEMTYPE_WB << 18) | (1<<22)) | ((BX_SUPPORT_VMX >= 2) ? (1<<23) : 0)

#define VMX_MSR_VMX_BASIC \
   ((((Bit64u) VMX_MSR_VMX_BASIC_HI) << 32) | VMX_MSR_VMX_BASIC_LO)


// ------------------------------------------------------------------------
//              reserved bit (must be '1) settings for VMX MSRs
// ------------------------------------------------------------------------

// -----------------------------------------
//  3322|2222|2222|1111|1111|11  |    |
//  1098|7654|3210|9876|5432|1098|7654|3210
// -----------------------------------------
//  ----.----.----.----.----.----.---1.-11-  MSR (0x481) IA32_MSR_VMX_PINBASED_CTRLS
//  ----.-1--.----.---1.111-.---1.-111.--1-  MSR (0x482) IA32_MSR_VMX_PROCBASED_CTRLS
//  ----.----.----.--11.-11-.11-1.1111.1111  MSR (0x483) IA32_MSR_VMX_VMEXIT_CTRLS
//  ----.----.----.----.---1.---1.1111.1111  MSR (0x484) IA32_MSR_VMX_VMENTRY_CTRLS
//

// IA32_MSR_VMX_PINBASED_CTRLS MSR (0x481)
// ---------------------------

// Bits 1, 2 and 4 must be '1 

// Allowed 0-settings: VMentry fail if a bit is '0 in pin-based vmexec controls
// but set to '1 in this MSR
#define VMX_MSR_VMX_PINBASED_CTRLS_LO (0x00000016)
// Allowed 1-settings: VMentry fail if a bit is '1 in pin-based vmexec controls
// but set to '0 in this MSR.
#define VMX_MSR_VMX_PINBASED_CTRLS_HI \
       (VMX_VM_EXEC_CTRL1_SUPPORTED_BITS | VMX_MSR_VMX_PINBASED_CTRLS_LO)

#define VMX_MSR_VMX_PINBASED_CTRLS \
   ((((Bit64u) VMX_MSR_VMX_PINBASED_CTRLS_HI) << 32) | VMX_MSR_VMX_PINBASED_CTRLS_LO)

// IA32_MSR_VMX_TRUE_PINBASED_CTRLS MSR (0x48d)
// --------------------------------

// no changes from original IA32_MSR_VMX_PINBASED_CTRLS
#define VMX_MSR_VMX_TRUE_PINBASED_CTRLS_LO (VMX_MSR_VMX_PINBASED_CTRLS_LO)
#define VMX_MSR_VMX_TRUE_PINBASED_CTRLS_HI (VMX_MSR_VMX_PINBASED_CTRLS_HI)

#define VMX_MSR_VMX_TRUE_PINBASED_CTRLS \
   ((((Bit64u) VMX_MSR_VMX_TRUE_PINBASED_CTRLS_HI) << 32) | VMX_MSR_VMX_TRUE_PINBASED_CTRLS_LO)


// IA32_MSR_VMX_PROCBASED_CTRLS MSR (0x482)
// ----------------------------

// Bits 1, 4-6, 8, 13-16, 26 must be '1
// Bits 0, 17, 18 must be '0
// Bits 19-21 also must be '0 when x86-64 is not supported

// Allowed 0-settings (must be '1 bits)
#define VMX_MSR_VMX_PROCBASED_CTRLS_LO (0x0401E172)
// Allowed 1-settings
#define VMX_MSR_VMX_PROCBASED_CTRLS_HI \
       (VMX_VM_EXEC_CTRL2_SUPPORTED_BITS | VMX_MSR_VMX_PROCBASED_CTRLS_LO)

#define VMX_MSR_VMX_PROCBASED_CTRLS \
   ((((Bit64u) VMX_MSR_VMX_PROCBASED_CTRLS_HI) << 32) | VMX_MSR_VMX_PROCBASED_CTRLS_LO)

// IA32_MSR_VMX_TRUE_PROCBASED_CTRLS MSR (0x48e)
// ---------------------------------

// Bits 15 and 16 no longer must be '1
#define VMX_MSR_VMX_TRUE_PROCBASED_CTRLS_LO (0x04006172)
#define VMX_MSR_VMX_TRUE_PROCBASED_CTRLS_HI (VMX_MSR_VMX_PROCBASED_CTRLS_HI)

#define VMX_MSR_VMX_TRUE_PROCBASED_CTRLS \
   ((((Bit64u) VMX_MSR_VMX_TRUE_PROCBASED_CTRLS_HI) << 32) | VMX_MSR_VMX_TRUE_PROCBASED_CTRLS_LO)


// IA32_MSR_VMX_VMEXIT_CTRLS MSR (0x483)
// -------------------------

// Bits 0-8, 10, 11, 13, 14, 16, 17 must be '1

// Allowed 0-settings (must be '1 bits)
#define VMX_MSR_VMX_VMEXIT_CTRLS_LO (0x00036DFF)
// Allowed 1-settings
#define VMX_MSR_VMX_VMEXIT_CTRLS_HI \
       (VMX_VMEXIT_CTRL1_SUPPORTED_BITS | VMX_MSR_VMX_VMEXIT_CTRLS_LO)

#define VMX_MSR_VMX_VMEXIT_CTRLS \
   ((((Bit64u) VMX_MSR_VMX_VMEXIT_CTRLS_HI) << 32) | VMX_MSR_VMX_VMEXIT_CTRLS_LO)

// IA32_MSR_VMX_TRUE_VMEXIT_CTRLS MSR (0x48f)
// ------------------------------

// Bit 2 no longer must be '1
#define VMX_MSR_VMX_TRUE_VMEXIT_CTRLS_LO (0x00036DFB)
#define VMX_MSR_VMX_TRUE_VMEXIT_CTRLS_HI (VMX_MSR_VMX_VMEXIT_CTRLS_HI)

#define VMX_MSR_VMX_TRUE_VMEXIT_CTRLS \
   ((((Bit64u) VMX_MSR_VMX_TRUE_VMEXIT_CTRLS_HI) << 32) | VMX_MSR_VMX_TRUE_VMEXIT_CTRLS_LO)


// IA32_MSR_VMX_VMENTRY_CTRLS MSR (0x484)
// --------------------------

// Bits 0-8, 12 must be '1

// Allowed 0-settings (must be '1 bits)
#define VMX_MSR_VMX_VMENTRY_CTRLS_LO (0x000011FF)
// Allowed 1-settings
#define VMX_MSR_VMX_VMENTRY_CTRLS_HI \
       (VMX_VMENTRY_CTRL1_SUPPORTED_BITS | VMX_MSR_VMX_VMENTRY_CTRLS_LO)

#define VMX_MSR_VMX_VMENTRY_CTRLS \
   ((((Bit64u) VMX_MSR_VMX_VMENTRY_CTRLS_HI) << 32) | VMX_MSR_VMX_VMENTRY_CTRLS_LO)

// IA32_MSR_VMX_TRUE_VMENTRY_CTRLS MSR (0x490)
// -------------------------------

// Bit 2 is longer must be '1
#define VMX_MSR_VMX_TRUE_VMENTRY_CTRLS_LO (0x000011FB)
#define VMX_MSR_VMX_TRUE_VMENTRY_CTRLS_HI (VMX_MSR_VMX_VMENTRY_CTRLS_HI)

#define VMX_MSR_VMX_TRUE_VMENTRY_CTRLS \
   ((((Bit64u) VMX_MSR_VMX_TRUE_VMENTRY_CTRLS_HI) << 32) | VMX_MSR_VMX_TRUE_VMENTRY_CTRLS_LO)


// IA32_MSR_VMX_MISC MSR (0x485)
// -----------------

//   [4:0] - TSC:VMX_PREEMPTION_TIMER ratio
//     [5] - VMEXITs store the value of EFER.LMA into the “x86-64 guest"
//           VMENTRY control (must set to '1 if 'unrestricted guest' is supported)
//     [6] - support VMENTER to HLT state
//     [7] - support VMENTER to SHUTDOWN state
//     [8] - support VMENTER to WAIT_FOR_SIPI state
//    [15] - RDMSR can be used in SMM to read the SMBASE MSR
// [24:16] - number of CR3 target values supported
// [27:25] - (N+1)*512 - recommended maximum MSRs in MSR store list
//    [28] - MSR_IA32_SMM_MONITOR_CTL[2] enable
//    [29] - Allow VMWRITE to R/O VMCS fields (to be used with VMCS Shadowing)
// [31-30] - Reserved
// --------------------------------------------
// [63:32] - MSEG revision ID used by processor

#if BX_SUPPORT_VMX >= 2
#define VMX_MISC_STORE_LMA_TO_X86_64_GUEST_VMENTRY_CONTROL (1<<5)
#else
#define VMX_MISC_STORE_LMA_TO_X86_64_GUEST_VMENTRY_CONTROL (0)
#endif

#define VMX_SUPPORT_VMENTER_TO_NON_ACTIVE_STATE ((1<<6) | (1<<7) | (1<<8))

#define VMX_MISC_SUPPORT_VMWRITE_READ_ONLY_FIELDS (1<<29)

//Rate to increase VMX preemtion timer
#define VMX_MISC_PREEMPTION_TIMER_RATE (0)

#define VMX_MSR_MISC (VMX_MISC_PREEMPTION_TIMER_RATE | \
                      VMX_MISC_STORE_LMA_TO_X86_64_GUEST_VMENTRY_CONTROL | \
                      VMX_SUPPORT_VMENTER_TO_NON_ACTIVE_STATE | \
                     (VMX_CR3_TARGET_MAX_CNT << 16) | \
                     (BX_SUPPORT_VMX_EXTENSION(BX_VMX_VMCS_SHADOWING) ? VMX_MISC_SUPPORT_VMWRITE_READ_ONLY_FIELDS : 0))

//
// IA32_VMX_CR0_FIXED0 MSR (0x486)   IA32_VMX_CR0_FIXED1 MSR (0x487)
// -------------------               -------------------

// allowed 0-setting in CR0 in VMX mode
// bits PE(0), NE(5) and PG(31) required to be set in CR0 to enter VMX mode
#define VMX_MSR_CR0_FIXED0_LO (0x80000021)
#define VMX_MSR_CR0_FIXED0_HI (0x00000000)

#define VMX_MSR_CR0_FIXED0 \
   ((((Bit64u) VMX_MSR_CR0_FIXED0_HI) << 32) | VMX_MSR_CR0_FIXED0_LO)

// allowed 1-setting in CR0 in VMX mode
#define VMX_MSR_CR0_FIXED1_LO (0xFFFFFFFF)
#define VMX_MSR_CR0_FIXED1_HI (0x00000000)

#define VMX_MSR_CR0_FIXED1 \
   ((((Bit64u) VMX_MSR_CR0_FIXED1_HI) << 32) | VMX_MSR_CR0_FIXED1_LO)

//
// IA32_VMX_CR4_FIXED0 MSR (0x488)   IA32_VMX_CR4_FIXED1 MSR (0x489)
// -------------------               -------------------

// allowed 0-setting in CR0 in VMX mode
// bit VMXE(13) required to be set in CR4 to enter VMX mode
#define VMX_MSR_CR4_FIXED0_LO (0x00002000)
#define VMX_MSR_CR4_FIXED0_HI (0x00000000)

#define VMX_MSR_CR4_FIXED0 \
   ((((Bit64u) VMX_MSR_CR4_FIXED0_HI) << 32) | VMX_MSR_CR4_FIXED0_LO)

// allowed 1-setting in CR0 in VMX mode
#define VMX_MSR_CR4_FIXED1_LO (cr4_suppmask_1)
#define VMX_MSR_CR4_FIXED1_HI (0)

#define VMX_MSR_CR4_FIXED1 \
   ((((Bit64u) VMX_MSR_CR4_FIXED1_HI) << 32) | VMX_MSR_CR4_FIXED1_LO)


//
// IA32_VMX_VMCS_ENUM MSR (0x48a)
// ------------------

//
// 09:01 highest index value used for any VMCS encoding
// 63:10 reserved, must be zero
//

#define VMX_MSR_VMCS_ENUM_LO (VMX_HIGHEST_VMCS_ENCODING)
#define VMX_MSR_VMCS_ENUM_HI (0x00000000)

#define VMX_MSR_VMCS_ENUM \
   ((((Bit64u) VMX_MSR_VMCS_ENUM_HI) << 32) | VMX_MSR_VMCS_ENUM_LO)


// IA32_VMX_MSR_PROCBASED_CTRLS2 MSR (0x48b)
// -----------------------------

// Allowed 0-settings (must be '1 bits)
#define VMX_MSR_VMX_PROCBASED_CTRLS2_LO (0x00000000)
// Allowed 1-settings
#define VMX_MSR_VMX_PROCBASED_CTRLS2_HI \
       (VMX_VM_EXEC_CTRL3_SUPPORTED_BITS | VMX_MSR_VMX_PROCBASED_CTRLS2_LO)

#define VMX_MSR_VMX_PROCBASED_CTRLS2 \
   ((((Bit64u) VMX_MSR_VMX_PROCBASED_CTRLS2_HI) << 32) | VMX_MSR_VMX_PROCBASED_CTRLS2_LO)

#if BX_SUPPORT_VMX >= 2

// IA32_VMX_EPT_VPID_CAP MSR (0x48c)
// ---------------------

enum VMX_INVEPT_INVVPID_type {
	BX_INVEPT_INVVPID_INDIVIDUAL_ADDRESS_INVALIDATION = 0,
	BX_INVEPT_INVVPID_SINGLE_CONTEXT_INVALIDATION,
	BX_INVEPT_INVVPID_ALL_CONTEXT_INVALIDATION,
	BX_INVEPT_INVVPID_SINGLE_CONTEXT_NON_GLOBAL_INVALIDATION
};

#define VMX_MSR_VMX_EPT_VPID_CAP \
   (BX_CPU_THIS_PTR vmx_cap.vmx_ept_vpid_cap_supported_bits)

#endif

#endif // _BX_VMX_INTEL_H_

#define VMENTRY_INJECTING_EVENT(vmentry_interr_info) (vmentry_interr_info & 0x80000000)

#define VMX_CHECKS_USE_MSR_VMX_PINBASED_CTRLS_LO \
  ((BX_SUPPORT_VMX >= 2) ? VMX_MSR_VMX_TRUE_PINBASED_CTRLS_LO : VMX_MSR_VMX_PINBASED_CTRLS_LO)
#define VMX_CHECKS_USE_MSR_VMX_PINBASED_CTRLS_HI \
  ((BX_SUPPORT_VMX >= 2) ? VMX_MSR_VMX_TRUE_PINBASED_CTRLS_HI : VMX_MSR_VMX_PINBASED_CTRLS_HI)

#define VMX_CHECKS_USE_MSR_VMX_PROCBASED_CTRLS_LO \
  ((BX_SUPPORT_VMX >= 2) ? VMX_MSR_VMX_TRUE_PROCBASED_CTRLS_LO : VMX_MSR_VMX_PROCBASED_CTRLS_LO)
#define VMX_CHECKS_USE_MSR_VMX_PROCBASED_CTRLS_HI \
  ((BX_SUPPORT_VMX >= 2) ? VMX_MSR_VMX_TRUE_PROCBASED_CTRLS_HI : VMX_MSR_VMX_PROCBASED_CTRLS_HI)

#define VMX_CHECKS_USE_MSR_VMX_VMEXIT_CTRLS_LO \
  ((BX_SUPPORT_VMX >= 2) ? VMX_MSR_VMX_TRUE_VMEXIT_CTRLS_LO : VMX_MSR_VMX_VMEXIT_CTRLS_LO)
#define VMX_CHECKS_USE_MSR_VMX_VMEXIT_CTRLS_HI \
  ((BX_SUPPORT_VMX >= 2) ? VMX_MSR_VMX_TRUE_VMEXIT_CTRLS_HI : VMX_MSR_VMX_VMEXIT_CTRLS_HI)

#define VMX_CHECKS_USE_MSR_VMX_VMENTRY_CTRLS_LO \
  ((BX_SUPPORT_VMX >= 2) ? VMX_MSR_VMX_TRUE_VMENTRY_CTRLS_LO : VMX_MSR_VMX_VMENTRY_CTRLS_LO)
#define VMX_CHECKS_USE_MSR_VMX_VMENTRY_CTRLS_HI \
  ((BX_SUPPORT_VMX >= 2) ? VMX_MSR_VMX_TRUE_VMENTRY_CTRLS_HI : VMX_MSR_VMX_VMENTRY_CTRLS_HI)

#define BX_EVENT_NMI                          (1 <<  0)
#define BX_EVENT_SMI                          (1 <<  1)
#define BX_EVENT_INIT                         (1 <<  2)
#define BX_EVENT_CODE_BREAKPOINT_ASSIST       (1 <<  3)
#define BX_EVENT_VMX_MONITOR_TRAP_FLAG        (1 <<  4)
#define BX_EVENT_VMX_PREEMPTION_TIMER_EXPIRED (1 <<  5)
#define BX_EVENT_VMX_INTERRUPT_WINDOW_EXITING (1 <<  6)
#define BX_EVENT_VMX_VIRTUAL_NMI              (1 <<  7)
#define BX_EVENT_SVM_VIRQ_PENDING             (1 <<  8)
#define BX_EVENT_PENDING_VMX_VIRTUAL_INTR     (1 <<  9)
#define BX_EVENT_PENDING_INTR                 (1 << 10)
#define BX_EVENT_PENDING_LAPIC_INTR           (1 << 11)
#define BX_EVENT_VMX_VTPR_UPDATE              (1 << 12)
#define BX_EVENT_VMX_VEOI_UPDATE              (1 << 13)
#define BX_EVENT_VMX_VIRTUAL_APIC_WRITE       (1 << 14)


// exception types for interrupt method
enum {
	BX_EXTERNAL_INTERRUPT = 0,
	BX_NMI = 2,
	BX_HARDWARE_EXCEPTION = 3,  // all exceptions except #BP and #OF
	BX_SOFTWARE_INTERRUPT = 4,
	BX_PRIVILEGED_SOFTWARE_INTERRUPT = 5,
	BX_SOFTWARE_EXCEPTION = 6
};

typedef enum {
	BX_ACTIVITY_STATE_ACTIVE = 0,
	BX_ACTIVITY_STATE_HLT,
	BX_ACTIVITY_STATE_SHUTDOWN,
	BX_ACTIVITY_STATE_WAIT_FOR_SIPI,
	BX_ACTIVITY_STATE_MWAIT,
	BX_ACTIVITY_STATE_MWAIT_IF
}CPU_Activity_State;


#define BX_CR0_PE_MASK         (1 <<  0)
#define BX_CR0_MP_MASK         (1 <<  1)
#define BX_CR0_EM_MASK         (1 <<  2)
#define BX_CR0_TS_MASK         (1 <<  3)
#define BX_CR0_ET_MASK         (1 <<  4)
#define BX_CR0_NE_MASK         (1 <<  5)
#define BX_CR0_WP_MASK         (1 << 16)
#define BX_CR0_AM_MASK         (1 << 18)
#define BX_CR0_NW_MASK         (1 << 29)
#define BX_CR0_CD_MASK         (1 << 30)
#define BX_CR0_PG_MASK         (1 << 31)



#define BX_CR4_VME_MASK        (1 << 0)
#define BX_CR4_PVI_MASK        (1 << 1)
#define BX_CR4_TSD_MASK        (1 << 2)
#define BX_CR4_DE_MASK         (1 << 3)
#define BX_CR4_PSE_MASK        (1 << 4)
#define BX_CR4_PAE_MASK        (1 << 5)
#define BX_CR4_MCE_MASK        (1 << 6)
#define BX_CR4_PGE_MASK        (1 << 7)
#define BX_CR4_PCE_MASK        (1 << 8)
#define BX_CR4_OSFXSR_MASK     (1 << 9)
#define BX_CR4_OSXMMEXCPT_MASK (1 << 10)
#define BX_CR4_UMIP_MASK       (1 << 11)
#define BX_CR4_VMXE_MASK       (1 << 13)
#define BX_CR4_SMXE_MASK       (1 << 14)
#define BX_CR4_FSGSBASE_MASK   (1 << 16)
#define BX_CR4_PCIDE_MASK      (1 << 17)
#define BX_CR4_OSXSAVE_MASK    (1 << 18)
#define BX_CR4_SMEP_MASK       (1 << 20)
#define BX_CR4_SMAP_MASK       (1 << 21)
#define BX_CR4_PKE_MASK        (1 << 22)

// segment register encoding
typedef enum {
	BX_SEG_REG_ES = 0,
	BX_SEG_REG_CS = 1,
	BX_SEG_REG_SS = 2,
	BX_SEG_REG_DS = 3,
	BX_SEG_REG_FS = 4,
	BX_SEG_REG_GS = 5,
	// NULL now has to fit in 3 bits.
	BX_SEG_REG_NULL = 7
}BxSegregs;

// For data/code descriptors:

#define BX_DATA_READ_ONLY                       (0x0)
#define BX_DATA_READ_ONLY_ACCESSED              (0x1)
#define BX_DATA_READ_WRITE                      (0x2)
#define BX_DATA_READ_WRITE_ACCESSED             (0x3)
#define BX_DATA_READ_ONLY_EXPAND_DOWN           (0x4)
#define BX_DATA_READ_ONLY_EXPAND_DOWN_ACCESSED  (0x5)
#define BX_DATA_READ_WRITE_EXPAND_DOWN          (0x6)
#define BX_DATA_READ_WRITE_EXPAND_DOWN_ACCESSED (0x7)
#define BX_CODE_EXEC_ONLY                       (0x8)
#define BX_CODE_EXEC_ONLY_ACCESSED              (0x9)
#define BX_CODE_EXEC_READ                       (0xa)
#define BX_CODE_EXEC_READ_ACCESSED              (0xb)
#define BX_CODE_EXEC_ONLY_CONFORMING            (0xc)
#define BX_CODE_EXEC_ONLY_CONFORMING_ACCESSED   (0xd)
#define BX_CODE_EXEC_READ_CONFORMING            (0xe)
#define BX_CODE_EXEC_READ_CONFORMING_ACCESSED   (0xf)

// For system & gate descriptors:

#define BX_GATE_TYPE_NONE                       (0x0)
#define BX_SYS_SEGMENT_AVAIL_286_TSS            (0x1)
#define BX_SYS_SEGMENT_LDT                      (0x2)
#define BX_SYS_SEGMENT_BUSY_286_TSS             (0x3)
#define BX_286_CALL_GATE                        (0x4)
#define BX_TASK_GATE                            (0x5)
#define BX_286_INTERRUPT_GATE                   (0x6)
#define BX_286_TRAP_GATE                        (0x7)
											  /* 0x8 reserved */
#define BX_SYS_SEGMENT_AVAIL_386_TSS            (0x9)
											  /* 0xa reserved */
#define BX_SYS_SEGMENT_BUSY_386_TSS             (0xb)
#define BX_386_CALL_GATE                        (0xc)
											  /* 0xd reserved */
#define BX_386_INTERRUPT_GATE                   (0xe)
#define BX_386_TRAP_GATE                        (0xf)


#define BX_VMX_LAST_ACTIVITY_STATE (BX_ACTIVITY_STATE_WAIT_FOR_SIPI)


static const Bit32u EFlagsCFMask = (1 << 0);
static const Bit32u EFlagsPFMask = (1 << 2);
static const Bit32u EFlagsAFMask = (1 << 4);
static const Bit32u EFlagsZFMask = (1 << 6);
static const Bit32u EFlagsSFMask = (1 << 7);
static const Bit32u EFlagsTFMask = (1 << 8);
static const Bit32u EFlagsIFMask = (1 << 9);
static const Bit32u EFlagsDFMask = (1 << 10);
static const Bit32u EFlagsOFMask = (1 << 11);
static const Bit32u EFlagsIOPLMask = (3 << 12);
static const Bit32u EFlagsNTMask = (1 << 14);
static const Bit32u EFlagsRFMask = (1 << 16);
static const Bit32u EFlagsVMMask = (1 << 17);
static const Bit32u EFlagsACMask = (1 << 18);
static const Bit32u EFlagsVIFMask = (1 << 19);
static const Bit32u EFlagsVIPMask = (1 << 20);
static const Bit32u EFlagsIDMask = (1 << 21);

#define BX_INHIBIT_INTERRUPTS        0x01
#define BX_INHIBIT_DEBUG             0x02
#define BX_INHIBIT_INTERRUPTS_BY_MOVSS        \
    (BX_INHIBIT_INTERRUPTS | BX_INHIBIT_DEBUG)

// possible types passed to BX_INSTR_TLB_CNTRL()
typedef enum {
	BX_INSTR_MOV_CR0 = 10,
	BX_INSTR_MOV_CR3 = 11,
	BX_INSTR_MOV_CR4 = 12,
	BX_INSTR_TASK_SWITCH = 13,
	BX_INSTR_CONTEXT_SWITCH = 14,
	BX_INSTR_INVLPG = 15,
	BX_INSTR_INVEPT = 16,
	BX_INSTR_INVVPID = 17,
	BX_INSTR_INVPCID = 18
}BX_Instr_TLBControl;

// static member functions.  With SMF, there is only one CPU by definition.
#  define BX_CPU_THIS_PTR  BX_CPU(0)->
#  define BX_CPU_THIS      BX_CPU(0)
#  define BX_SMF           static
#  define BX_CPU_CALL_METHOD(func, args) \
            ((BxExecutePtr_tR) (func)) args
#  define BX_CPU_CALL_REP_ITERATION(func, args) \
            ((BxRepIterationPtr_tR) (func)) args


typedef enum {
	BX_DE_EXCEPTION = 0, // Divide Error (fault)
	BX_DB_EXCEPTION = 1, // Debug (fault/trap)
	BX_BP_EXCEPTION = 3, // Breakpoint (trap)
	BX_OF_EXCEPTION = 4, // Overflow (trap)
	BX_BR_EXCEPTION = 5, // BOUND (fault)
	BX_UD_EXCEPTION = 6,
	BX_NM_EXCEPTION = 7,
	BX_DF_EXCEPTION = 8,
	BX_TS_EXCEPTION = 10,
	BX_NP_EXCEPTION = 11,
	BX_SS_EXCEPTION = 12,
	BX_GP_EXCEPTION = 13,
	BX_PF_EXCEPTION = 14,
	BX_MF_EXCEPTION = 16,
	BX_AC_EXCEPTION = 17,
	BX_MC_EXCEPTION = 18,
	BX_XM_EXCEPTION = 19,
	BX_VE_EXCEPTION = 20
}BX_Exception;

typedef enum {
	BX_MODE_IA32_REAL = 0,        // CR0.PE=0                |
	BX_MODE_IA32_V8086 = 1,       // CR0.PE=1, EFLAGS.VM=1   | EFER.LMA=0
	BX_MODE_IA32_PROTECTED = 2,   // CR0.PE=1, EFLAGS.VM=0   |
	BX_MODE_LONG_COMPAT = 3,      // EFER.LMA = 1, CR0.PE=1, CS.L=0
	BX_MODE_LONG_64 = 4           // EFER.LMA = 1, CR0.PE=1, CS.L=1
}BxCpuMode;

// local apic registers

#define BX_LAPIC_ID                   0x020
#define BX_LAPIC_VERSION              0x030
#define BX_LAPIC_TPR                  0x080
#define BX_LAPIC_ARBITRATION_PRIORITY 0x090
#define BX_LAPIC_PPR                  0x0A0
#define BX_LAPIC_EOI                  0x0B0
#define BX_LAPIC_RRD                  0x0C0
#define BX_LAPIC_LDR                  0x0D0
#define BX_LAPIC_DESTINATION_FORMAT   0x0E0
#define BX_LAPIC_SPURIOUS_VECTOR      0x0F0
#define BX_LAPIC_ISR1                 0x100
#define BX_LAPIC_ISR2                 0x110
#define BX_LAPIC_ISR3                 0x120
#define BX_LAPIC_ISR4                 0x130
#define BX_LAPIC_ISR5                 0x140
#define BX_LAPIC_ISR6                 0x150
#define BX_LAPIC_ISR7                 0x160
#define BX_LAPIC_ISR8                 0x170
#define BX_LAPIC_TMR1                 0x180
#define BX_LAPIC_TMR2                 0x190
#define BX_LAPIC_TMR3                 0x1A0
#define BX_LAPIC_TMR4                 0x1B0
#define BX_LAPIC_TMR5                 0x1C0
#define BX_LAPIC_TMR6                 0x1D0
#define BX_LAPIC_TMR7                 0x1E0
#define BX_LAPIC_TMR8                 0x1F0
#define BX_LAPIC_IRR1                 0x200
#define BX_LAPIC_IRR2                 0x210
#define BX_LAPIC_IRR3                 0x220
#define BX_LAPIC_IRR4                 0x230
#define BX_LAPIC_IRR5                 0x240
#define BX_LAPIC_IRR6                 0x250
#define BX_LAPIC_IRR7                 0x260
#define BX_LAPIC_IRR8                 0x270
#define BX_LAPIC_ESR                  0x280
#define BX_LAPIC_LVT_CMCI             0x2F0
#define BX_LAPIC_ICR_LO               0x300
#define BX_LAPIC_ICR_HI               0x310
#define BX_LAPIC_LVT_TIMER            0x320
#define BX_LAPIC_LVT_THERMAL          0x330
#define BX_LAPIC_LVT_PERFMON          0x340
#define BX_LAPIC_LVT_LINT0            0x350
#define BX_LAPIC_LVT_LINT1            0x360
#define BX_LAPIC_LVT_ERROR            0x370
#define BX_LAPIC_TIMER_INITIAL_COUNT  0x380
#define BX_LAPIC_TIMER_CURRENT_COUNT  0x390
#define BX_LAPIC_TIMER_DIVIDE_CFG     0x3E0
#define BX_LAPIC_SELF_IPI             0x3F0

// extended AMD 
#define BX_LAPIC_EXT_APIC_FEATURE     0x400
#define BX_LAPIC_EXT_APIC_CONTROL     0x410
#define BX_LAPIC_SPECIFIC_EOI         0x420
#define BX_LAPIC_IER1                 0x480
#define BX_LAPIC_IER2                 0x490
#define BX_LAPIC_IER3                 0x4A0
#define BX_LAPIC_IER4                 0x4B0
#define BX_LAPIC_IER5                 0x4C0
#define BX_LAPIC_IER6                 0x4D0
#define BX_LAPIC_IER7                 0x4E0
#define BX_LAPIC_IER8                 0x4F0


static const unsigned BX_CPU_HANDLED_EXCEPTIONS = 32;


// cpuid VMX features
#define BX_VMX_TPR_SHADOW            (1 <<  0)              /* TPR shadow */
#define BX_VMX_VIRTUAL_NMI           (1 <<  1)              /* Virtual NMI */
#define BX_VMX_APIC_VIRTUALIZATION   (1 <<  2)              /* APIC Access Virtualization */
#define BX_VMX_WBINVD_VMEXIT         (1 <<  3)              /* WBINVD VMEXIT */
#define BX_VMX_PERF_GLOBAL_CTRL      (1 <<  4)              /* Save/Restore MSR_PERF_GLOBAL_CTRL */
#define BX_VMX_MONITOR_TRAP_FLAG     (1 <<  5)              /* Monitor trap Flag (MTF) */
#define BX_VMX_X2APIC_VIRTUALIZATION (1 <<  6)              /* Virtualize X2APIC */
#define BX_VMX_EPT                   (1 <<  7)              /* Extended Page Tables (EPT) */
#define BX_VMX_VPID                  (1 <<  8)              /* VPID */
#define BX_VMX_UNRESTRICTED_GUEST    (1 <<  9)              /* Unrestricted Guest */
#define BX_VMX_PREEMPTION_TIMER      (1 << 10)              /* VMX preemption timer */
#define BX_VMX_SAVE_DEBUGCTL_DISABLE (1 << 11)              /* Disable Save/Restore of MSR_DEBUGCTL */
#define BX_VMX_PAT                   (1 << 12)              /* Save/Restore MSR_PAT */
#define BX_VMX_EFER                  (1 << 13)              /* Save/Restore MSR_EFER */
#define BX_VMX_DESCRIPTOR_TABLE_EXIT (1 << 14)              /* Descriptor Table VMEXIT */
#define BX_VMX_PAUSE_LOOP_EXITING    (1 << 15)              /* Pause Loop Exiting */
#define BX_VMX_EPTP_SWITCHING        (1 << 16)              /* EPTP switching (VM Function 0) */
#define BX_VMX_EPT_ACCESS_DIRTY      (1 << 17)              /* Extended Page Tables (EPT) A/D Bits */
#define BX_VMX_VINTR_DELIVERY        (1 << 18)              /* Virtual Interrupt Delivery */
#define BX_VMX_POSTED_INSTERRUPTS    (1 << 19)              /* Posted Interrupts support - not implemented yet */
#define BX_VMX_VMCS_SHADOWING        (1 << 20)              /* VMCS Shadowing */
#define BX_VMX_EPT_EXCEPTION         (1 << 21)              /* EPT Violation (#VE) exception */
#define BX_VMX_PML                   (1 << 22)              /* Page Modification Logging - not implemented yet */
#define BX_VMX_TSC_SCALING           (1 << 23)              /* TSC Scaling */



#if BX_SUPPORT_X86_64
#define FMT_ADDRX FMT_ADDRX64
#else
#define FMT_ADDRX FMT_ADDRX32
#endif

#define BX_SELECTOR_RPL(selector) ((selector) & 0x03)
#define BX_SELECTOR_RPL_MASK (0xfffc)


#define BX_EFER_SCE_MASK       (1 <<  0)
#define BX_EFER_LME_MASK       (1 <<  8)
#define BX_EFER_LMA_MASK       (1 << 10)
#define BX_EFER_NXE_MASK       (1 << 11)
#define BX_EFER_SVME_MASK      (1 << 12)
#define BX_EFER_LMSLE_MASK     (1 << 13)
#define BX_EFER_FFXSR_MASK     (1 << 14)
#define BX_EFER_TCE_MASK       (1 << 15)


#define BX_DEBUG_TRAP_HIT             (1 << 12)
#define BX_DEBUG_DR_ACCESS_BIT        (1 << 13)
#define BX_DEBUG_SINGLE_STEP_BIT      (1 << 14)
#define BX_DEBUG_TRAP_TASK_SWITCH_BIT (1 << 15)


#define BX_SUPPORT_VMX_EXTENSION(feature_mask) \
   (vmx_extensions_bitmask & (feature_mask))

#define BX_SUPPORT_SVM_EXTENSION(feature_mask) \
   (BX_CPU_THIS_PTR svm_extensions_bitmask & (feature_mask))


#define BX_PHY_ADDRESS_WIDTH 40

#define BX_PHY_ADDRESS_MASK ((((Bit64u)(1)) << BX_PHY_ADDRESS_WIDTH) - 1)
#define BX_PHY_ADDRESS_RESERVED_BITS (~BX_PHY_ADDRESS_MASK)

#if BX_SUPPORT_X86_64
#define BX_LIN_ADDRESS_WIDTH 48
#else
#define BX_LIN_ADDRESS_WIDTH 32
#endif

#define BX_TRUE  (1)
#define BX_FALSE (0)

enum {
	BX_MEMTYPE_UC = 0,
	BX_MEMTYPE_WC = 1,
	BX_MEMTYPE_RESERVED2 = 2,
	BX_MEMTYPE_RESERVED3 = 3,
	BX_MEMTYPE_WT = 4,
	BX_MEMTYPE_WP = 5,
	BX_MEMTYPE_WB = 6,
	BX_MEMTYPE_UC_WEAK = 7, // PAT only
	BX_MEMTYPE_INVALID = 8
};

#define GET32L(val64) ((Bit32u)(((Bit64u)(val64)) & 0xFFFFFFFF))
#define GET32H(val64) ((Bit32u)(((Bit64u)(val64)) >> 32))

#define BX_PAGING_PHY_ADDRESS_RESERVED_BITS \
    (BX_PHY_ADDRESS_RESERVED_BITS & BX_CONST64(0xfffffffffffff))

#define PAGE_DIRECTORY_NX_BIT (BX_CONST64(0x8000000000000000))

#define BX_CR3_PAGING_MASK    (BX_CONST64(0x000ffffffffff000))

#define PAGING_PAE_PDPTE_RESERVED_BITS \
    (BX_PAGING_PHY_ADDRESS_RESERVED_BITS | BX_CONST64(0xFFF00000000001E6))


/*
#define MSR_APIC_BASE                       0x01B
#define MSR_IA32_FEATURE_CONTROL            0x03A

#define MSR_IA32_VMX_BASIC                  0x480
#define MSR_IA32_VMX_PINBASED_CTLS          0x481
#define MSR_IA32_VMX_PROCBASED_CTLS         0x482
#define MSR_IA32_VMX_EXIT_CTLS              0x483
#define MSR_IA32_VMX_ENTRY_CTLS             0x484
#define MSR_IA32_VMX_MISC                   0x485
#define MSR_IA32_VMX_CR0_FIXED0             0x486
#define MSR_IA32_VMX_CR0_FIXED1             0x487
#define MSR_IA32_VMX_CR4_FIXED0             0x488
#define MSR_IA32_VMX_CR4_FIXED1             0x489
#define MSR_IA32_VMX_VMCS_ENUM              0x48A
#define MSR_IA32_VMX_PROCBASED_CTLS2        0x48B
#define MSR_IA32_VMX_EPT_VPID_CAP           0x48C
#define MSR_IA32_VMX_TRUE_PINBASED_CTLS     0x48D
#define MSR_IA32_VMX_TRUE_PROCBASED_CTLS    0x48E
#define MSR_IA32_VMX_TRUE_EXIT_CTLS         0x48F
#define MSR_IA32_VMX_TRUE_ENTRY_CTLS        0x490
#define MSR_IA32_VMX_VMFUNC                 0x491

#define MSR_IA32_SYSENTER_CS                0x174
#define MSR_IA32_SYSENTER_ESP               0x175
#define MSR_IA32_SYSENTER_EIP               0x176
#define MSR_IA32_DEBUGCTL                   0x1D9

#define MSR_LSTAR                           0xC0000082

#define MSR_FS_BASE                         0xC0000100
#define MSR_GS_BASE                         0xC0000101
#define MSR_SHADOW_GS_BASE                  0xC0000102
*/
enum {
	BX_ET_BENIGN = 0,
	BX_ET_CONTRIBUTORY = 1,
	BX_ET_PAGE_FAULT = 2,
	BX_ET_DOUBLE_FAULT = 10
};

enum {
	BX_EXCEPTION_CLASS_TRAP = 0,
	BX_EXCEPTION_CLASS_FAULT = 1,
	BX_EXCEPTION_CLASS_ABORT = 2
};

typedef struct {
	unsigned exception_type;
	unsigned exception_class;
	bx_bool push_error;
}BxExceptionInfo;



#define ubyte(i) _ubyte[(i)]


typedef union bx_packed_reg_t {
	Bit8s   _sbyte[8];
	Bit16s  _s16[4];
	Bit32s  _s32[2];
	Bit64s  _s64;
	Bit8u   _ubyte[8];
	Bit16u  _u16[4];
	Bit32u  _u32[2];
	Bit64u  _u64;
//public:
//	bx_packed_reg_t() {}
//	bx_packed_reg_t(Bit64u val) : _u64(val) {}
//	bx_packed_reg_t(Bit64s val) : _s64(val) {}
} BxPackedRegister;



BOOLEAN CheckVMXState(VMCS_CACHE *pVm, BOOLEAN IsVMResume, UINT64 VMXON_Pointer, INT32 RevisionID,
	UINT32 _vmx_pin_vmexec_ctrl_supported_bits, UINT32 _vmx_proc_vmexec_ctrl_supported_bits,
	UINT32 _vmx_vmexec_ctrl2_supported_bits, UINT32 _vmx_vmexit_ctrl_supported_bits,
	UINT32 _vmx_vmentry_ctrl_supported_bits, UINT64 _vmx_ept_vpid_cap_supported_bits,
	UINT64 _vmx_vmfunc_supported_bits, UINT32 _cr0_suppmask_0, UINT32 _cr0_suppmask_1,
	UINT32 _cr4_suppmask_0, UINT32 _cr4_suppmask_1);

int audit_vmcs(BOOLEAN IsVMResume, UINT64 RevisionID, UINT64 VMXON_Pointer);

#endif /* __VMCS_AUDITOR_H */
