/*
 * vmx.h: VMX Architecture related definitions
 * Copyright (c) 2004, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 */
#ifndef __VMX_H__
#define __VMX_H__

#include <vmm_types.h>
#include <vm/vmcs.h>

#define VMX_FAIL_INVALID	-1
#define VMX_FAIL_VALID          -2
#define VMX_FAIL_UD_GF		-3

#define EPT_TABLE_ORDER         9
#define EPTE_SUPER_PAGE_MASK    0x80
#define EPTE_MFN_MASK           0xffffffffff000ULL
#define EPTE_AVAIL1_MASK        0xF00
#define EPTE_EMT_MASK           0x38
#define EPTE_IGMT_MASK          0x40
#define EPTE_AVAIL1_SHIFT       8
#define EPTE_EMT_SHIFT          3
#define EPTE_IGMT_SHIFT         6

/*
 * Exit Reasons
 */
#define VMX_EXIT_REASONS_FAILED_VMENTRY 0x80000000

#define EXIT_REASON_EXCEPTION_NMI       0
#define EXIT_REASON_EXTERNAL_INTERRUPT  1
#define EXIT_REASON_TRIPLE_FAULT        2
#define EXIT_REASON_INIT                3
#define EXIT_REASON_SIPI                4
#define EXIT_REASON_IO_SMI              5
#define EXIT_REASON_OTHER_SMI           6
#define EXIT_REASON_PENDING_VIRT_INTR   7
#define EXIT_REASON_PENDING_VIRT_NMI    8
#define EXIT_REASON_TASK_SWITCH         9
#define EXIT_REASON_CPUID               10
#define EXIT_REASON_HLT                 12
#define EXIT_REASON_INVD                13
#define EXIT_REASON_INVLPG              14
#define EXIT_REASON_RDPMC               15
#define EXIT_REASON_RDTSC               16
#define EXIT_REASON_RSM                 17
#define EXIT_REASON_VMCALL              18
#define EXIT_REASON_VMCLEAR             19
#define EXIT_REASON_VMLAUNCH            20
#define EXIT_REASON_VMPTRLD             21
#define EXIT_REASON_VMPTRST             22
#define EXIT_REASON_VMREAD              23
#define EXIT_REASON_VMRESUME            24
#define EXIT_REASON_VMWRITE             25
#define EXIT_REASON_VMXOFF              26
#define EXIT_REASON_VMXON               27
#define EXIT_REASON_CR_ACCESS           28
#define EXIT_REASON_DR_ACCESS           29
#define EXIT_REASON_IO_INSTRUCTION      30
#define EXIT_REASON_MSR_READ            31
#define EXIT_REASON_MSR_WRITE           32
#define EXIT_REASON_INVALID_GUEST_STATE 33
#define EXIT_REASON_MSR_LOADING         34
#define EXIT_REASON_MWAIT_INSTRUCTION   36
#define EXIT_REASON_MONITOR_TRAP_FLAG   37
#define EXIT_REASON_MONITOR_INSTRUCTION 39
#define EXIT_REASON_PAUSE_INSTRUCTION   40
#define EXIT_REASON_MCE_DURING_VMENTRY  41
#define EXIT_REASON_TPR_BELOW_THRESHOLD 43
#define EXIT_REASON_APIC_ACCESS         44
#define EXIT_REASON_EPT_VIOLATION       48
#define EXIT_REASON_EPT_MISCONFIG       49
#define EXIT_REASON_RDTSCP              51
#define EXIT_REASON_WBINVD              54
#define EXIT_REASON_XSETBV              55

/*
 * Interruption-information format
 */
#define INTR_INFO_VECTOR_MASK           0xff            /* 7:0 */
#define INTR_INFO_INTR_TYPE_MASK        0x700           /* 10:8 */
#define INTR_INFO_DELIVER_CODE_MASK     0x800           /* 11 */
#define INTR_INFO_NMI_UNBLOCKED_BY_IRET 0x1000          /* 12 */
#define INTR_INFO_VALID_MASK            0x80000000      /* 31 */
#define INTR_INFO_RESVD_BITS_MASK       0x7ffff000

/*
 * Exit Qualifications for MOV for Control Register Access
 */
 /* 3:0 - control register number (CRn) */
#define VMX_CONTROL_REG_ACCESS_NUM      0xf
 /* 5:4 - access type (CR write, CR read, CLTS, LMSW) */
#define VMX_CONTROL_REG_ACCESS_TYPE     0x30
 /* 10:8 - general purpose register operand */
#define VMX_CONTROL_REG_ACCESS_GPR      0xf00
#define VMX_CONTROL_REG_ACCESS_TYPE_MOV_TO_CR   (0 << 4)
#define VMX_CONTROL_REG_ACCESS_TYPE_MOV_FROM_CR (1 << 4)
#define VMX_CONTROL_REG_ACCESS_TYPE_CLTS        (2 << 4)
#define VMX_CONTROL_REG_ACCESS_TYPE_LMSW        (3 << 4)
#define VMX_CONTROL_REG_ACCESS_GPR_EAX  (0 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_ECX  (1 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_EDX  (2 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_EBX  (3 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_ESP  (4 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_EBP  (5 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_ESI  (6 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_EDI  (7 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_R8   (8 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_R9   (9 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_R10  (10 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_R11  (11 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_R12  (12 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_R13  (13 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_R14  (14 << 8)
#define VMX_CONTROL_REG_ACCESS_GPR_R15  (15 << 8)

/*
 * Access Rights
 */
#define X86_SEG_AR_SEG_TYPE     0xf        /* 3:0, segment type */
#define X86_SEG_AR_DESC_TYPE    (1u << 4)  /* 4, descriptor type */
#define X86_SEG_AR_DPL          0x60       /* 6:5, descriptor privilege level */
#define X86_SEG_AR_SEG_PRESENT  (1u << 7)  /* 7, segment present */
#define X86_SEG_AR_AVL          (1u << 12) /* 12, available for system software */
#define X86_SEG_AR_CS_LM_ACTIVE (1u << 13) /* 13, long mode active (CS only) */
#define X86_SEG_AR_DEF_OP_SIZE  (1u << 14) /* 14, default operation size */
#define X86_SEG_AR_GRANULARITY  (1u << 15) /* 15, granularity */
#define X86_SEG_AR_SEG_UNUSABLE (1u << 16) /* 16, segment unusable */

extern u64 vmx_ept_vpid_cap;

#define cpu_has_vmx_ept_wl4_supported				\
	(vmx_ept_vpid_cap & VMX_EPT_WALK_LENGTH_4_SUPPORTED)
#define cpu_has_vmx_ept_mt_uc				\
	(vmx_ept_vpid_cap & VMX_EPT_MEMORY_TYPE_UC)
#define cpu_has_vmx_ept_mt_wb				\
	(vmx_ept_vpid_cap & VMX_EPT_MEMORY_TYPE_WB)
#define cpu_has_vmx_ept_2MB				\
	(vmx_ept_vpid_cap & VMX_EPT_SUPERPAGE_2MB)
#define cpu_has_vmx_ept_invept_single_context		\
	(vmx_ept_vpid_cap & VMX_EPT_INVEPT_SINGLE_CONTEXT)

#define INVEPT_SINGLE_CONTEXT   1
#define INVEPT_ALL_CONTEXT      2

#define cpu_has_vmx_vpid_invvpid_individual_addr		\
	(vmx_ept_vpid_cap & VMX_VPID_INVVPID_INDIVIDUAL_ADDR)
#define cpu_has_vmx_vpid_invvpid_single_context			\
	(vmx_ept_vpid_cap & VMX_VPID_INVVPID_SINGLE_CONTEXT)
#define cpu_has_vmx_vpid_invvpid_single_context_retaining_global	\
	(vmx_ept_vpid_cap & VMX_VPID_INVVPID_SINGLE_CONTEXT_RETAINING_GLOBAL)

#define INVVPID_INDIVIDUAL_ADDR                 0
#define INVVPID_SINGLE_CONTEXT                  1
#define INVVPID_ALL_CONTEXT                     2
#define INVVPID_SINGLE_CONTEXT_RETAINING_GLOBAL 3

#define __FIXUP_ALIGN ".align 8"
#define __FIXUP_WORD  ".quad"

static inline int __vmptrld(u64 addr)
{
	int rc = 0;

	asm volatile("1: vmptrld (%1) \n\t"
		     "jz 5f\n"
		     "jc 2f\n"
		     "jmp 3f\n"
		     "5:sub $%c4, %0\n"
		     "jmp  3f\n"
		     "2:sub  $%c3, %0\n"
		     "3:\n"
		     ".section .fixup,\"ax\"\n"
		     "4:sub $%c5, %0 ; jmp 3b\n"
		     ".previous\n"
		     ".section __ex_table,\"a\"\n"
		     "   "__FIXUP_ALIGN"\n"
		     "   "__FIXUP_WORD" 1b,4b\n"
		     ".previous\n"
		     : "=a"(rc)
		     : "c"(&addr), "0"(0),  "i"(-VMX_FAIL_INVALID),
		       "i"(-VMX_FAIL_VALID), "i"(-VMX_FAIL_UD_GF)
		     : "memory", "cc");

	return rc;
}

static inline u64 __vmptrst(void)
{
        u64 addr = 0;
	int rc = VMM_OK;

	asm volatile("1: vmptrst %1 \n\t"
		     "2:\n"
		     ".section .fixup,\"ax\"\n"
		     "3: sub $%c2, %0 ; jmp 2b\n"
		     ".previous\n"
		     ".section __ex_table,\"a\"\n"
		     "   "__FIXUP_ALIGN"\n"
		     "   "__FIXUP_WORD" 1b,3b\n"
		     ".previous\n"
		     : "=q" (rc), "=m"(addr)
		     : "i"(-VMX_FAIL_UD_GF)
		     : "memory");

        return addr;
}

static inline int __vmpclear(u64 addr)
{
	int rc = VMM_OK;

	asm volatile("1: vmclear (%1) \n\t"
		     "jz 5f\n"
		     "jc 2f\n"
		     "jmp 3f\n"
		     "5:sub $%c4, %0\n"
		     "jmp  3f\n"
		     "2:sub  $%c3, %0\n"
		     "3:\n"
		     ".section .fixup,\"ax\"\n"
		     "4:sub $%c5, %0 ; jmp 3b\n"
		     ".previous\n"
		     ".section __ex_table,\"a\"\n"
		     "   "__FIXUP_ALIGN"\n"
		     "   "__FIXUP_WORD" 1b,4b\n"
		     ".previous\n"
		     : "=a"(rc)
		     : "c"(&addr), "0"(0),  "i"(-VMX_FAIL_INVALID),
		       "i"(-VMX_FAIL_VALID), "i"(-VMX_FAIL_UD_GF)
		     : "memory", "cc");

	return rc;
}

static inline int __vmread(u64 field, u64 *value)
{
	unsigned long edx;
	int rc = VMM_OK;

	asm volatile ("1: vmread %2, %1\n\t"
		      "jz 5f\n"
		      "jc 2f\n"
		      "jmp 3f\n"
		      "5:sub $%c4, %0\n"
		      "jmp  3f\n"
		      "2:sub  $%c3, %0\n"
		      "3:\n"
		      ".section .fixup,\"ax\"\n"
		      "4:sub $%c5, %0 ; jmp 3b\n"
		      ".previous\n"
		      ".section __ex_table,\"a\"\n"
		      "   "__FIXUP_ALIGN"\n"
		      "   "__FIXUP_WORD" 1b,4b\n"
		      ".previous\n"
		      : "=r"(rc), "=r"(edx)
		      : "r"(field), "i"(-VMX_FAIL_INVALID),
			"i"(-VMX_FAIL_VALID), "i"(-VMX_FAIL_UD_GF),  "0"(0)
		      : "memory", "cc");

	if (!rc)
		*value = edx;

	return rc;
}

static inline int __vmwrite(unsigned long field, unsigned long value)
{
	int rc = VMM_OK;
	asm volatile ("1: vmwrite %1, %2\n"
		      "jz 5f\n"
		      "jc 2f\n"
		      "jmp 3f\n"
		      "5:sub $%c4, %0\n"
		      "jmp  3f\n"
		      "2:sub  $%c3, %0\n"
		      "3:\n"
		      ".section .fixup,\"ax\"\n"
		      "4:sub $%c5, %0 ; jmp 3b\n"
		      ".previous\n"
		      ".section __ex_table,\"a\"\n"
		      "   "__FIXUP_ALIGN"\n"
		      "   "__FIXUP_WORD" 1b,4b\n"
		      ".previous\n"
		      : "=qm"(rc)
		      : "r"(value), "r"(field),  "i"(-VMX_FAIL_INVALID),
			"i"(-VMX_FAIL_VALID), "i"(-VMX_FAIL_UD_GF)
		      : "memory");

	return rc;
}

static inline unsigned long __vmread_safe(unsigned long field, int *error)
{
	unsigned long ecx;
	int rc;

	if ((rc = __vmread(field, &ecx)) != VMM_OK) {
		*error = rc;
		return 0;
	}

	*error = 0;

	return ecx;
}

static inline int __vm_set_bit(unsigned long field, unsigned int bit)
{
	unsigned long value;
	int rc = VMM_OK;

	if ((rc = __vmread(field, &value)) != VMM_OK)
		return rc;
	if ((rc = __vmwrite(field, value | (1UL << bit))) != VMM_OK)
		return rc;

	return VMM_OK;
}

static inline int __vm_clear_bit(unsigned long field, unsigned int bit)
{
	unsigned long value;
	int rc = VMM_OK;

	if ((rc = __vmread(field, &value)) != VMM_OK)
		return rc;
	if ((rc = __vmwrite(field, value & ~(1UL << bit))) != VMM_OK)
		return rc;

	return VMM_OK;
}

static inline void __vmxoff(void)
{
	asm volatile("vmxoff\n"
                     : : : "memory" );
}

static inline int __vmxon(u64 addr)
{
	int rc = VMM_OK;

	asm volatile ("1: vmxon (%1)\n"
		      "jz 5f\n"
		      "jc 2f\n"
		      "jmp 3f\n"
		      "5:sub $%c4, %0\n"
		      "jmp  3f\n"
		      "2:sub  $%c3, %0\n"
		      "3:\n"
		      ".section .fixup,\"ax\"\n"
		      "4:sub $%c5, %0 ; jmp 3b\n"
		      ".previous\n"
		      ".section __ex_table,\"a\"\n"
		      "   "__FIXUP_ALIGN"\n"
		      "   "__FIXUP_WORD" 1b,4b\n"
		      ".previous\n"
		      : "=a"(rc)
		      : "c"(&addr), "0"(0),  "i"(-VMX_FAIL_INVALID),
			"i"(-VMX_FAIL_VALID), "i"(-VMX_FAIL_UD_GF)
		      : "memory");

	return rc;
}

/* EPT violation qualifications definitions */
#define _EPT_READ_VIOLATION         0
#define EPT_READ_VIOLATION          (1UL<<_EPT_READ_VIOLATION)
#define _EPT_WRITE_VIOLATION        1
#define EPT_WRITE_VIOLATION         (1UL<<_EPT_WRITE_VIOLATION)
#define _EPT_EXEC_VIOLATION         2
#define EPT_EXEC_VIOLATION          (1UL<<_EPT_EXEC_VIOLATION)
#define _EPT_EFFECTIVE_READ         3
#define EPT_EFFECTIVE_READ          (1UL<<_EPT_EFFECTIVE_READ)
#define _EPT_EFFECTIVE_WRITE        4
#define EPT_EFFECTIVE_WRITE         (1UL<<_EPT_EFFECTIVE_WRITE)
#define _EPT_EFFECTIVE_EXEC         5
#define EPT_EFFECTIVE_EXEC          (1UL<<_EPT_EFFECTIVE_EXEC)
#define _EPT_GLA_VALID              7
#define EPT_GLA_VALID               (1UL<<_EPT_GLA_VALID)
#define _EPT_GLA_FAULT              8
#define EPT_GLA_FAULT               (1UL<<_EPT_GLA_FAULT)

#define EPT_PAGETABLE_ENTRIES       512

extern int __init intel_init(struct cpuinfo_x86 *cpuinfo);
extern int intel_setup_vm_control(struct vcpu_hw_context *context);

#endif /* __VMX_H__ */
