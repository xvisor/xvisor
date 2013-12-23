/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file arch_guest_helper.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Guest management functions.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <vmm_guest_aspace.h>
#include <cpu_mmu.h>
#include <cpu_features.h>
#include <cpu_vm.h>
#include <libs/stringlib.h>
#include <arch_guest_helper.h>

int arch_guest_init(struct vmm_guest * guest)
{
	struct x86_guest_priv *priv = vmm_zalloc(sizeof(struct x86_guest_priv));

	if (priv == NULL) {
		VM_LOG(LVL_ERR, "ERROR: Failed to create guest private data.\n");
		return VMM_EFAIL;
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
	 * translates that entry's guest physical address into a system physical
	 * address.
	 *
	 * npt should be created should be used when nested page table
	 * walking is available. But we will create it anyways since
	 * we are creating only the first level.
	 */
	priv->g_npt = mmu_pgtbl_alloc(&host_pgtbl_ctl, PGTBL_STAGE_2);

	if (priv->g_npt == NULL) {
		VM_LOG(LVL_ERR, "ERROR: Failed to create nested page table for guest.\n");
		vmm_free(priv);
		return VMM_EFAIL;
	}

	guest->arch_priv = (void *)priv;

	VM_LOG(LVL_VERBOSE, "Guest init successful!\n");
	return VMM_OK;
}

int arch_guest_deinit(struct vmm_guest * guest)
{
	struct x86_guest_priv *priv = x86_guest_priv(guest);

	if (priv) {
		if (mmu_pgtbl_free(&host_pgtbl_ctl, priv->g_npt) != VMM_OK)
			VM_LOG(LVL_ERR, "ERROR: Failed to unmap the nested page table. Will Leak.\n");

		vmm_free(priv);
	}

	return VMM_OK;
}
