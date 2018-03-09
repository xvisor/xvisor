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
	unsigned long reason = 0;
	int rc;

	if (unlikely((rc = __vmread(VM_INSTRUCTION_ERROR, &reason)) != VMM_OK))
		if (likely(context->vcpu_emergency_shutdown))
			context->vcpu_emergency_shutdown(context);

	vmm_printf("Guest Exited with Error: 0x%ld\n", reason);

	context->vcpu_emergency_shutdown(context);
}
