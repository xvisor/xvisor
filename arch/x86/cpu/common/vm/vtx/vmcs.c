/**
 * Copyright (c) 2004, Intel Corporation.
 * Copyright (c) 2013 Himanshu Chauhan.
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
 * @file intel_vmcs.c
 * @author Himanshu Chauhan
 * @brief Intel VMCS setup functions.
 * Largely derived from Intel code available online.
 */

#include <vmm_types.h>
#include <vmm_compiler.h>
#include <vmm_host_aspace.h>
#include <processor_flags.h>
#include <libs/bitops.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <cpu_features.h>
#include <control_reg_access.h>
#include <vm/vmcs.h>
#include <vm/vmx.h>
#include <vm/ept.h>

#define BYTES_PER_LONG (BITS_PER_LONG/8)

static int __unused __read_mostly opt_vpid_enabled = 1;
static int __unused __read_mostly opt_unrestricted_guest_enabled = 1;

/*
 * These two parameters are used to config the controls for Pause-Loop Exiting:
 * ple_gap:    upper bound on the amount of time between two successive
 *             executions of PAUSE in a loop.
 * ple_window: upper bound on the amount of time a guest is allowed to execute
 *	       in a PAUSE loop.
 * Time is measured based on a counter that runs at the same rate as the TSC,
 * refer SDM volume 3b section 21.6.13 & 22.1.3.
 */
static unsigned int __unused __read_mostly ple_gap = 41;
static unsigned int __unused __read_mostly ple_window = 4096;

static u32 vmx_basic_msr_low __read_mostly;
static u32 vmx_basic_msr_high __read_mostly;
static u32 vmx_misc_msr_low __read_mostly;
static u32 vmx_misc_msr_high __read_mostly;

/* Dynamic (run-time adjusted) execution control flags. */
u32 vmx_pin_based_exec_control __read_mostly;
u32 vmx_pin_based_exec_default1 __read_mostly;
u32 vmx_pin_based_exec_default0 __read_mostly;

u32 vmx_cpu_based_exec_control __read_mostly;
u32 vmx_cpu_based_exec_default1 __read_mostly;
u32 vmx_cpu_based_exec_default0 __read_mostly;

u32 vmx_secondary_exec_control __read_mostly;
u32 vmx_secondary_exec_default1 __read_mostly;
u32 vmx_secondary_exec_default0 __read_mostly;

u32 vmx_vmexit_control __read_mostly;
u32 vmx_vmexit_default1 __read_mostly;
u32 vmx_vmexit_default0 __read_mostly;

u32 vmx_vmentry_control __read_mostly;
u32 vmx_vmentry_default1 __read_mostly;
u32 vmx_vmentry_default0 __read_mostly;
u32 cpu_has_vmx_ept_2mb __read_mostly;

u64 vmx_ept_vpid_cap __read_mostly;
u32 vmx_on_size __read_mostly;
u8 cpu_has_vmx_ins_outs_instr_info __read_mostly;
u32 vmxon_region_size __read_mostly;
u32 vmxon_region_nr_pages __read_mostly;

u32 vmcs_revision_id __read_mostly;

static void __init vmx_display_features(void)
{
	int printed = 0;

	vmm_printf("VMX: Supported advanced features:\n");

#define P(p,s) if ( p ) { vmm_printf(" - %s\n", s); printed = 1; }
	P(cpu_has_vmx_virtualize_apic_accesses, "APIC MMIO access virtualisation");
	P(cpu_has_vmx_tpr_shadow, "APIC TPR shadow");
	P(cpu_has_vmx_ept, "Extended Page Tables (EPT)");
	P(cpu_has_vmx_vpid, "Virtual-Processor Identifiers (VPID)");
	P(cpu_has_vmx_vnmi, "Virtual NMI");
	P(cpu_has_vmx_msr_bitmap, "MSR direct-access bitmap");
	P(cpu_has_vmx_unrestricted_guest, "Unrestricted Guest");
#undef P

	if (!printed)
		vmm_printf(" - none\n");

	if (cpu_has_vmx_ept_2mb)
		vmm_printf("EPT supports 2MB super page.\n");
}

/* VMX copabilities detection */
/* Intel IA-32 manual 3B 27.5.1 p. 222 */
void vmx_detect_capability(void)
{
	cpu_read_msr32(MSR_IA32_VMX_BASIC, &vmx_basic_msr_high,
                       &vmx_basic_msr_low);

	cpu_read_msr32(MSR_IA32_VMX_MISC, &vmx_misc_msr_high,
		       &vmx_misc_msr_low);

	if (!(vmx_misc_msr_low & (0x1UL << 5)))
		vmm_panic("CPU doesn't supports the mandated unrestricted guest mode!\n");

	/* save the revision_id */
	vmcs_revision_id = vmx_basic_msr_low;

	VM_LOG(LVL_VERBOSE, "%s: Basic MSR: 0x%lx\n", __func__, cpu_read_msr(MSR_IA32_VMX_BASIC));
	VM_LOG(LVL_VERBOSE, "%s: Basic low: 0x%x\n", __func__, vmx_basic_msr_low);

	vmxon_region_size = VMM_ROUNDUP2_PAGE_SIZE(vmx_basic_msr_high
						   & 0x1ffful);
	VM_LOG(LVL_VERBOSE, "%s: VMXON Region Size: 0x%x\n", __func__, vmxon_region_size);

	vmxon_region_nr_pages = VMM_SIZE_TO_PAGE(vmxon_region_size);

	/* Determine the default1 and default0 for control msrs
	 *
	 * Intel IA-32 manual 3B Appendix G.3
	 *
	 * bit == 0 in msr high word ==> must be zero (default0, allowed1)
	 * bit == 1 in msr low word ==> must be one (default1, allowed0)
	 *
	 */
	if (!(vmx_basic_msr_high & (1u << 23))) {
		/* PIN BASED CONTROL */
		cpu_read_msr32(MSR_IA32_VMX_PINBASED_CTLS,
			     &vmx_pin_based_exec_default1,
			     &vmx_pin_based_exec_default0);

		/* PROCESSOR(CPU) BASED CONTROL */
		cpu_read_msr32(MSR_IA32_VMX_PROCBASED_CTLS,
			     &vmx_cpu_based_exec_default1,
			     &vmx_cpu_based_exec_default0);

		/* VMEXIT CONTROL */
		cpu_read_msr32(MSR_IA32_VMX_EXIT_CTLS,
			     &vmx_vmexit_default1,
			     &vmx_vmexit_default0);

		/* VMENTRY CONTROL */
		cpu_read_msr32(MSR_IA32_VMX_ENTRY_CTLS,
			     &vmx_vmentry_default1,
			     &vmx_vmentry_default0);
	} else { /* if the 55 bit is 1 */
		/* PIN BASED CONTROL */
		cpu_read_msr32(MSR_IA32_VMX_TRUE_PINBASED_CTLS,
			     &vmx_pin_based_exec_default1,
			     &vmx_pin_based_exec_default0);

		/* PROCESSOR(CPU) BASED CONTROL */
		cpu_read_msr32(MSR_IA32_VMX_TRUE_PROCBASED_CTLS,
			     &vmx_cpu_based_exec_default1,
			     &vmx_cpu_based_exec_default0);

		/* VMEXIT CONTROL */
		cpu_read_msr32(MSR_IA32_VMX_TRUE_EXIT_CTLS,
			     &vmx_vmexit_default1,
			     &vmx_vmexit_default0);

		/* VMENTRY CONTROL */
		cpu_read_msr32(MSR_IA32_VMX_TRUE_ENTRY_CTLS,
			     &vmx_vmentry_default1,
			     &vmx_vmentry_default0);
	}

	/* detect EPT and VPID capability */
	if (vmx_cpu_based_exec_default1
            & CPU_BASED_ACTIVATE_SECONDARY_CONTROLS) {
		cpu_read_msr32(MSR_IA32_VMX_PROCBASED_CTLS2,
			     &vmx_secondary_exec_default1,
			     &vmx_secondary_exec_default0);

		/* The IA32_VMX_EPT_VPID_CAP MSR exists only when EPT or VPID available */
		if (vmx_secondary_exec_default1
		    & (SECONDARY_EXEC_ENABLE_EPT
		       | SECONDARY_EXEC_ENABLE_VPID)) {
			vmx_ept_vpid_cap = cpu_read_msr(MSR_IA32_VMX_EPT_VPID_CAP);
		}
	}

	if (!vmx_pin_based_exec_control) {
		/* First time through. */
		vmcs_revision_id = vmx_basic_msr_low;
		vmx_pin_based_exec_control = vmx_pin_based_exec_default1;
		vmx_cpu_based_exec_control = vmx_cpu_based_exec_default1;
		vmx_secondary_exec_control = vmx_secondary_exec_default1;
		vmx_vmexit_control	   = vmx_vmexit_default1;
		vmx_vmentry_control	   = vmx_vmentry_default1;
		cpu_has_vmx_ins_outs_instr_info = !!(vmx_basic_msr_high & (1U<<22));
		vmx_display_features();
	}
}

struct vmcs *alloc_vmcs(void)
{
	struct vmcs *vmcs;

	vmcs  = (struct vmcs *)vmm_host_alloc_pages(1, /* number of pages */
						    VMM_MEMORY_FLAGS_IO);

	memset (( char *)vmcs, 0, (1 * PAGE_SIZE));

	return vmcs;
}

void *alloc_vmx_on_region(void)
{
	void *vmcs;

	vmcs  = (void *)vmm_host_alloc_pages(vmxon_region_nr_pages, /* number of pages */
					     VMM_MEMORY_FLAGS_IO);

	if (!vmcs)
		return NULL;

	memset (( char *)vmcs, 0, (vmxon_region_nr_pages * PAGE_SIZE));

	return vmcs;
}

struct vmcs *current_vmcs(physical_addr_t *phys)
{
        physical_addr_t vmcs_phys = 0;
        virtual_addr_t vmcs_virt = 0;

        vmcs_phys = __vmptrst();

        /* There is not current VMCS */
        if (!vmcs_phys || vmcs_phys == 0xFFFFFFFFFFFFFFFFULL) {
                VM_LOG(LVL_ERR, "%s: There is not active(current) VMCS on this "
		       "logical processor.\n", __func__);
                return NULL;
        }

        if (vmm_host_pa2va(vmcs_phys, &vmcs_virt) != VMM_OK) {
                VM_LOG(LVL_ERR, "%s: Could not find virtual address for current VMCS\n", __func__);
                return NULL;
        }

	if (phys)
		*phys = vmcs_phys;

        return (struct vmcs *)(vmcs_virt);
}

struct vmcs* create_vmcs(void)
{
	/* verify settings */

	/* IA-32 SDM Vol 3B: VMCS size is never greater than 4kB. */
	if ((vmx_basic_msr_high & 0x1fff) > PAGE_SIZE) {
		VM_LOG(LVL_ERR, "VMCS size larger than 4K\n");
		return NULL;
	}

	/* IA-32 SDM Vol 3B: 64-bit CPUs always have VMX_BASIC_MSR[48]==0. */
	if (vmx_basic_msr_high & (1u<<16)) {
		VM_LOG(LVL_ERR, "VMX_BASIC_MSR[48] = 1\n");
		return NULL;
	}

	/* Require Write-Back (WB) memory type for VMCS accesses. */
	if (((vmx_basic_msr_high >> 18) & 15) != 6) {
		VM_LOG(LVL_ERR, "Write-back memory required for VMCS\n");
		return NULL;
	}

	/* Alloc a page for vmcs */
	struct vmcs* vmcs = alloc_vmcs();

	vmcs->revision_id = vmcs_revision_id;

	return vmcs;
}

static void vmcs_init_host_env(void)
{
	unsigned long cr0, cr4;
	struct {
		u16 limit;
		u64 base;
	} __attribute__ ((packed)) xdt;

	/* Host data selectors. */
	__vmwrite(HOST_SS_SELECTOR, VMM_DATA_SEG_SEL);
	__vmwrite(HOST_DS_SELECTOR, VMM_DATA_SEG_SEL);
	__vmwrite(HOST_ES_SELECTOR, VMM_DATA_SEG_SEL);
	__vmwrite(HOST_FS_SELECTOR, VMM_DATA_SEG_SEL);
	__vmwrite(HOST_GS_SELECTOR, VMM_DATA_SEG_SEL);
	__vmwrite(HOST_FS_BASE, 0);
	__vmwrite(HOST_GS_BASE, 0);

	/* Host control registers. */
	cr0 = read_cr0() | X86_CR0_TS;
	__vmwrite(HOST_CR0, cr0);

	cr4 = read_cr4() | X86_CR4_OSXSAVE;
	__vmwrite(HOST_CR4, cr4);

	/* Host CS:RIP. */
	__vmwrite(HOST_CS_SELECTOR, VMM_CODE_SEG_SEL);
	//__vmwrite(HOST_RIP, (unsigned long)vmx_asm_vmexit_handler);

	/* Host SYSENTER CS:RIP. */
	__vmwrite(HOST_SYSENTER_CS, 0);
	__vmwrite(HOST_SYSENTER_EIP, 0);
	__vmwrite(HOST_SYSENTER_ESP, 0);

	/* GDT */
	__asm__ __volatile__ ("sgdt (%0) \n" :: "a"(&xdt) : "memory");
	__vmwrite(HOST_GDTR_BASE, xdt.base);

	/* IDT */
	__asm__ __volatile__ ("sidt (%0) \n" :: "a"(&xdt) : "memory");
	__vmwrite(HOST_IDTR_BASE, xdt.base);

	/* TR */
	__vmwrite(HOST_TR_SELECTOR, VMM_DATA_SEG_SEL);
	__vmwrite(HOST_TR_BASE, 0);

}

void set_pin_based_exec_controls(void)
{
	u32 pin_controls[] = {PIN_BASED_EXT_INTR_MASK,
			      PIN_BASED_NMI_EXITING,
			      PIN_BASED_VIRTUAL_NMIS,
			      PIN_BASED_PREEMPTION_TIMER,
			      PIN_BASED_PROCESS_INTRS};
	u32 pin_controls_processed = (PIN_BASED_EXT_INTR_MASK |
				      PIN_BASED_NMI_EXITING |
				      PIN_BASED_VIRTUAL_NMIS |
				      PIN_BASED_PREEMPTION_TIMER |
				      PIN_BASED_PROCESS_INTRS);
	u32 vmx_pin_based_control = 0;
	u32 i = 0;
	u32 nr_pin_controls = sizeof(pin_controls)/sizeof(u32);

	/* Set the single settings */
	vmx_pin_based_control |= (vmx_pin_based_exec_default1 &
				  vmx_pin_based_exec_default0);
	pin_controls_processed &= ~(vmx_pin_based_exec_default1 &
				    vmx_pin_based_exec_default0);

	for (i = 0; i < nr_pin_controls; i++) {
		if (!(pin_controls[i] & pin_controls_processed))
			/* one of the single settings. Already set
			 * in the previous step so skip */
			continue;

		/* controls that allow 0 or 1 settings */
		switch(pin_controls[i]) {
		case PIN_BASED_EXT_INTR_MASK:
			/* we want to support this, set 1 for external
			 * interrupts to cause VM exits */
			vmx_pin_based_control |= PIN_BASED_EXT_INTR_MASK;
			break;

		default:
			/* we don't want to enable them by default so
			 * consider the default settings. */
			if (vmx_pin_based_exec_default1 & pin_controls[i])
				vmx_pin_based_control |= pin_controls[i];
			break;
		}
	}

	/* Initialize pin based control */
	__vmwrite(PIN_BASED_VM_EXEC_CONTROL, vmx_pin_based_control);
}

void set_proc_based_exec_controls(void)
{
	u32 proc_controls[] = {CPU_BASED_VIRTUAL_INTR_PENDING,
			       CPU_BASED_USE_TSC_OFFSETING,
			       CPU_BASED_HLT_EXITING,
			       CPU_BASED_INVLPG_EXITING,
			       CPU_BASED_MWAIT_EXITING,
			       CPU_BASED_RDPMC_EXITING,
			       CPU_BASED_RDTSC_EXITING,
			       CPU_BASED_CR3_LOAD_EXITING,
			       CPU_BASED_CR3_STORE_EXITING,
			       CPU_BASED_CR8_LOAD_EXITING,
			       CPU_BASED_CR8_STORE_EXITING,
			       CPU_BASED_TPR_SHADOW,
			       CPU_BASED_VIRTUAL_NMI_PENDING,
			       CPU_BASED_MOV_DR_EXITING,
			       CPU_BASED_UNCOND_IO_EXITING,
			       CPU_BASED_ACTIVATE_IO_BITMAP,
			       CPU_BASED_MONITOR_TRAP_FLAG,
			       CPU_BASED_ACTIVATE_MSR_BITMAP,
			       CPU_BASED_MONITOR_EXITING,
			       CPU_BASED_PAUSE_EXITING,
			       CPU_BASED_ACTIVATE_SECONDARY_CONTROLS};
	u32 proc_controls_processed = (CPU_BASED_VIRTUAL_INTR_PENDING |
				       CPU_BASED_USE_TSC_OFFSETING |
				       CPU_BASED_HLT_EXITING |
				       CPU_BASED_INVLPG_EXITING |
				       CPU_BASED_MWAIT_EXITING |
				       CPU_BASED_RDPMC_EXITING |
				       CPU_BASED_RDTSC_EXITING |
				       CPU_BASED_CR3_LOAD_EXITING |
				       CPU_BASED_CR3_STORE_EXITING |
				       CPU_BASED_CR8_LOAD_EXITING |
				       CPU_BASED_CR8_STORE_EXITING |
				       CPU_BASED_TPR_SHADOW |
				       CPU_BASED_VIRTUAL_NMI_PENDING |
				       CPU_BASED_MOV_DR_EXITING |
				       CPU_BASED_UNCOND_IO_EXITING |
				       CPU_BASED_ACTIVATE_IO_BITMAP |
				       CPU_BASED_MONITOR_TRAP_FLAG |
				       CPU_BASED_ACTIVATE_MSR_BITMAP |
				       CPU_BASED_MONITOR_EXITING |
				       CPU_BASED_PAUSE_EXITING |
				       CPU_BASED_ACTIVATE_SECONDARY_CONTROLS);
	u32 vmx_proc_based_control = 0;
	u32 vmx_proc_secondary_control = 0;
	u32 i = 0;
	u32 nr_proc_controls = sizeof(proc_controls)/sizeof(u32);

	/* Set the single settings */
	vmx_proc_based_control |= (vmx_cpu_based_exec_default1 &
				   vmx_cpu_based_exec_default0);
	proc_controls_processed &= ~(vmx_cpu_based_exec_default1 &
				     vmx_cpu_based_exec_default0);

	for (i = 0; i < nr_proc_controls; i++) {
		if (!(proc_controls[i] & proc_controls_processed))
			/* one of the single settings. Already set
			 * in the previous step so skip */
			continue;

		/* controls that allow 0 or 1 settings */
		switch(proc_controls[i]) {
		case CPU_BASED_HLT_EXITING:
		case CPU_BASED_INVLPG_EXITING:
		case CPU_BASED_CR3_LOAD_EXITING:
		case CPU_BASED_CR3_STORE_EXITING:
		case CPU_BASED_ACTIVATE_IO_BITMAP:
		case CPU_BASED_ACTIVATE_MSR_BITMAP:
		case CPU_BASED_ACTIVATE_SECONDARY_CONTROLS:
		case CPU_BASED_MONITOR_EXITING:
		case CPU_BASED_PAUSE_EXITING:
			/* we want to support this, set 1 for external
			 * interrupts to cause VM exits */
			vmx_proc_based_control |= proc_controls[i];

			if (proc_controls[i] == CPU_BASED_ACTIVATE_SECONDARY_CONTROLS) {
				vmx_proc_secondary_control = (SECONDARY_EXEC_ENABLE_EPT |
							      SECONDARY_EXEC_ENABLE_VPID |
							      SECONDARY_EXEC_UNRESTRICTED_GUEST);
				__vmwrite(SECONDARY_VM_EXEC_CONTROL, vmx_proc_secondary_control);
			}
			break;

		default:
			/* Others we don't want to enable them by default so
			 * consider the default settings. */
			if (vmx_cpu_based_exec_default1 & proc_controls[i])
				vmx_proc_based_control |= proc_controls[i];
			break;
		}
	}

	/* Initialize proc based control */
	__vmwrite(CPU_BASED_VM_EXEC_CONTROL, vmx_proc_based_control);
}

void set_vmx_entry_exec_controls(void)
{
	u32 entry_controls[] = {VM_ENTRY_LOAD_DEBUG_CONTROLS,
				VM_ENTRY_IA32E_MODE,
				VM_ENTRY_SMM,
				VM_ENTRY_DEACT_DUAL_MONITOR,
				VM_ENTRY_LOAD_PERF_GLOBAL_CTRL,
				VM_ENTRY_LOAD_GUEST_PAT,
				VM_ENTRY_LOAD_GUEST_EFER,
				VM_ENTRY_LOAD_GUEST_BNDCFGS,
				VM_ENTRY_CONCEAL_VMX_PT};
	u32 entry_controls_processed = (VM_ENTRY_LOAD_DEBUG_CONTROLS |
					VM_ENTRY_IA32E_MODE |
					VM_ENTRY_SMM |
					VM_ENTRY_DEACT_DUAL_MONITOR |
					VM_ENTRY_LOAD_PERF_GLOBAL_CTRL |
					VM_ENTRY_LOAD_GUEST_PAT |
					VM_ENTRY_LOAD_GUEST_EFER |
					VM_ENTRY_LOAD_GUEST_BNDCFGS |
					VM_ENTRY_CONCEAL_VMX_PT);
	u32 vmx_entry_control = 0;
	u32 i = 0;
	u32 nr_entry_controls = sizeof(entry_controls)/sizeof(u32);

	/* Set the single settings */
	vmx_entry_control |= (vmx_vmentry_default1 &
			      vmx_vmentry_default0);
	entry_controls_processed &= ~(vmx_vmentry_default1 &
				      vmx_vmentry_default0);

	for (i = 0; i < nr_entry_controls; i++) {
		if (!(entry_controls[i] & entry_controls_processed))
			/* one of the single settings. Already set
			 * in the previous step so skip */
			continue;

		/* controls that allow 0 or 1 settings */
		switch(entry_controls[i]) {
		default:
			/* we don't want to enable them by default so
			 * consider the default settings. */
			if (vmx_vmentry_default1 & entry_controls[i])
				vmx_entry_control |= entry_controls[i];
			break;
		}
	}

	/* Initialize VMEntry control */
	__vmwrite(VM_ENTRY_CONTROLS, vmx_entry_control);
}

void set_vmx_exit_exec_controls(void)
{
	u32 exit_controls[] = {VM_EXIT_SAVE_DBG_CTRL,
			       VM_EXIT_IA32E_MODE,
			       VM_EXIT_LOAD_PERF_GLOBAL_CTRL,
			       VM_EXIT_ACK_INTR_ON_EXIT,
			       VM_EXIT_SAVE_GUEST_PAT,
			       VM_EXIT_LOAD_HOST_PAT,
			       VM_EXIT_SAVE_GUEST_EFER,
			       VM_EXIT_LOAD_HOST_EFER,
			       VM_EXIT_SAVE_PREEMPT_TIMER,
			       VM_EXIT_CLEAR_BNDCFGS,
			       VM_EXIT_CONCEAL_VMX_PT};
	u32 exit_controls_processed = (VM_EXIT_SAVE_DBG_CTRL |
				       VM_EXIT_IA32E_MODE |
				       VM_EXIT_LOAD_PERF_GLOBAL_CTRL |
				       VM_EXIT_ACK_INTR_ON_EXIT |
				       VM_EXIT_SAVE_GUEST_PAT |
				       VM_EXIT_LOAD_HOST_PAT |
				       VM_EXIT_SAVE_GUEST_EFER |
				       VM_EXIT_LOAD_HOST_EFER |
				       VM_EXIT_SAVE_PREEMPT_TIMER |
				       VM_EXIT_CLEAR_BNDCFGS |
				       VM_EXIT_CONCEAL_VMX_PT);
	u32 vmx_exit_control = 0;
	u32 i = 0;
	u32 nr_exit_controls = sizeof(exit_controls)/sizeof(u32);

	/* Set the single settings */
	vmx_exit_control |= (vmx_vmexit_default1 &
			     vmx_vmexit_default0);
	exit_controls_processed &= ~(vmx_vmexit_default1 &
				     vmx_vmexit_default0);

	for (i = 0; i < nr_exit_controls; i++) {
		if (!(exit_controls[i] & exit_controls_processed))
			/* one of the single settings. Already set
			 * in the previous step so skip */
			continue;

		/* controls that allow 0 or 1 settings */
		switch(exit_controls[i]) {
		case VM_EXIT_IA32E_MODE:
		case VM_EXIT_SAVE_GUEST_PAT:
		case VM_EXIT_LOAD_HOST_PAT:
		case VM_EXIT_SAVE_GUEST_EFER:
		case VM_EXIT_LOAD_HOST_EFER:
		case VM_EXIT_ACK_INTR_ON_EXIT:
			vmx_exit_control |= exit_controls[i];
			break;
		default:
			/* we don't want to enable them by default so
			 * consider the default settings. */
			if (vmx_vmexit_default1 & exit_controls[i])
				vmx_exit_control |= exit_controls[i];
			break;
		}
	}

	/* Initialize VMEntry control */
	__vmwrite(VM_EXIT_CONTROLS, vmx_exit_control);

}

int vmx_set_control_params(struct vcpu_hw_context *context)
{
	int rc = VMM_OK;
	u32 vcpu_id = context->assoc_vcpu->subid = 1;

	set_pin_based_exec_controls();
	set_proc_based_exec_controls();
	set_vmx_entry_exec_controls();
	set_vmx_exit_exec_controls();

        /* A and B - 4K each */
	context->icept_table.io_table_phys =
		cpu_create_vcpu_intercept_table(VMM_SIZE_TO_PAGE(8 << 10),
						&context->icept_table.io_table_virt);

	__vmwrite(IO_BITMAP_A, context->icept_table.io_table_phys);
	__vmwrite(IO_BITMAP_B, context->icept_table.io_table_phys + VMM_PAGE_SIZE);

	context->icept_table.msr_table_phys =
		cpu_create_vcpu_intercept_table(VMM_SIZE_TO_PAGE(4 << 10),
						&context->icept_table.msr_table_virt);

	__vmwrite(MSR_BITMAP, context->icept_table.msr_table_phys);

	/* Set up the VCPU's guest extended page tables */
	if ((rc = setup_ept(context)) != VMM_OK) {
		VM_LOG(LVL_ERR, "EPT Setup failed with error: %d\n", rc);
		return rc;
	}

	__vmwrite(EPT_POINTER, context->eptp);
	__vmwrite(VIRTUAL_PROCESSOR_ID, vcpu_id);

	/* Initialize host save area */
	vmcs_init_host_env();

	return VMM_OK;
}

struct xgt_desc {
	unsigned short size;
	unsigned long address __attribute__((packed));
};

void vmx_save_host_state(struct vcpu_hw_context *context)
{
	unsigned long rsp;

	/*
	 * Skip end of cpu_user_regs when entering the hypervisor because the
	 * CPU does not save context onto the stack. SS,RSP,CS,RIP,RFLAGS,etc
	 * all get saved into the VMCS instead.
	 */
	__asm__ __volatile__ ("movq %%rsp, %0 \n\t" : "=r"(rsp));
	__vmwrite(HOST_RSP, rsp);
}

void vmx_disable_intercept_for_msr(struct vcpu_hw_context *context, u32 msr)
{
	unsigned long *msr_bitmap = (unsigned long *)context->icept_table.msr_table_virt;

	/* VMX MSR bitmap supported? */
	if (msr_bitmap == NULL)
		return;

	/*
	 * See Intel PRM Vol. 3, 20.6.9 (MSR-Bitmap Address). Early manuals
	 * have the write-low and read-high bitmap offsets the wrong way round.
	 * We can control MSRs 0x00000000-0x00001fff and 0xc0000000-0xc0001fff.
	 */
	if (msr <= 0x1fff) {
		clear_bit(msr, msr_bitmap + 0x000/BYTES_PER_LONG); /* read-low */
		clear_bit(msr, msr_bitmap + 0x800/BYTES_PER_LONG); /* write-low */
	} else if ((msr >= 0xc0000000) && (msr <= 0xc0001fff)) {
		msr &= 0x1fff;
		clear_bit(msr, msr_bitmap + 0x400/BYTES_PER_LONG); /* read-high */
		clear_bit(msr, msr_bitmap + 0xc00/BYTES_PER_LONG); /* write-high */
	}
}

void vmx_set_vm_to_powerup_state(struct vcpu_hw_context *context)
{
	/* MSR intercepts. */
	__vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);
	__vmwrite(VM_EXIT_MSR_STORE_COUNT, 0);
	__vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);

	__vmwrite(VM_ENTRY_INTR_INFO, 0);

	__vmwrite(CR0_GUEST_HOST_MASK, ~0UL);
	__vmwrite(CR4_GUEST_HOST_MASK, ~0UL);

	__vmwrite(PAGE_FAULT_ERROR_CODE_MASK, 0);
	__vmwrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);

	__vmwrite(CR3_TARGET_COUNT, 0);

	__vmwrite(GUEST_ACTIVITY_STATE, 0);

	/*
	 * Make the CS.RIP point to 0xFFFF0. The reset vector. The Bios seems
	 * to be linked in a fashion that the reset vectors lies at0x3fff0.
	 * The guest physical address will be 0xFFFF0 when the first page fault
	 * happens in paged real mode. Hence, the the bios is loaded at 0xc0c0000
	 * so that 0xc0c0000 + 0x3fff0 becomes 0xc0ffff0 => The host physical
	 * for reset vector. Everything else then just falls in place.
	 */

	/* Guest segment bases. */
	__vmwrite(GUEST_ES_BASE, 0);
	__vmwrite(GUEST_ES_LIMIT, 0xFFFF);
	__vmwrite(GUEST_ES_AR_BYTES, 0x93);
	__vmwrite(GUEST_ES_SELECTOR, 0);

	__vmwrite(GUEST_SS_BASE, 0);
	__vmwrite(GUEST_SS_LIMIT, 0xFFFF);
	__vmwrite(GUEST_SS_AR_BYTES, 0x193);
	__vmwrite(GUEST_SS_SELECTOR, 0);

	__vmwrite(GUEST_DS_BASE, 0);
	__vmwrite(GUEST_DS_LIMIT, 0xFFFF);
	__vmwrite(GUEST_DS_AR_BYTES, 0x93);
	__vmwrite(GUEST_DS_SELECTOR, 0);

	__vmwrite(GUEST_FS_BASE, 0);
	__vmwrite(GUEST_FS_LIMIT, 0xFFFF);
	__vmwrite(GUEST_FS_AR_BYTES, 0x93);
	__vmwrite(GUEST_FS_SELECTOR, 0);

	__vmwrite(GUEST_GS_BASE, 0);
	__vmwrite(GUEST_GS_LIMIT, 0xFFFF);
	__vmwrite(GUEST_GS_AR_BYTES, 0x93);
	__vmwrite(GUEST_GS_SELECTOR, 0);

	__vmwrite(GUEST_CS_BASE, 0xF0000);
	__vmwrite(GUEST_CS_LIMIT, 0xFFFF);
	__vmwrite(GUEST_CS_AR_BYTES, 0x19b);
	__vmwrite(GUEST_CS_SELECTOR, 0xF000);

	/* Guest IDT. */
	__vmwrite(GUEST_IDTR_BASE, 0);
	__vmwrite(GUEST_IDTR_LIMIT, 0);

	/* Guest GDT. */
	__vmwrite(GUEST_GDTR_BASE, 0);
	__vmwrite(GUEST_GDTR_LIMIT, 0xFFFF);

	/* Guest LDT. */
	__vmwrite(GUEST_LDTR_AR_BYTES, 0x0082); /* LDT */
	__vmwrite(GUEST_LDTR_SELECTOR, 0);
	__vmwrite(GUEST_LDTR_BASE, 0);
	__vmwrite(GUEST_LDTR_LIMIT, 0xFFFF);

	/* Guest TSS. */
	__vmwrite(GUEST_TR_AR_BYTES, 0x008b); /* 32-bit TSS (busy) */
	__vmwrite(GUEST_TR_BASE, 0);
	__vmwrite(GUEST_TR_LIMIT, 0xFFFF);

	__vmwrite(GUEST_INTERRUPTIBILITY_INFO, 0);
	__vmwrite(GUEST_DR7, 0);
	__vmwrite(VMCS_LINK_POINTER, ~0UL);

	__vmwrite(EXCEPTION_BITMAP, 0);

	/* Control registers */
	__vmwrite(GUEST_CR0, (X86_CR0_ET | X86_CR0_CD | X86_CR0_NW | X86_CR0_PG));
	__vmwrite(GUEST_CR3, 0);
	__vmwrite(GUEST_CR4, 0);

	/* G_PAT */
	u64 host_pat, guest_pat;

	host_pat = cpu_read_msr(MSR_IA32_CR_PAT);
	guest_pat = MSR_IA32_CR_PAT_RESET;

	__vmwrite(HOST_PAT, host_pat);
	__vmwrite(GUEST_PAT, guest_pat);

	/* Initial state */
	__vmwrite(GUEST_RSP, 0x0);
	__vmwrite(GUEST_RFLAGS, 0x2);
	__vmwrite(GUEST_RIP, 0xFFF0);

	context->g_cr0 = (X86_CR0_ET | X86_CR0_CD | X86_CR0_NW);
	context->g_cr1 = context->g_cr2 = context->g_cr3 = 0;

	vmcs_dump(context);
}

void vmx_set_vm_to_mbr_start_state(struct vcpu_hw_context *context)
{
	/* MSR intercepts. */
	__vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);
	__vmwrite(VM_EXIT_MSR_STORE_COUNT, 0);
	__vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);

	__vmwrite(VM_ENTRY_INTR_INFO, 0);

	__vmwrite(CR0_GUEST_HOST_MASK, ~0UL);
	__vmwrite(CR4_GUEST_HOST_MASK, ~0UL);

	__vmwrite(PAGE_FAULT_ERROR_CODE_MASK, 0);
	__vmwrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);

	__vmwrite(CR3_TARGET_COUNT, 0);

	__vmwrite(GUEST_ACTIVITY_STATE, 0);

	/* Guest segment bases. */
	__vmwrite(GUEST_ES_BASE, 0);
	__vmwrite(GUEST_SS_BASE, 0);
	__vmwrite(GUEST_DS_BASE, 00000400);
	__vmwrite(GUEST_FS_BASE, 0xE7170);
	__vmwrite(GUEST_GS_BASE, 0xF0000);
	__vmwrite(GUEST_CS_BASE, 0);

	/* Guest segment limits. */
	__vmwrite(GUEST_ES_LIMIT, ~0u);
	__vmwrite(GUEST_SS_LIMIT, ~0u);
	__vmwrite(GUEST_DS_LIMIT, ~0u);
	__vmwrite(GUEST_FS_LIMIT, ~0u);
	__vmwrite(GUEST_GS_LIMIT, ~0u);
	__vmwrite(GUEST_CS_LIMIT, ~0u);

	/* Guest segment AR bytes. */
	__vmwrite(GUEST_ES_AR_BYTES, 0x93);
	__vmwrite(GUEST_SS_AR_BYTES, 0x193);
	__vmwrite(GUEST_DS_AR_BYTES, 0x93);
	__vmwrite(GUEST_FS_AR_BYTES, 0x93);
	__vmwrite(GUEST_GS_AR_BYTES, 0x93);
	__vmwrite(GUEST_CS_AR_BYTES, 0x19b);

	/* Guest segment selector. */
	__vmwrite(GUEST_ES_SELECTOR, 0);
	__vmwrite(GUEST_SS_SELECTOR, 0);
	__vmwrite(GUEST_DS_SELECTOR, 0x0040);
	__vmwrite(GUEST_FS_SELECTOR, 0xE717);
	__vmwrite(GUEST_GS_SELECTOR, 0xF000);
	__vmwrite(GUEST_CS_SELECTOR, 0);

	/* Guest IDT. */
	__vmwrite(GUEST_IDTR_BASE, 0);
	__vmwrite(GUEST_IDTR_LIMIT, 0);

	/* Guest GDT. */
	__vmwrite(GUEST_GDTR_BASE, 0);
	__vmwrite(GUEST_GDTR_LIMIT, 0);

	/* Guest LDT. */
	__vmwrite(GUEST_LDTR_AR_BYTES, 0x0082); /* LDT */
	__vmwrite(GUEST_LDTR_SELECTOR, 0);
	__vmwrite(GUEST_LDTR_BASE, 0);
	__vmwrite(GUEST_LDTR_LIMIT, 0);

	/* Guest TSS. */
	__vmwrite(GUEST_TR_AR_BYTES, 0x008b); /* 32-bit TSS (busy) */
	__vmwrite(GUEST_TR_BASE, 0);
	__vmwrite(GUEST_TR_LIMIT, 0xff);

	__vmwrite(GUEST_INTERRUPTIBILITY_INFO, 0);
	__vmwrite(GUEST_DR7, 0);
	__vmwrite(VMCS_LINK_POINTER, ~0UL);

	__vmwrite(EXCEPTION_BITMAP, 0);

	/* Control registers */
	__vmwrite(GUEST_CR0, (X86_CR0_PE | X86_CR0_ET));
	__vmwrite(GUEST_CR3, 0);
	__vmwrite(GUEST_CR4, 0);

	/* G_PAT */
	u64 host_pat, guest_pat;

	host_pat = cpu_read_msr(MSR_IA32_CR_PAT);
	guest_pat = MSR_IA32_CR_PAT_RESET;

	__vmwrite(HOST_PAT, host_pat);
	__vmwrite(GUEST_PAT, guest_pat);

	/* Initial state */
	__vmwrite(GUEST_RSP, 0x3E2);
	__vmwrite(GUEST_RFLAGS, 0x2206);
	__vmwrite(GUEST_RIP, 0x7C00);
}

int vmx_read_guest_msr(struct vcpu_hw_context *context, u32 msr, u64 *val)
{
	unsigned int i, msr_count = context->msr_count;
	const struct vmx_msr_entry *msr_area = context->msr_area;

	for ( i = 0; i < msr_count; i++ ) {
		if ( msr_area[i].index == msr ) {
			*val = msr_area[i].data;
			return 0;
		}
	}

	return -1;
}

int vmx_write_guest_msr(struct vcpu_hw_context *context, u32 msr, u64 val)
{
	unsigned int i, msr_count = context->msr_count;
	struct vmx_msr_entry *msr_area = context->msr_area;

	for ( i = 0; i < msr_count; i++ ) {
		if ( msr_area[i].index == msr ) {
			msr_area[i].data = val;
			return 0;
		}
	}

	return -1;
}

int vmx_add_guest_msr(struct vcpu_hw_context *context, u32 msr)
{
	unsigned int i, msr_count = context->msr_count;
	struct vmx_msr_entry *msr_area = context->msr_area;

	if ( msr_area == NULL ) {
		if ( (msr_area = (struct vmx_msr_entry *)vmm_host_alloc_pages(1, 1)) == NULL )
			return -2;
		context->msr_area = msr_area;
		__vmwrite(VM_EXIT_MSR_STORE_ADDR, (u64)msr_area);
		__vmwrite(VM_ENTRY_MSR_LOAD_ADDR, (u64)msr_area);
	}

	for ( i = 0; i < msr_count; i++ )
		if ( msr_area[i].index == msr )
			return 0;

	if ( msr_count == (PAGE_SIZE / sizeof(struct vmx_msr_entry)) )
		return -1;

	msr_area[msr_count].index = msr;
	msr_area[msr_count].mbz   = 0;
	msr_area[msr_count].data  = 0;
	context->msr_count = ++msr_count;
	__vmwrite(VM_EXIT_MSR_STORE_COUNT, msr_count);
	__vmwrite(VM_ENTRY_MSR_LOAD_COUNT, msr_count);

	return 0;
}

int vmx_add_host_load_msr(struct vcpu_hw_context *context, u32 msr)
{
	unsigned int i, msr_count = context->host_msr_count;
	struct vmx_msr_entry *msr_area = context->host_msr_area;

	if (msr_area == NULL) {
		if ((msr_area = (struct vmx_msr_entry *)vmm_host_alloc_pages(1,
									     VMM_MEMORY_FLAGS_IO))
		    == NULL )
			return VMM_ENOMEM;
		context->host_msr_area = msr_area;
		__vmwrite(VM_EXIT_MSR_LOAD_ADDR, (u64)msr_area);
	}

	for ( i = 0; i < msr_count; i++ )
		if ( msr_area[i].index == msr )
			return 0;

	if ( msr_count == (PAGE_SIZE / sizeof(struct vmx_msr_entry)) )
		return -1;

	msr_area[msr_count].index = msr;
	msr_area[msr_count].mbz   = 0;
	msr_area[msr_count].data = cpu_read_msr(msr);
	context->host_msr_count = ++msr_count;
	__vmwrite(VM_EXIT_MSR_LOAD_COUNT, msr_count);

	return 0;
}

static unsigned long vmr(unsigned long field)
{
	int rc;
	unsigned long val;
	val = __vmread_safe(field, &rc);
	return rc ? 0 : val;
}

static void __unused vmx_dump_sel(char *name, u32 selector)
{
	u32 sel, attr, limit;
	u64 base;
	sel = vmr(selector);
	attr = vmr(selector + (GUEST_ES_AR_BYTES - GUEST_ES_SELECTOR));
	limit = vmr(selector + (GUEST_ES_LIMIT - GUEST_ES_SELECTOR));
	base = vmr(selector + (GUEST_ES_BASE - GUEST_ES_SELECTOR));
	vmm_printf("%s: sel=0x%"PRIx32", attr=0x%"PRIx32
		   ", limit=0x%"PRIx32", base=0x%"PRIx64"\n",
		   name, sel, attr, limit, base);
}

static void __unused vmx_dump_sel2(char *name, u32 lim)
{
	u32 limit;
	u64 base;
	limit = vmr(lim);
	base = vmr(lim + (GUEST_GDTR_BASE - GUEST_GDTR_LIMIT));
	vmm_printf("%s:                           "
		   "limit=0x%"PRIx32", base=0x%"PRIx64"\n",
		   name, limit, base);
}

void vmcs_dump(struct vcpu_hw_context *context)
{
	#if 0
	unsigned long long x;

	vmm_printf("*** Guest State ***\n");
	vmm_printf("CR0: actual=0x%016llx, shadow=0x%016llx, gh_mask=%016llx\n",
		   (unsigned long long)vmr(GUEST_CR0),
		   (unsigned long long)vmr(CR0_READ_SHADOW),
		   (unsigned long long)vmr(CR0_GUEST_HOST_MASK));
	vmm_printf("CR4: actual=0x%016llx, shadow=0x%016llx, gh_mask=%016llx\n",
		   (unsigned long long)vmr(GUEST_CR4),
		   (unsigned long long)vmr(CR4_READ_SHADOW),
		   (unsigned long long)vmr(CR4_GUEST_HOST_MASK));
	vmm_printf("CR3: actual=0x%016llx, target_count=%d\n",
		   (unsigned long long)vmr(GUEST_CR3),
		   (int)vmr(CR3_TARGET_COUNT));
	vmm_printf("     target0=%016llx, target1=%016llx\n",
		   (unsigned long long)vmr(CR3_TARGET_VALUE0),
		   (unsigned long long)vmr(CR3_TARGET_VALUE1));
	vmm_printf("     target2=%016llx, target3=%016llx\n",
		   (unsigned long long)vmr(CR3_TARGET_VALUE2),
		   (unsigned long long)vmr(CR3_TARGET_VALUE3));
	vmm_printf("RSP = 0x%016llx, RIP = 0x%016llx\n",
		   (unsigned long long)vmr(GUEST_RSP),
		   (unsigned long long)vmr(GUEST_RIP));
	vmm_printf("RFLAGS=0x%016llx DR7 = 0x%016llx\n",
		   (unsigned long long)vmr(GUEST_RFLAGS),
		   (unsigned long long)vmr(GUEST_DR7));
	vmm_printf("Sysenter RSP=%016llx CS:RIP=%04x:%016llx\n",
		   (unsigned long long)vmr(GUEST_SYSENTER_ESP),
		   (int)vmr(GUEST_SYSENTER_CS),
		   (unsigned long long)vmr(GUEST_SYSENTER_EIP));
	vmx_dump_sel("CS", GUEST_CS_SELECTOR);
	vmx_dump_sel("DS", GUEST_DS_SELECTOR);
	vmx_dump_sel("SS", GUEST_SS_SELECTOR);
	vmx_dump_sel("ES", GUEST_ES_SELECTOR);
	vmx_dump_sel("FS", GUEST_FS_SELECTOR);
	vmx_dump_sel("GS", GUEST_GS_SELECTOR);
	vmx_dump_sel2("GDTR", GUEST_GDTR_LIMIT);
	vmx_dump_sel("LDTR", GUEST_LDTR_SELECTOR);
	vmx_dump_sel2("IDTR", GUEST_IDTR_LIMIT);
	vmx_dump_sel("TR", GUEST_TR_SELECTOR);
	vmm_printf("Guest PAT = 0x%08x%08x\n",
		   (u32)vmr(GUEST_PAT_HIGH), (u32)vmr(GUEST_PAT));
	x  = (unsigned long long)vmr(TSC_OFFSET_HIGH) << 32;
	x |= (u32)vmr(TSC_OFFSET);
	vmm_printf("TSC Offset = %016llx\n", x);
	x  = (unsigned long long)vmr(GUEST_IA32_DEBUGCTL_HIGH) << 32;
	x |= (u32)vmr(GUEST_IA32_DEBUGCTL);
	vmm_printf("DebugCtl=%016llx DebugExceptions=%016llx\n", x,
		   (unsigned long long)vmr(GUEST_PENDING_DBG_EXCEPTIONS));
	vmm_printf("Interruptibility=%04x ActivityState=%04x\n",
		   (int)vmr(GUEST_INTERRUPTIBILITY_INFO),
		   (int)vmr(GUEST_ACTIVITY_STATE));

	vmm_printf("*** Host State ***\n");
	vmm_printf("RSP = 0x%016llx  RIP = 0x%016llx\n",
		   (unsigned long long)vmr(HOST_RSP),
		   (unsigned long long)vmr(HOST_RIP));
	vmm_printf("CS=%04x DS=%04x ES=%04x FS=%04x GS=%04x SS=%04x TR=%04x\n",
		   (u16)vmr(HOST_CS_SELECTOR),
		   (u16)vmr(HOST_DS_SELECTOR),
		   (u16)vmr(HOST_ES_SELECTOR),
		   (u16)vmr(HOST_FS_SELECTOR),
		   (u16)vmr(HOST_GS_SELECTOR),
		   (u16)vmr(HOST_SS_SELECTOR),
		   (u16)vmr(HOST_TR_SELECTOR));
	vmm_printf("FSBase=%016llx GSBase=%016llx TRBase=%016llx\n",
		   (unsigned long long)vmr(HOST_FS_BASE),
		   (unsigned long long)vmr(HOST_GS_BASE),
		   (unsigned long long)vmr(HOST_TR_BASE));
	vmm_printf("GDTBase=%016llx IDTBase=%016llx\n",
		   (unsigned long long)vmr(HOST_GDTR_BASE),
		   (unsigned long long)vmr(HOST_IDTR_BASE));
	vmm_printf("CR0=%016llx CR3=%016llx CR4=%016llx\n",
		   (unsigned long long)vmr(HOST_CR0),
		   (unsigned long long)vmr(HOST_CR3),
		   (unsigned long long)vmr(HOST_CR4));
	vmm_printf("Sysenter RSP=%016llx CS:RIP=%04x:%016llx\n",
		   (unsigned long long)vmr(HOST_SYSENTER_ESP),
		   (int)vmr(HOST_SYSENTER_CS),
		   (unsigned long long)vmr(HOST_SYSENTER_EIP));
	vmm_printf("Host PAT = 0x%08x%08x\n",
		   (u32)vmr(HOST_PAT_HIGH), (u32)vmr(HOST_PAT));
#endif
	vmm_printf("*** Control State ***\n");
	vmm_printf("PinBased=%08x CPUBased=%08x SecondaryExec=%08x\n",
		   (u32)vmr(PIN_BASED_VM_EXEC_CONTROL),
		   (u32)vmr(CPU_BASED_VM_EXEC_CONTROL),
		   (u32)vmr(SECONDARY_VM_EXEC_CONTROL));
	vmm_printf("EntryControls=%08x ExitControls=%08x\n",
		   (u32)vmr(VM_ENTRY_CONTROLS),
		   (u32)vmr(VM_EXIT_CONTROLS));
	vmm_printf("ExceptionBitmap=%08x\n",
		   (u32)vmr(EXCEPTION_BITMAP));
	vmm_printf("VMEntry: intr_info=%08x errcode=%08x ilen=%08x\n",
		   (u32)vmr(VM_ENTRY_INTR_INFO),
		   (u32)vmr(VM_ENTRY_EXCEPTION_ERROR_CODE),
		   (u32)vmr(VM_ENTRY_INSTRUCTION_LEN));
	vmm_printf("VMExit: intr_info=%08x errcode=%08x ilen=%08x\n",
		   (u32)vmr(VM_EXIT_INTR_INFO),
		   (u32)vmr(VM_EXIT_INTR_ERROR_CODE),
		   (u32)vmr(VM_ENTRY_INSTRUCTION_LEN));
	vmm_printf("        reason=%08x qualification=%08x\n",
		   (u32)vmr(VM_EXIT_REASON),
		   (u32)vmr(EXIT_QUALIFICATION));
	vmm_printf("IDTVectoring: info=%08x errcode=%08x\n",
		   (u32)vmr(IDT_VECTORING_INFO),
		   (u32)vmr(IDT_VECTORING_ERROR_CODE));
	vmm_printf("TPR Threshold = 0x%02x\n",
		   (u32)vmr(TPR_THRESHOLD));
	vmm_printf("EPT pointer = 0x%08x%08x\n",
		   (u32)vmr(EPT_POINTER_HIGH), (u32)vmr(EPT_POINTER));
	vmm_printf("Virtual processor ID = 0x%04x\n",
		   (u32)vmr(VIRTUAL_PROCESSOR_ID));
}
