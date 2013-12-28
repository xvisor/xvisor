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
 * @file amd_intercept.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief SVM intercept handling/registration code.
 */
#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <cpu_vm.h>
#include <cpu_features.h>
#include <cpu_mmu.h>
#include <cpu_pgtbl_helper.h>
#include <vm/amd_intercept.h>
#include <vm/amd_svm.h>
#include <vmm_manager.h>
#include <vmm_main.h>

extern int realmode_map_memory(struct vcpu_hw_context *context, virtual_addr_t vaddr,
			       physical_addr_t paddr, size_t size);

static inline void dump_guest_exception_insts(struct vmcb *vmcb)
{
	int i;
	u8 *guest_ins_base = (u8 *)((u8 *)(vmcb)) + 0xd0;

	for (i = 0; i < 16; i++) {
		vmm_printf("%x ", guest_ins_base[i]);
		if (i && !(i % 8)) vmm_printf("\n");
	}
}

static inline int guest_in_realmode(struct vmcb *vmcb)
{
	return (!(vmcb->cr0 & X86_CR0_PE));
}

void __handle_vm_npf (struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: nested page fault.\n");
}

void __handle_vm_swint (struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: software interrupt.\n");
}

void __handle_vm_exception (struct vcpu_hw_context *context)
{
	switch (context->vmcb->exitcode)
	{
	case VMEXIT_EXCEPTION_TS:
		break;

	case VMEXIT_EXCEPTION_GP:
		return;

	case VMEXIT_EXCEPTION_DB:
		if (context->itc_skip_flag) {
			cpu_enable_vcpu_intercept(context, context->itc_skip_flag);
			context->itc_skip_flag = 0;
		}

		return;

	case VMEXIT_EXCEPTION_PF:
		/*
		 * If guest is in real mode, this fault is xvisor induced
		 * So CR2 doesn't need to be updated as guest doesn't
		 * know about it.
		 */
		if (unlikely(guest_in_realmode(context->vmcb))) {
			VM_LOG(LVL_DEBUG, "Guest fault: 0x%x (rIP: %x)\n",
			       context->vmcb->exitinfo2, context->vmcb->rip);

			u64 fault_gphys = context->vmcb->exitinfo2;
			/* Guest is in real mode so faulting guest virtual is
			 * guest physical address. We just need to add faulting
			 * address as offset to host physical address to get
			 * the destination physical address.
			 */
			struct vmm_region *g_reg = vmm_guest_find_region(context->assoc_vcpu->guest,
									 fault_gphys,
									 VMM_REGION_REAL | VMM_REGION_MEMORY,
									 FALSE);
			if (!g_reg) {
				vmm_printf("ERROR: Can't find the host physical address for guest physical: 0x%lx\n",
					   fault_gphys);
				if (context->vcpu_emergency_shutdown) {
					/* FIXME: Better way to do it? */
					vmm_printf("Shutting the VM down.\n");
					context->vcpu_emergency_shutdown(context);
				}

				vmm_hang();
			}

			if (realmode_map_memory(context, fault_gphys,
						(g_reg->hphys_addr + fault_gphys),
						PAGE_SIZE) != VMM_OK) {
				vmm_printf("ERROR: Failed to create map in guest's shadow page table.\n");
				if (context->vcpu_emergency_shutdown){
					context->vcpu_emergency_shutdown(context);
				}
				vmm_hang();
			}
		} else {
			context->vmcb->cr2 = context->vmcb->exitinfo2;
		}
		break;
	}
}

void __handle_vm_wrmsr (struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: msr write.\n");
}

void __handle_popf(struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: popf.\n");
}

void __handle_vm_vmmcall (struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: vmmcall.\n");
}

void __handle_vm_iret(struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: iret.\n");
}

void __handle_idtr_write(struct vcpu_hw_context *context)
{
	vmm_printf("%s: info1: %lx info2: %lx rip: %x\n",
		   __func__, context->vmcb->exitinfo1,
		   context->vmcb->exitinfo2, context->vmcb->rip);
}

void __handle_gdtr_write(struct vcpu_hw_context *context)
{
	vmm_printf("%s: info1: %lx info2: %lx rip: %x\n",
		   __func__, context->vmcb->exitinfo1,
		   context->vmcb->exitinfo2, context->vmcb->rip);
}
void __handle_idtr_read(struct vcpu_hw_context *context)
{
	vmm_printf("%s: info1: %lx info2: %lx rip: %x\n",
		   __func__, context->vmcb->exitinfo1,
		   context->vmcb->exitinfo2, context->vmcb->rip);
}

void __handle_gdtr_read(struct vcpu_hw_context *context)
{
	vmm_printf("%s: info1: %lx info2: %lx rip: %x\n",
		   __func__, context->vmcb->exitinfo1,
		   context->vmcb->exitinfo2, context->vmcb->rip);
}

void __handle_crN_read(struct vcpu_hw_context *context)
{
	int crn = context->vmcb->exitcode - VMEXIT_CR0_READ;
	switch(crn) {
	case 0:
		break;
	case 3:
		break;
	default:
		VM_LOG(LVL_ERR, "Unhandled intercept cr%d read\n",
		       crn);
		break;
	}
}

void __handle_crN_write(struct vcpu_hw_context *context)
{
	int crn = context->vmcb->exitcode - VMEXIT_CR0_WRITE;
	int cr_gpr;

	switch(crn) {
	case 0:
		if (context->vmcb->exitinfo1 & VALID_CRN_TRAP) {
			cr_gpr = (context->vmcb->exitinfo1 & 0xf);
		}
		break;
	case 3:
		break;
	default:
		VM_LOG(LVL_ERR, "Unhandled intercept cr%d write\n",
		       crn);
		break;
	}
}
/**
 * \brief Handle the shutdown condition in guest.
 *
 * If the guest has seen a shutdown condition (a.k.a. triple fault)
 * give the notification to guest and the guest must be
 * destroyed then. If the guest as multiple vCPUs, all of then
 * should be sent a notification of this.
 *
 * @param context
 * The hardware context of the vcpu of the guest which saw the triple fault.
 */
void __handle_triple_fault(struct vcpu_hw_context *context)
{
	VM_LOG(LVL_ERR, "Triple fault in guest: %s!!\n", context->assoc_vcpu->guest->name);

	if (context->vcpu_emergency_shutdown)
		context->vcpu_emergency_shutdown(context);

	vmm_hang();
}

void handle_vcpuexit(struct vcpu_hw_context *context)
{
	VM_LOG(LVL_DEBUG, "**** #VMEXIT - exit code: %x\n", (u32) context->vmcb->exitcode);

	switch (context->vmcb->exitcode) {
	case VMEXIT_CR0_READ ... VMEXIT_CR15_READ: __handle_crN_read(context); break;
	case VMEXIT_CR0_WRITE ... VMEXIT_CR15_WRITE: __handle_crN_write(context); break;
	case VMEXIT_MSR:
		if (context->vmcb->exitinfo1 == 1) __handle_vm_wrmsr (context);
		break;
	case VMEXIT_EXCEPTION_DE ... VMEXIT_EXCEPTION_XF:
		__handle_vm_exception(context); break;

	case VMEXIT_SWINT: __handle_vm_swint(context); break;
	case VMEXIT_NPF: __handle_vm_npf (context); break;
	case VMEXIT_VMMCALL: __handle_vm_vmmcall(context); break;
	case VMEXIT_IRET: __handle_vm_iret(context); break;
	case VMEXIT_POPF: __handle_popf(context); break;
	case VMEXIT_SHUTDOWN: __handle_triple_fault(context); break;
	case VMEXIT_IDTR_WRITE: __handle_idtr_write(context); break;
	case VMEXIT_GDTR_WRITE: __handle_gdtr_write(context); break;
	case VMEXIT_IDTR_READ: __handle_idtr_read(context); break;
	case VMEXIT_GDTR_READ: __handle_gdtr_read(context); break;
	}
}
