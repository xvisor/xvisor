/**
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file vm.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Generic vm related code.
 */
#include <vmm_stdio.h>
#include <vmm_types.h>
#include <vmm_host_aspace.h>
#include <vmm_error.h>
#include <libs/stringlib.h>
#include <cpu_vm.h>
#include <cpu_features.h>
#include <vm/amd_svm.h>
#include <vm/amd_intercept.h>

physical_addr_t cpu_create_vcpu_intercept_table(size_t size)
{
	physical_addr_t phys = 0;

	virtual_addr_t vaddr = vmm_host_alloc_pages(size >> VMM_PAGE_SHIFT,
						    VMM_MEMORY_FLAGS_NORMAL);

	if (vmm_host_va2pa(vaddr, &phys) != VMM_OK)
		return 0;

	memset((void *)vaddr, 0x00, size);

	return phys;
}

/*
 * Create the nested paging table that map guest physical to host physical
 * return the (host) physical base address of the table.
 * Note: Nested paging table must use the same paging mode as the host,
 * regardless of guest paging mode - See AMD man vol2:
 * The extra translation uses the same paging mode as the VMM used when it
 * executed the most recent VMRUN.
 *
 * Also, it is important to note that gCR3 and the guest page table entries
 * contain guest physical addresses, not system physical addresses.
 * Hence, before accessing a guest page table entry, the table walker first
 * translates that entry’s guest physical address into a system physical
 * address.
 */
static unsigned long create_nested_pagetable (unsigned long vmm_pmem_start,
					      unsigned long vmm_pmem_size,
					      unsigned long page_shift)
{

	unsigned long g_cr3;

	g_cr3 = 0;

	return g_cr3;
}

void cpu_disable_vcpu_intercept(struct vcpu_hw_context *context, int flags)
{
	/* disable taskswitch interception */
	if (flags & USER_ITC_TASKSWITCH) {
		vmm_printf("Disable taskswitch interception\n");
		context->vmcb->cr_intercepts &= ~INTRCPT_WRITE_CR3;
	}

	if (flags & USER_ITC_SWINT) {
		vmm_printf("Disable software interrupt interception\n");
		context->vmcb->general1_intercepts &= ~INTRCPT_INTN;
	}

	if (flags & USER_ITC_IRET) {
		vmm_printf("Enable software interrupt interception\n");
		context->vmcb->general1_intercepts &= ~INTRCPT_IRET;
	}

	if (flags & USER_ITC_SYSCALL) {
		vmm_printf("Disable syscall interception\n");
		context->vmcb->general1_intercepts &= ~INTRCPT_INTN;
	}

	/* disable single stepping */
	if (flags & USER_SINGLE_STEPPING) {
		vmm_printf("Disable single stepping\n");
		context->vmcb->rflags &= ~X86_EFLAGS_TF;
		context->vmcb->exception_intercepts &= ~INTRCPT_DB;
	}
}

void cpu_enable_vcpu_intercept(struct vcpu_hw_context *context, int flags)
{
	/* enable taskswitch interception */
	if (flags & USER_ITC_TASKSWITCH) {
		vmm_printf("Enable taskswitch interception\n");
		context->vmcb->cr_intercepts |= INTRCPT_WRITE_CR3;
	}

	/* enable software interrupt interception */
	if (flags & USER_ITC_SWINT) {
		vmm_printf("Enable software interrupt interception\n");
		context->vmcb->general1_intercepts |= INTRCPT_INTN;
	}

	if (flags & USER_ITC_IRET) {
		vmm_printf("Enable software interrupt interception\n");
		context->vmcb->general1_intercepts |= INTRCPT_IRET;
	}
}

int cpu_init_vcpu_hw_context(struct cpuinfo_x86 *cpuinfo,
			     struct vcpu_hw_context *context,
			     unsigned long vmm_pmem_start,
			     unsigned long vmm_pmem_size)
{
	int ret = VMM_EFAIL;

	memset((char *)context, 0, sizeof(struct vcpu_hw_context));

	/* create a guest phys -> host phys */
	context->n_cr3 = create_nested_pagetable(vmm_pmem_start,
						 vmm_pmem_size,
						 21 /* 2mb page */);

	if (!context->n_cr3)
		goto _error;

	context->io_intercept_table = cpu_create_vcpu_intercept_table(IO_INTCPT_TBL_SZ);
	context->msr_intercept_table = cpu_create_vcpu_intercept_table(MSR_INTCPT_TBL_SZ);

	switch (cpuinfo->vendor) {
	case x86_VENDOR_AMD:
		if((ret = amd_setup_vm_control(context)) != VMM_OK)
			goto _error;
		break;

	default:
		goto _error;
		break;
	}

	return VMM_OK;

 _error:
	/* FIXME: VM: Free nested page table pages. */
	return VMM_EFAIL;
}

/*
 * Identify the CPU and enable the VM feature on it.
 * Only AMD processors are supported right now.
 */
int cpu_enable_vm_extensions(struct cpuinfo_x86 *cpuinfo)
{
	int ret = VMM_EFAIL;;

	switch (cpuinfo->vendor) {
	case x86_VENDOR_AMD:
		vmm_printf("initializing vm extensions for AMD.\n");
		ret = init_amd(cpuinfo);
		break;

	case x86_VENDOR_INTEL:
		vmm_panic("Intel CPUs not supported yet!\n");
		break;

	case x86_VENDOR_UNKNOWN:
	default:
		vmm_panic("Unknown CPU vendor: %d\n", cpuinfo->vendor);
		break;
	}

	return ret;
}

void cpu_boot_vcpu(struct vcpu_hw_context *context)
{
	for(;;)	{
		context->vcpu_run(context);
		context->vcpu_exit(context);
	}
}
