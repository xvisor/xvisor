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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source of VCPU helper functions
 */

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <vmm_guest_aspace.h>
#include <libs/stringlib.h>
#include <cpu_asm_macros.h>
#include <cpu_mmu.h>

extern char _stack_start;

#define VMM_REGION_TYPE_ROM	0
#define VMM_REGION_TYPE_RAM	1

static int map_guest_region(struct vmm_vcpu *vcpu, int region_type, int tlb_index)
{
	mips32_tlb_entry_t shadow_entry;
	physical_addr_t gphys;
	physical_addr_t hphys, paddr;
	virtual_addr_t vaddr2map;
	u32 gphys_size;
	struct vmm_region *region;
	struct vmm_guest *aguest = vcpu->guest;

	vaddr2map = (region_type == VMM_REGION_TYPE_ROM ? 0x3FC00000 : 0x0);
	paddr = (region_type == VMM_REGION_TYPE_ROM ? 0x1FC00000 : 0x0);

	/*
	 * Create the initial TLB entry mapping complete RAM promised
	 * to the guest. The idea is that guest vcpu shouldn't fault
	 * on this address.
	 */
	region = vmm_guest_find_region(aguest, paddr, TRUE);
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

	memcpy((void *)&mips_sregs(vcpu)->shadow_tlb_entries[tlb_index],
		   (void *)&shadow_entry, sizeof(mips32_tlb_entry_t));

	return VMM_OK;
}

static  __attribute__((unused)) int map_vcpu_ram(struct vmm_vcpu *vcpu)
{
	return map_guest_region(vcpu, VMM_REGION_TYPE_RAM, 1);
}

static  __attribute__((unused)) int map_vcpu_rom(struct vmm_vcpu *vcpu)
{
	return map_guest_region(vcpu, VMM_REGION_TYPE_ROM, 0);
}

int arch_guest_init(struct vmm_guest * guest)
{
	/* Don't have per guest arch context */
	return VMM_OK;
}

int arch_guest_deinit(struct vmm_guest * guest)
{
	/* Don't have per guest arch context */
	return VMM_OK;
}

int arch_vcpu_init(struct vmm_vcpu *vcpu)
{
	memset(mips_uregs(vcpu), 0, sizeof(arch_regs_t));

        if (!vcpu->is_normal) {
		/* For orphan vcpu */
                mips_uregs(vcpu)->cp0_epc = vcpu->start_pc;
                mips_uregs(vcpu)->regs[SP_IDX] = 
					vcpu->stack_va + vcpu->stack_sz - 4;
		mips_uregs(vcpu)->regs[S8_IDX] = mips_uregs(vcpu)->regs[SP_IDX];
		mips_uregs(vcpu)->cp0_status = read_c0_status();
		mips_uregs(vcpu)->cp0_entryhi = read_c0_entryhi();
        } else {
		/* For normal vcpu running guests */
		mips_sregs(vcpu)->cp0_regs[CP0_CAUSE_IDX] = 0x400;
		mips_sregs(vcpu)->cp0_regs[CP0_STATUS_IDX] = 0x40004;
		mips_uregs(vcpu)->cp0_status = read_c0_status() | (0x01UL << CP0_STATUS_UM_SHIFT);
		mips_uregs(vcpu)->cp0_entryhi = read_c0_entryhi();
		mips_uregs(vcpu)->cp0_entryhi &= ASID_MASK;
		mips_uregs(vcpu)->cp0_entryhi |= (0x2 << ASID_SHIFT);
		mips_uregs(vcpu)->cp0_epc = vcpu->start_pc;

		/* All guest run from 0 and fault */
		mips_sregs(vcpu)->cp0_regs[CP0_EPC_IDX] = vcpu->start_pc;
		/* Give guest the same CPU cap as we have */
		mips_sregs(vcpu)->cp0_regs[CP0_PRID_IDX] = read_c0_prid();
	}

	return VMM_OK;
}

int arch_vcpu_deinit(struct vmm_vcpu * vcpu)
{
	memset(mips_uregs(vcpu), 0, sizeof(arch_regs_t));

	return VMM_OK;
}

void arch_vcpu_switch(struct vmm_vcpu *tvcpu, 
		      struct vmm_vcpu *vcpu,
		      arch_regs_t *regs)
{
	if (tvcpu) {
		memcpy(mips_uregs(tvcpu), regs, sizeof(arch_regs_t));
	}

	if (vcpu) {
		if (!vcpu->is_normal) {
			mips_uregs(vcpu)->cp0_status = read_c0_status() & ~(0x01UL << CP0_STATUS_UM_SHIFT);
		} else {
			mips_uregs(vcpu)->cp0_status = read_c0_status() | (0x01UL << CP0_STATUS_UM_SHIFT);
		}

		memcpy(regs, mips_uregs(vcpu), sizeof(arch_regs_t));
	}
}

void arch_vcpu_regs_dump(struct vmm_vcpu *vcpu) 
{
}

void arch_vcpu_stat_dump(struct vmm_vcpu *vcpu) 
{
}
