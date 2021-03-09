/**
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
 *
 * @file vmx_intercept.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Intel guest intercept related defines.
 */

#ifndef _VMX_INTERCEPT_H__
#define _VMX_INTERCEPT_H__

#include <vmm_types.h>
#include <cpu_vm.h>

#define VMX_EPTV_GUEST_LINEAR_ADDRESS_VALID		(0x1UL << 7)
#define VMX_EPTV_GUEST_LINEAR_ADDRESS_TRANSLATED_MASK	(0x1UL << 8)

typedef union _er {
	struct _vmexit {
		u32 reason:16;
		u32 other:15;
		u32 vm_entry_failure:1;
	} bits;
	unsigned long r;
} exit_reason_t;

typedef union vmx_io_exit_qualification {
	u64 val;
	struct {
		u32 io_size:3;
		u32 direction:1;
		u32 str_inst:1;
		u32 rep_prefix:1;
		u32 op_encoding:1;
		u32 res:9;
		u32 port:16;
		u32 res1:32;
	} bits;
} vmx_io_exit_qualification_t;

#define VMX_GUEST_SAVE_EQ(__context) \
	(__context->vmx_last_exit_qualification = vmr(EXIT_QUALIFICATION))
#define VMX_GUEST_EQ(__context) \
	(__context->vmx_last_exit_qualification)

#define VMX_GUEST_SAVE_CR0(__context) \
	(__context->g_cr0 = vmr(GUEST_CR0))
#define VMX_GUEST_CR0(__context) (__context->g_cr0)

#define VMX_GUEST_SAVE_RIP(__context) \
	(__context->g_rip = vmr(GUEST_RIP))
#define VMX_GUEST_RIP(__context) (__context->g_rip)

#define VMX_GUEST_NEXT_RIP(__context) \
	(__context->g_rip + vmr(VM_EXIT_INSTRUCTION_LEN))


extern void vmx_vcpu_exit(struct vcpu_hw_context *context);

static inline u8 is_guest_linear_address_valid(u64 qualification)
{
	return (qualification & VMX_EPTV_GUEST_LINEAR_ADDRESS_VALID);
}

static inline u8 is_guest_address_translated(u64 qualification)
{
	return (!(qualification & VMX_EPTV_GUEST_LINEAR_ADDRESS_TRANSLATED_MASK));
}

#endif
