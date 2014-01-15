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
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <vmm_guest_aspace.h>
#include <vmm_host_aspace.h>
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

/*---------------------------------*
 * Guest's vCPU's helper funstions *
 *---------------------------------*/
physical_addr_t guest_virtual_to_physical(struct vcpu_hw_context *context, virtual_addr_t vaddr)
{
	/* If guest is in real or paged real mode, virtual = physical. */
	if (!(context->vmcb->cr0 & X86_CR0_PG)
	    ||((context->vmcb->cr0 & X86_CR0_PG)
	       && !(context->vmcb->cr0 & X86_CR0_PE)))
		return vaddr;

	/*
	 * FIXME: Check if guest has moved to long mode, in which case
	 * This page walk won't apply. This is only for 32-bit systems.
	 *
	 * FIXME: Here physical address extension and page size extention
	 * is not accounted.
	 */
        u32 pde, pte;
	physical_addr_t pte_addr, pde_addr;

        /* page directory entry */
        pde_addr = (context->vmcb->cr3 & ~0xfff) + ((vaddr >> 20) & 0xffc);
	if (vmm_guest_memory_read(context->assoc_vcpu->guest, pde_addr, &pde,
				  sizeof(pde)) < sizeof(pde))
		return 0;

        if (!(pde & 0x1))
		return 0;

	/* page directory entry */
	pte_addr = ((pde & ~0xfff) + ((vaddr >> 10) & 0xffc));
	if (vmm_guest_memory_read(context->assoc_vcpu->guest, pte_addr, &pte,
				  sizeof(pte)) < sizeof(pte))
		return 0;

	if (!(pte & 0x1))
		return 0;

	return ((pte & PAGE_MASK) + (vaddr & ~PAGE_MASK));
}

int realmode_map_memory(struct vcpu_hw_context *context, virtual_addr_t vaddr,
			physical_addr_t paddr, size_t size)
{
	union page32 pde, pte;
	union page32 *pde_addr;
	physical_addr_t tpaddr, pte_addr;
	virtual_addr_t tvaddr;
	u32 index, boffs;

	pde_addr = &context->shadow32_pgt[((vaddr >> 20) & 0xffc)];
	pde = *pde_addr;

	if (!pde.present) {
		if (context->pgmap_free_cache) {
			index = context->pgmap_free_cache;
			context->pgmap_free_cache = 0;
		} else {
			boffs = bitmap_find_free_region(context->shadow32_pg_map,
							NR_32BIT_PGLIST_PAGES, 0);
			index = boffs;
			context->pgmap_free_cache = boffs+1;
		}

		pde_addr->present = 1;
		pde_addr->rw = 1;
		tvaddr = (virtual_addr_t)(((virtual_addr_t)context->shadow32_pg_list) + (index * PAGE_SIZE));
		if (vmm_host_va2pa(tvaddr, &tpaddr) != VMM_OK)
			vmm_panic("%s: Failed to map vaddr to paddr for pde.\n",
				  __func__);
		pde_addr->paddr = (tpaddr >> PAGE_SHIFT);
	}

	pte_addr = ((pde_addr->paddr << PAGE_SHIFT) + ((vaddr >> 10) & 0xffc));
	if (vmm_host_memory_read(pte_addr, (void *)&pte, sizeof(pte)) < sizeof(pte))
		return VMM_EFAIL;

	if (pte.present)
		return VMM_EFAIL;

	pte.present = 1;
	pte.rw = 1;
	pte.paddr = (paddr >> PAGE_SHIFT);

	if (vmm_host_memory_write(pte_addr, (void *)&pte, sizeof(pte)) < sizeof(pte))
		return VMM_EFAIL;

	return VMM_OK;
}

int realmode_unmap_memory(struct vcpu_hw_context *context, virtual_addr_t vaddr,
			  size_t size)
{
	return VMM_OK;
}

/**
 * \brief Initiate a guest halt.
 *
 * This function is to be used by the vCPU which is
 * currently active and running. Since that vCPU
 * cannot destroy itself and associated guest,
 * it gets itself out of execution and tells
 * VMM by special opcode that it want to shutdown.
 *
 * @param guest
 * The guest that needs to be shutdown in emergency.
 */
void arch_guest_halt(struct vmm_guest *guest)
{
	__asm__ __volatile__("movq %0, %%rdi\n"
			     "movq %1, %%rsi\n"
			     "int $0x80\n"
			     ::"ri"(GUEST_HALT_SW_CODE), "r"(guest)
			     :"rdi","rsi");
}
