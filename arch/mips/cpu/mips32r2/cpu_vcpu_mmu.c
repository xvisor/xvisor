/**
 * Copyright (c) 2010-2011 Himanshu Chauhan.
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
 * @file cpu_vcpu_mmu.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief MMU handling functions related to VCPU.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_regs.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <cpu_asm_macros.h>
#include <cpu_vcpu_mmu.h>
#include <vmm_guest_aspace.h>

int do_vcpu_tlbmiss(vmm_user_regs_t *uregs)
{
	u32 badvaddr = read_c0_badvaddr();
	vmm_vcpu_t *current_vcpu;
	int counter = 0;
	mips32_tlb_entry_t *c_tlbe;

	current_vcpu = vmm_scheduler_current_vcpu();
	for (counter = 0; counter < 2 * CPU_TLB_COUNT; counter++) {
		c_tlbe = &mips_sregs(current_vcpu)->shadow_tlb_entries[counter];
		if (TBE_PGMSKD_VPN2(c_tlbe) ==
		    (badvaddr & ~c_tlbe->page_mask)) {
			mips_fill_tlb_entry(c_tlbe, -1);
			return 0;
		} else {
			vmm_panic("No TLB entry in shadow."
				  " Send fault to guest.\n");
		}
	}

	return VMM_EFAIL;
}

u32 mips_probe_vcpu_tlb(vmm_vcpu_t *vcpu, vmm_user_regs_t *uregs)
{
	u32 guest_cp0_index = (0x01UL << 31);
	u32 tlb_counter;
	mips32_tlb_entry_t *c_tlb_entry, *tempe, t_tlb_entry;
	mips32_entryhi_t g_probed_ehi;

	g_probed_ehi._entryhi = mips_sregs(vcpu)->cp0_regs[CP0_ENTRYHI_IDX];

	for (tlb_counter = 0; tlb_counter < CPU_TLB_COUNT; tlb_counter++) {
		c_tlb_entry = &mips_sregs(vcpu)->hw_tlb_entries[tlb_counter];

		t_tlb_entry.page_mask = c_tlb_entry->page_mask;
		t_tlb_entry.entryhi._entryhi = g_probed_ehi._entryhi;
		tempe = &t_tlb_entry;

		if (TBE_PGMSKD_VPN2(c_tlb_entry) == TBE_PGMSKD_VPN2(tempe)
		    && (TBE_ELO_VALID(c_tlb_entry, entrylo0)
			|| TBE_ELO_VALID(c_tlb_entry, entrylo1))
		    && (TBE_ASID(c_tlb_entry) == TBE_ASID(tempe)
			|| TBE_ELO_GLOBAL(c_tlb_entry, entrylo0)
			|| TBE_ELO_GLOBAL(c_tlb_entry, entrylo1))) {
			guest_cp0_index = 0;
			guest_cp0_index = tlb_counter;
			break;
		}
	}

	mips_sregs(vcpu)->cp0_regs[CP0_INDEX_IDX] = guest_cp0_index;

	return VMM_OK;
}

u32 mips_read_vcpu_tlb(vmm_vcpu_t *vcpu, vmm_user_regs_t *uregs)
{
	return VMM_OK;
}

static u32 mips_vcpu_map_guest_to_host(vmm_vcpu_t *vcpu,
				       mips32_tlb_entry_t *gtlbe)
{
	vmm_guest_t *guest = NULL;
	vmm_region_t *guest_region = NULL;
	physical_addr_t gphys_addr = 0, hphys_addr = 0, gphys_addr2map = 0, gphys_offset = 0;
	physical_addr_t hphys_addr2map = 0;
	int do_map = 0;

	guest = vcpu->guest;

	if (vcpu->is_normal) {
		/*
		 * Only if the guest is making a valid tlb entry, try to map the
		 * gphys to hphys
		 */
		if (TBE_ELO_VALID(gtlbe, entrylo0)) {
			gphys_addr2map = (gtlbe->entrylo0._s_entrylo.pfn \
					  << PAGE_SHIFT);
			guest_region = \
			vmm_guest_find_region(guest, gphys_addr2map, TRUE);
			if (!guest_region) {
				TBE_ELO_INVALIDATE(gtlbe, entrylo0);
				return VMM_EFAIL;
			}

			gphys_addr = guest_region->gphys_addr;
			hphys_addr = guest_region->hphys_addr;
			if ((gphys_offset = gphys_addr - gphys_addr2map) >= 0) {
				hphys_addr2map = hphys_addr + gphys_offset;
				gtlbe->entrylo0._s_entrylo.pfn =
					(hphys_addr2map >> PAGE_SHIFT);
				/*
				 * We can keep the valid and other bits same
				 * for now.
				 */
				do_map = 1;
			}
		}

		if (TBE_ELO_VALID(gtlbe, entrylo1)) {
			gphys_addr = (gtlbe->entrylo1._s_entrylo.pfn \
				      << PAGE_SHIFT);
			guest_region = \
			vmm_guest_find_region(guest, gphys_addr, TRUE);
			if (!guest_region) {
				TBE_ELO_INVALIDATE(gtlbe, entrylo1);
				return VMM_EFAIL;
			}

			gphys_addr = guest_region->gphys_addr;
			hphys_addr = guest_region->hphys_addr;
			if ((gphys_offset = gphys_addr - gphys_addr2map) >= 0) {
				hphys_addr2map = hphys_addr + gphys_offset;
				gtlbe->entrylo0._s_entrylo.pfn =
					(hphys_addr2map >> PAGE_SHIFT);
				/*
				 * We can keep the valid and other bits same
				 * for now.
				 */
				do_map = 1;
			}
		}
	}

	if (do_map) {
		/* Program a random TLB for guest */
		mips_fill_tlb_entry(gtlbe, -1);
	}

	return VMM_OK;
}

u32 mips_write_vcpu_tlbi(vmm_vcpu_t *vcpu, vmm_user_regs_t *uregs)
{
	mips32_tlb_entry_t *entry2prgm;
	u32 tlb_index = mips_sregs(vcpu)->cp0_regs[CP0_INDEX_IDX];

	/*
	 * TODO: Release 2 of MIPS32 checks and alerts for the duplicate
	 * entries in TLB. We need to implement this.
	 */
	/*
	 * Copy the entryhi, lo0, lo1 and page mask to hw_tlb_entries.
	 * These are THE entries that the guest created.
	 */
	if (tlb_index >=0 && tlb_index < CPU_TLB_COUNT) {
		entry2prgm = &mips_sregs(vcpu)->hw_tlb_entries[tlb_index];
		entry2prgm->entryhi._entryhi =
			mips_sregs(vcpu)->cp0_regs[CP0_ENTRYHI_IDX];
		entry2prgm->entrylo0._entrylo =
			mips_sregs(vcpu)->cp0_regs[CP0_ENTRYLO0_IDX];
		entry2prgm->entrylo1._entrylo =
			mips_sregs(vcpu)->cp0_regs[CP0_ENTRYLO1_IDX];
		entry2prgm->page_mask =
			mips_sregs(vcpu)->cp0_regs[CP0_PAGEMASK_IDX];

		return mips_vcpu_map_guest_to_host(vcpu, entry2prgm);
	}

	return VMM_OK;
}

u32 mips_write_vcpu_tlbr(vmm_vcpu_t *vcpu, vmm_user_regs_t *uregs)
{
	return VMM_OK;
}
