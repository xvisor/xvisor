/**
 * Copyright (c) 2011 Himanshu Chauhan
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
 * @file cpu_vcpu_helper.c
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source of VCPU helper functions
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_guest.h>
#include <cpu_asm_macros.h>
#include <vmm_cpu.h>
#include <cpu_mmu.h>
#include <vmm_guest_aspace.h>

extern char _stack_start;

#define VMM_REGION_TYPE_ROM	0
#define VMM_REGION_TYPE_RAM	1

static int map_guest_region(vmm_vcpu_t *vcpu, int region_type, int tlb_index)
{
	mips32_tlb_entry_t shadow_entry;
	physical_addr_t gphys;
	physical_addr_t hphys, paddr;
	virtual_addr_t vaddr2map;
	u32 gphys_size;
	vmm_guest_region_t *region;
	vmm_guest_t *aguest = vcpu->guest;

	vaddr2map = (region_type == VMM_REGION_TYPE_ROM ? 0x3FC00000 : 0x0);
	paddr = (region_type == VMM_REGION_TYPE_ROM ? 0x1FC00000 : 0x0);

	/*
	 * Create the initial TLB entry mapping complete RAM promised
	 * to the guest. The idea is that guest vcpu shouldn't fault
	 * on this address.
	 */
	region = vmm_guest_aspace_getregion(aguest, paddr);
	if (region == NULL) {
		vmm_printf("Bummer!!! No guest region defined for VCPU RAM.\n");
		return VMM_EFAIL;
	}

	gphys = region->gphys_addr;
	hphys = region->hphys_addr;
	gphys_size = region->phys_size;

	switch (gphys_size) {
	case TLB_PAGE_SIZE_1K:
	case TLB_PAGE_SIZE_4K:
	case TLB_PAGE_SIZE_16K:
	case TLB_PAGE_SIZE_256K:
	case TLB_PAGE_SIZE_1M:
	case TLB_PAGE_SIZE_4M:
	case TLB_PAGE_SIZE_16M:
	case TLB_PAGE_SIZE_64M:
	case TLB_PAGE_SIZE_256M:
		gphys_size = gphys_size;
		shadow_entry.page_mask = ((gphys_size / 2) - 1);
		break;
	default:
		vmm_panic("Guest physical memory region should be same as page"
			  " sizes available for MIPS32.\n");
	}

	/* FIXME: Guest physical/virtual should be from DTS */
	shadow_entry.entryhi._s_entryhi.vpn2 = (vaddr2map >> VPN2_SHIFT);
	shadow_entry.entryhi._s_entryhi.asid = (u8)(2 << 6);
	shadow_entry.entryhi._s_entryhi.reserved = 0;
	shadow_entry.entryhi._s_entryhi.vpn2x = 0;

	shadow_entry.entrylo0._s_entrylo.global = 0;
	shadow_entry.entrylo0._s_entrylo.valid = 1;
	shadow_entry.entrylo0._s_entrylo.dirty = 1;
	shadow_entry.entrylo0._s_entrylo.cacheable = 1;
	shadow_entry.entrylo0._s_entrylo.pfn = (hphys >> PAGE_SHIFT);

	shadow_entry.entrylo1._s_entrylo.global = 0;
	shadow_entry.entrylo1._s_entrylo.valid = 0;
	shadow_entry.entrylo1._s_entrylo.dirty = 0;
	shadow_entry.entrylo1._s_entrylo.cacheable = 0;
	shadow_entry.entrylo1._s_entrylo.pfn = 0;

	vmm_memcpy((void *)&vcpu->sregs->shadow_tlb_entries[tlb_index],
		   (void *)&shadow_entry, sizeof(mips32_tlb_entry_t));

	return VMM_OK;
}

static int map_vcpu_ram(vmm_vcpu_t *vcpu)
{
	return map_guest_region(vcpu, VMM_REGION_TYPE_RAM, 1);
}

static int map_vcpu_rom(vmm_vcpu_t *vcpu)
{
	return map_guest_region(vcpu, VMM_REGION_TYPE_ROM, 0);
}

int vmm_vcpu_regs_init(vmm_vcpu_t *vcpu)
{
	vmm_memset(vcpu->uregs, 0, sizeof(vmm_user_regs_t));

        if (vcpu->guest == NULL) {
		/* For hypercore */
                vcpu->uregs->cp0_epc = vcpu->start_pc;
                vcpu->uregs->regs[SP_IDX] = (virtual_addr_t)&_stack_start;
		vcpu->uregs->regs[S8_IDX] = vcpu->uregs->regs[SP_IDX];
		vcpu->uregs->cp0_status = read_c0_status();
		vcpu->uregs->cp0_entryhi = read_c0_entryhi();
        } else {
		/* For vcpu running guests */
		vcpu->sregs->cp0_regs[CP0_CAUSE_IDX] = 0x400;
		vcpu->sregs->cp0_regs[CP0_STATUS_IDX] = 0x40004;
		vcpu->uregs->cp0_status = read_c0_status() | (0x01UL << CP0_STATUS_UM_SHIFT);
		vcpu->uregs->cp0_entryhi = read_c0_entryhi();
		vcpu->uregs->cp0_entryhi &= ASID_MASK;
		vcpu->uregs->cp0_entryhi |= (0x2 << ASID_SHIFT);
		vcpu->uregs->cp0_epc = vcpu->start_pc;

		/* All guest run from 0 and fault */
		vcpu->sregs->cp0_regs[CP0_EPC_IDX] = vcpu->start_pc;
		/* Give guest the same CPU cap as we have */
		vcpu->sregs->cp0_regs[CP0_PRID_IDX] = read_c0_prid();
		/*
		 * FIXME: Prepare the configuration registers as well. OS like
		 * Linux use them for setting up handlers etc.
		 */
		if (map_vcpu_ram(vcpu) != VMM_OK) {
			vmm_printf("(%s) Error: Failed to map guest's RAM in its address space!\n",
				   __FUNCTION__);
			return VMM_EFAIL;
		}

		if (map_vcpu_rom(vcpu) != VMM_OK) {
			vmm_printf("(%s) Error: Failed to map guest's ROM in its address space!\n",
				   __FUNCTION__);
			return VMM_EFAIL;
		}
	}

	return VMM_OK;
}

void vmm_vcpu_regs_switch(vmm_vcpu_t *tvcpu, vmm_vcpu_t *vcpu,
			  vmm_user_regs_t *regs)
{
	if (tvcpu) {
		vmm_memcpy(&tvcpu->uregs, regs, sizeof(vmm_user_regs_t));
	}

	if (vcpu) {
		if (vcpu->guest == NULL) {
			vcpu->uregs->cp0_status = read_c0_status() & ~(0x01UL << CP0_STATUS_UM_SHIFT);
		} else {
			vcpu->uregs->cp0_status = read_c0_status() | (0x01UL << CP0_STATUS_UM_SHIFT);
		}

		vmm_memcpy(regs, vcpu->uregs, sizeof(vmm_user_regs_t));
	}
}

void vmm_vcpu_regs_dump(vmm_vcpu_t *vcpu) 
{
}
