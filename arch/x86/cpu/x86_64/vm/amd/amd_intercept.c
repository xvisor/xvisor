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
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <cpu_vm.h>
#include <cpu_features.h>
#include <vm/amd_intercept.h>
#include <vm/amd_svm.h>
#include <vmm_manager.h>
#include <vmm_main.h>

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
		print_vmcb_state(context->vmcb);
		break;

	case VMEXIT_EXCEPTION_GP:
		print_vmcb_state(context->vmcb);

		/*
		  context->vmcb->cs.sel = context->org_sysenter_cs;
		  context->vmcb->ss.sel = context->org_sysenter_cs + 8;
		  context->vmcb->rsp = context->vmcb->sysenter_esp;
		  context->vmcb->rip = context->vmcb->sysenter_eip;
		*/
		return;

	case VMEXIT_EXCEPTION_DB:
		if (context->itc_skip_flag) {
			cpu_enable_vcpu_intercept(context, context->itc_skip_flag);
			context->itc_skip_flag = 0;

			/*
			  if (!context->btrackcurrent || !(context->itc_flag & USER_SINGLE_STEPPING))
			  vm_disable_intercept(vm, USER_SINGLE_STEPPING);
			*/
		}

		return;

	case VMEXIT_EXCEPTION_PF: //VECTOR_PF
		if ((context->vmcb->exitinfo1 & 1) && (context->vmcb->exitinfo1 & 2) && (context->itc_flag & USER_UNPACK))
		{
			//__skip_intercpt_cur_instr(vm, USER_UNPACK); //skip handling next instruction

			return;
		}
		else print_page_errorcode(context->vmcb->exitinfo1);

		context->vmcb->cr2 = context->vmcb->exitinfo2;
		break;
	}

	int vector =  context->vmcb->exitcode - VMEXIT_EXCEPTION_DE;

	context->vmcb->eventinj.fields.vector = vector;
	context->vmcb->eventinj.fields.type = EVENT_TYPE_EXCEPTION;
	context->vmcb->eventinj.fields.ev = 1;
	context->vmcb->eventinj.fields.v = 1;
	context->vmcb->eventinj.fields.errorcode = context->vmcb->exitinfo1;
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

void __handle_cr3_write(struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: cr3 write.\n");
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
	VM_LOG(LVL_DEBUG, "Triple fault in guest: %s!!\n", context->assoc_vcpu->guest->name);

	if (context->vcpu_emergency_shutdown)
		context->vcpu_emergency_shutdown(context);

	vmm_hang();
}

void handle_vcpuexit(struct vcpu_hw_context *context)
{
	VM_LOG(LVL_DEBUG, "**** #VMEXIT - exit code: %x\n", (u32) context->vmcb->exitcode);
	print_vmcb_state(context->vmcb);

	switch (context->vmcb->exitcode) {
	case VMEXIT_MSR:
		if (context->vmcb->exitinfo1 == 1) __handle_vm_wrmsr (context);
		break;
	case VMEXIT_EXCEPTION_DE ... VMEXIT_EXCEPTION_XF:
		__handle_vm_exception(context); break;

	case VMEXIT_SWINT: __handle_vm_swint(context); break;
	case VMEXIT_NPF: __handle_vm_npf (context); break;
	case VMEXIT_VMMCALL: __handle_vm_vmmcall(context); break;
	case VMEXIT_IRET: __handle_vm_iret(context); break;
	case VMEXIT_CR3_WRITE: __handle_cr3_write(context);break;
	case VMEXIT_POPF: __handle_popf(context); break;
	case VMEXIT_SHUTDOWN: __handle_triple_fault(context); break;
	}
}
