/**
 * Copyright (c) 2015 Himanshu Chauhan.
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
 * @file intel_intercept.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief VMX intercept handling code.
 */
#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <cpu_vm.h>
#include <cpu_inst_decode.h>
#include <cpu_features.h>
#include <cpu_mmu.h>
#include <cpu_pgtbl_helper.h>
#include <arch_guest_helper.h>
#include <vmm_devemu.h>
#include <vmm_manager.h>
#include <vmm_main.h>
#include <vm/vmcs.h>
#include <vm/vmx.h>

void vmx_vcpu_exit(struct vcpu_hw_context *context)
{
	union _er {
		struct _vmexit {
			u32 reason:16;
			u32 other:15;
			u32 vm_entry_failure:1;
		} bits;
		unsigned long r;
	} _exit_reason;

	unsigned long reason = 0;
	int rc;

	if (unlikely((rc = __vmread(VM_EXIT_REASON, &reason)) != VMM_OK))
		if (likely(context->vcpu_emergency_shutdown))
			context->vcpu_emergency_shutdown(context);

	_exit_reason.r = reason;

	if (_exit_reason.bits.vm_entry_failure) {
		switch(_exit_reason.bits.reason) {
		case 33:
			vmm_printf("VM Entry failed due to invalid guest state.\n");
			break;
		case 34:
			vmm_printf("VM Entry failed due to MSR loading.\n");
			break;
		case 41:
			vmm_printf("VM Entry failed due to machine-check event.\n");
			break;
		default:
			vmm_printf("VM Entry failed due to unknown reason %d.\n", _exit_reason.bits.reason);
			break;
		}
	} else {
		vmm_printf("VM Exit reason: %d\n", _exit_reason.bits.reason);
	}

	context->vcpu_emergency_shutdown(context);
}
