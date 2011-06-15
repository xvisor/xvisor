/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file cpu_vcpu_cp15.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Source of VCPU cp15 emulation
 */

#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vmm_scheduler.h>
#include <vmm_guest_aspace.h>
#include <cpu_mmu.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_emulate.h>
#include <cpu_vcpu_cp15.h>

int cpu_vcpu_cp15_mem_read(vmm_vcpu_t * vcpu, 
			   virtual_addr_t addr, 
			   void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 vind;
	cpu_page_t pg;
	if ((addr & ~(sizeof(vcpu->sregs.cp15.ovect) - 1)) == 
					vcpu->sregs.cp15.ovect_base) {
		vind = addr & (sizeof(vcpu->sregs.cp15.ovect) - 1);
		switch (dst_len) {
		case 4:
			vind &= ~(0x4 - 1);
			vind /= 0x4;
			*((u32 *)dst) = vcpu->sregs.cp15.ovect[vind];
			break;
		case 2:
			vind &= ~(0x2 - 1);
			vind /= 0x2;
			*((u16 *)dst) = ((u16 *)vcpu->sregs.cp15.ovect)[vind];
			break;
		case 1:
			*((u8 *)dst) = ((u8 *)vcpu->sregs.cp15.ovect)[vind];
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	} else {
		if (!(rc = cpu_mmu_get_page(vcpu->sregs.cp15.l1, addr, &pg))) {
			switch(pg.ap) {
			case TTBL_AP_SR_U:
			case TTBL_AP_SRW_U:
				rc = vmm_devemu_emulate_read(vcpu->guest, 
							     pg.pa, 
							     dst, dst_len);
				break;
			case TTBL_AP_SRW_UR:
			case TTBL_AP_SRW_URW:
				switch (dst_len) {
				case 4:
					*((u32 *)dst) = *((u32 *)addr);
					break;
				case 2:
					*((u16 *)dst) = *((u16 *)addr);
					break;
				case 1:
					*((u8 *)dst) = *((u8 *)addr);
					break;
				default:
					rc = VMM_EFAIL;
					break;
				};
				break;
			default:
				rc = VMM_EFAIL;
				break;
			};
		}
	}
	return rc;
}

int cpu_vcpu_cp15_mem_write(vmm_vcpu_t * vcpu, 
			    virtual_addr_t addr, 
			    void *src, u32 src_len)
{
	int rc = VMM_OK;
	u32 vind;
	cpu_page_t pg;
	if ((addr & ~(sizeof(vcpu->sregs.cp15.ovect) - 1)) == 
					vcpu->sregs.cp15.ovect_base) {
		vind = addr & (sizeof(vcpu->sregs.cp15.ovect) - 1);
		switch (src_len) {
		case 4:
			vind &= ~(0x4 - 1);
			vind /= 0x4;
			vcpu->sregs.cp15.ovect[vind] = *((u32 *)src);
			break;
		case 2:
			vind &= ~(0x2 - 1);
			vind /= 0x2;
			((u16 *)vcpu->sregs.cp15.ovect)[vind] = *((u16 *)src);
			break;
		case 1:
			((u8 *)vcpu->sregs.cp15.ovect)[vind] = *((u8 *)src);
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	} else {
		if (!(rc = cpu_mmu_get_page(vcpu->sregs.cp15.l1, addr, &pg))) {
			switch(pg.ap) {
			case TTBL_AP_SRW_U:
				rc = vmm_devemu_emulate_write(vcpu->guest, 
							      pg.pa, 
							      src, src_len);
				break;
			case TTBL_AP_SRW_URW:
				switch (src_len) {
				case 4:
					*((u32 *)addr) = *((u32 *)src);
					break;
				case 2:
					*((u16 *)addr) = *((u16 *)src);
					break;
				case 1:
					*((u8 *)addr) = *((u8 *)src);
					break;
				default:
					rc = VMM_EFAIL;
					break;
				};
				break;
			default:
				rc = VMM_EFAIL;
				break;
			};
		}
	}
	return rc;
}

/* FIXME: */
bool cpu_vcpu_cp15_read(vmm_vcpu_t * vcpu, 
			u32 opc1, u32 opc2, u32 CRm, 
			u32 *data)
{
	return TRUE;
}

/* FIXME: */
bool cpu_vcpu_cp15_write(vmm_vcpu_t * vcpu, 
			 u32 opc1, u32 opc2, u32 CRm, 
			 u32 data)
{
	return TRUE;
}

int cpu_vcpu_cp15_trans_fault(vmm_vcpu_t * vcpu, 
			      vmm_user_regs_t * regs, 
			      u32 fsr, u32 far, u32 page, u32 xn)
{
	int rc;
	u32 victim;
	vmm_guest_region_t *reg;
	cpu_page_t *p;

	/* Find out next victim page from shadow TLB */
	victim = vcpu->sregs.cp15.vtlb.victim;
	p = &vcpu->sregs.cp15.vtlb.page[victim];
	if (vcpu->sregs.cp15.vtlb.valid[victim]) {
		/* Remove valid victim page from L1 Page Table */
		if ((rc = cpu_mmu_unmap_page(vcpu->sregs.cp15.l1, p))) {
			return rc;
		}
		vcpu->sregs.cp15.vtlb.valid[victim] = 0;
	}

	/* Get the required page for vcpu */
	if (vcpu->sregs.cp15.c1_sctlr & SCTLR_M_MASK) {
		/* FIXME: MMU enabled for VCPU */
	} else {
		/* MMU disabled for VCPU */
		reg = vmm_guest_aspace_getregion(vcpu->guest, far);
		if (!reg) {
			return VMM_EFAIL;
		}
		p->pa = reg->hphys_addr + (far - reg->gphys_addr);
		p->va = far;
		p->sz = reg->phys_size - (far - reg->gphys_addr);
		if (TTBL_L1TBL_SECTION_PAGE_SIZE <= p->sz) {
			p->sz = TTBL_L1TBL_SECTION_PAGE_SIZE;
		} else if (TTBL_L2TBL_LARGE_PAGE_SIZE <= p->sz) {
			p->sz = TTBL_L2TBL_LARGE_PAGE_SIZE;
		} else {
			p->sz = TTBL_L2TBL_SMALL_PAGE_SIZE;
		}
		p->imp = 0;
		p->dom = TTBL_L1TBL_TTE_DOM_VCPU_NOMMU;
		if (reg->is_virtual) {
			p->ap = TTBL_AP_SRW_U;
		} else {
			p->ap = TTBL_AP_SRW_URW;
		}
		p->xn = 0;
		p->c = 0;
		p->b = 0;
	}

	/* Add victim page to L1 page table */
	if ((rc = cpu_mmu_map_page(vcpu->sregs.cp15.l1, p))) {
		return rc;
	}

	/* Mark current victim as valid and 
	 * point to next victim page in shadow TLB */
	vcpu->sregs.cp15.vtlb.valid[victim] = 1;
	victim = (victim + 1) % vcpu->sregs.cp15.vtlb.count;
	vcpu->sregs.cp15.vtlb.victim = victim;

	return VMM_OK;
}

/* FIXME: */
int cpu_vcpu_cp15_access_fault(vmm_vcpu_t * vcpu, 
			       vmm_user_regs_t * regs, 
			       u32 fsr, u32 far, u32 page, u32 xn)
{
	return VMM_OK;
}

/* FIXME: */
int cpu_vcpu_cp15_domain_fault(vmm_vcpu_t * vcpu, 
			       vmm_user_regs_t * regs, 
			       u32 fsr, u32 far, u32 page, u32 xn)
{
	return VMM_OK;
}

int cpu_vcpu_cp15_perm_fault(vmm_vcpu_t * vcpu, 
			     vmm_user_regs_t * regs, 
			     u32 fsr, u32 far, u32 page, u32 xn)
{
	if ((vcpu->sregs.cpsr & CPSR_MODE_MASK) != CPSR_MODE_USER) {
		return cpu_vcpu_emulate_inst(vcpu, regs, FALSE);
	} else {
		/* FIXME: Permission fault to VCPU */
	}

	return VMM_OK;
}

int cpu_vcpu_cp15_ifault(u32 ifsr, u32 ifar, vmm_vcpu_t * vcpu, 
						vmm_user_regs_t * regs)
{
	int rc = VMM_EFAIL;
	u32 fs;

	if (!vcpu) {
		return rc;
	}
	if (!vcpu->guest) {
		return rc;
	}

	fs = (ifsr & IFSR_FS4_MASK) >> IFSR_FS4_SHIFT;
	fs = (fs << 4) | (ifsr & IFSR_FS_MASK);

	switch(fs) {
	case IFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_1:
	case IFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_2:
		break;
	case IFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_1:
	case IFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_2:
		break;
	case IFSR_FS_TRANS_FAULT_SECTION:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, ifsr, ifar, 0, 0);
		break;
	case IFSR_FS_TRANS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, ifsr, ifar, 1, 0);
		break;
	case IFSR_FS_ACCESS_FAULT_SECTION:
		rc = cpu_vcpu_cp15_access_fault(vcpu, regs, ifsr, ifar, 0, 0);
		break;
	case IFSR_FS_ACCESS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_access_fault(vcpu, regs, ifsr, ifar, 1, 0);
		break;
	case IFSR_FS_DOMAIN_FAULT_SECTION:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, regs, ifsr, ifar, 0, 0);
		break;
	case IFSR_FS_DOMAIN_FAULT_PAGE:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, regs, ifsr, ifar, 1, 0);
		break;
	case IFSR_FS_PERM_FAULT_SECTION:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, regs, ifsr, ifar, 0, 0);
		break;
	case IFSR_FS_PERM_FAULT_PAGE:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, regs, ifsr, ifar, 1, 0);
		break;
	case IFSR_FS_DEBUG_EVENT:
	case IFSR_FS_SYNC_EXT_ABORT:
	case IFSR_FS_IMP_VALID_LOCKDOWN:
	case IFSR_FS_IMP_VALID_COPROC_ABORT:
	case IFSR_FS_MEM_ACCESS_SYNC_PARITY_ERROR:
		break;
	default:
		break; 
	};

	return rc;
}

int cpu_vcpu_cp15_dfault(u32 dfsr, u32 dfar, 
			 vmm_vcpu_t * vcpu, 
			 vmm_user_regs_t * regs)
{
	int rc = VMM_EFAIL;
	u32 fs;

	if (!vcpu) {
		return rc;
	}
	if (!vcpu->guest) {
		return rc;
	}

	fs = (dfsr & DFSR_FS4_MASK) >> DFSR_FS4_SHIFT;
	fs = (fs << 4) | (dfsr & DFSR_FS_MASK);

	switch(fs) {
	case DFSR_FS_ALIGN_FAULT:
		break;
	case DFSR_FS_ICACHE_MAINT_FAULT:
		break;
	case DFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_1:
	case DFSR_FS_TTBL_WALK_SYNC_EXT_ABORT_2:
		break;
	case DFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_1:
	case DFSR_FS_TTBL_WALK_SYNC_PARITY_ERROR_2:
		break;
	case DFSR_FS_TRANS_FAULT_SECTION:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, dfsr, dfar, 0, 1);
		break;
	case DFSR_FS_TRANS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_trans_fault(vcpu, regs, dfsr, dfar, 1, 1);
		break;
	case DFSR_FS_ACCESS_FAULT_SECTION:
		rc = cpu_vcpu_cp15_access_fault(vcpu, regs, dfsr, dfar, 0, 1);
		break;
	case DFSR_FS_ACCESS_FAULT_PAGE:
		rc = cpu_vcpu_cp15_access_fault(vcpu, regs, dfsr, dfar, 1, 1);
		break;
	case DFSR_FS_DOMAIN_FAULT_SECTION:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, regs, dfsr, dfar, 0, 1);
		break;
	case DFSR_FS_DOMAIN_FAULT_PAGE:
		rc = cpu_vcpu_cp15_domain_fault(vcpu, regs, dfsr, dfar, 1, 1);
		break;
	case DFSR_FS_PERM_FAULT_SECTION:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, regs, dfsr, dfar, 0, 1);
		break;
	case DFSR_FS_PERM_FAULT_PAGE:
		rc = cpu_vcpu_cp15_perm_fault(vcpu, regs, dfsr, dfar, 1, 1);
		break;
	case DFSR_FS_DEBUG_EVENT:
	case DFSR_FS_SYNC_EXT_ABORT:
	case DFSR_FS_IMP_VALID_LOCKDOWN:
	case DFSR_FS_IMP_VALID_COPROC_ABORT:
	case DFSR_FS_MEM_ACCESS_SYNC_PARITY_ERROR:
	case DFSR_FS_ASYNC_EXT_ABORT:
	case DFSR_FS_MEM_ACCESS_ASYNC_PARITY_ERROR:
		break;
	default:
		break;
	};

	return rc;
}

virtual_addr_t cpu_vcpu_cp15_vector_addr(vmm_vcpu_t * vcpu, u32 irq_no)
{
	virtual_addr_t vaddr;
	irq_no = irq_no % CPU_IRQ_NR;

	if (vcpu->sregs.cp15.c1_sctlr & SCTLR_V_MASK) {
		vaddr = CPU_IRQ_HIGHVEC_BASE;
	} else {
		vaddr = CPU_IRQ_LOWVEC_BASE;
	}

	if (vcpu->sregs.cp15.ovect_base == vaddr) {
		/* FIXME: We assume that guest will use 
		 * LDR PC, [PC, #xx] as first instruction of irq handler */
		vaddr = vcpu->sregs.cp15.ovect[irq_no + 8];
	} else {
		vaddr += 4*irq_no;
	}

	return vaddr;
}

void cpu_vcpu_cp15_sync_cpsr(vmm_vcpu_t * vcpu)
{
	vmm_vcpu_t * cvcpu = vmm_scheduler_current_vcpu();
	vcpu->sregs.cp15.dacr &= 
			~(0x3 << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
	if ((vcpu->sregs.cpsr & CPSR_MODE_MASK) == CPSR_MODE_USER) {
		vcpu->sregs.cp15.dacr |= 
		(TTBL_DOM_NOACCESS << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
	} else {
		vcpu->sregs.cp15.dacr |= 
		(TTBL_DOM_CLIENT << (2 * TTBL_L1TBL_TTE_DOM_VCPU_SUPER));
	}
	if (cvcpu->num == vcpu->num) {
		cpu_mmu_chdacr(vcpu->sregs.cp15.dacr);
	}
}

void cpu_vcpu_cp15_context_switch(vmm_vcpu_t * tvcpu, 
				  vmm_vcpu_t * vcpu, 
				  vmm_user_regs_t * regs)
{
	cpu_mmu_chdacr(vcpu->sregs.cp15.dacr);
	cpu_mmu_chttbr(vcpu->sregs.cp15.l1);
}

static u32 cortexa9_cp15_c0_c1[8] =
{ 0x1031, 0x11, 0x000, 0, 0x00100103, 0x20000000, 0x01230000, 0x00002111 };

static u32 cortexa9_cp15_c0_c2[8] =
{ 0x00101111, 0x13112111, 0x21232041, 0x11112131, 0x00111142, 0, 0, 0 };

static u32 cortexa8_cp15_c0_c1[8] =
{ 0x1031, 0x11, 0x400, 0, 0x31100003, 0x20000000, 0x01202000, 0x11 };

static u32 cortexa8_cp15_c0_c2[8] =
{ 0x00101111, 0x12112111, 0x21232031, 0x11112131, 0x00111142, 0, 0, 0 };

int cpu_vcpu_cp15_init(vmm_vcpu_t * vcpu, u32 cpuid)
{
	u32 vtlb_count;
	vmm_devtree_node_t *node;
	const char *attrval;

	vmm_memset(&vcpu->sregs.cp15, 0, sizeof(vcpu->sregs.cp15));

	vcpu->sregs.cp15.l1 = cpu_mmu_l1tbl_alloc();
	vcpu->sregs.cp15.dacr = 0x0;
	vcpu->sregs.cp15.dacr |= (TTBL_DOM_CLIENT << 
					(TTBL_L1TBL_TTE_DOM_VCPU_NOMMU * 2));
	vcpu->sregs.cp15.dacr |= (TTBL_DOM_NOACCESS << 
					(TTBL_L1TBL_TTE_DOM_VCPU_NOACCESS * 2));
	vcpu->sregs.cp15.dacr |= (TTBL_DOM_CLIENT << 
					(TTBL_L1TBL_TTE_DOM_VCPU_SUPER * 2));
	vcpu->sregs.cp15.dacr |= (TTBL_DOM_CLIENT << 
					(TTBL_L1TBL_TTE_DOM_VCPU_USER * 2));

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!node) {
		return VMM_EFAIL;
	}
	attrval = vmm_devtree_attrval(node, MMU_TLBENT_PER_VCPU_ATTR_NAME);
	if (!attrval) {
		return VMM_EFAIL;
	}
	vtlb_count = *((u32 *)attrval);
	vcpu->sregs.cp15.vtlb.count = vtlb_count;
	vcpu->sregs.cp15.vtlb.valid = vmm_malloc(vtlb_count);
	vmm_memset(vcpu->sregs.cp15.vtlb.valid, 0, vtlb_count);
	vcpu->sregs.cp15.vtlb.asid = vmm_malloc(vtlb_count);
	vmm_memset(vcpu->sregs.cp15.vtlb.asid, 0, vtlb_count);
	vcpu->sregs.cp15.vtlb.page = vmm_malloc(vtlb_count * 
							sizeof(cpu_page_t));
	vmm_memset(vcpu->sregs.cp15.vtlb.page, 0, vtlb_count * 
							sizeof(cpu_page_t));
	vcpu->sregs.cp15.vtlb.victim = 0;

	if (read_sctlr() & SCTLR_V_MASK) {
		vcpu->sregs.cp15.ovect_base = CPU_IRQ_HIGHVEC_BASE;
	} else {
		vcpu->sregs.cp15.ovect_base = CPU_IRQ_LOWVEC_BASE;
	}

	vcpu->sregs.cp15.c0_cpuid = cpuid;
	switch (cpuid) {
	case CPUID_CORTEXA8:
		vmm_memcpy(vcpu->sregs.cp15.c0_c1, cortexa8_cp15_c0_c1, 
							8 * sizeof(u32));
		vmm_memcpy(vcpu->sregs.cp15.c0_c2, cortexa8_cp15_c0_c2, 
							8 * sizeof(u32));
		vcpu->sregs.cp15.c0_cachetype = 0x82048004;
		vcpu->sregs.cp15.c0_clid = (1 << 27) | (2 << 24) | 3;
		vcpu->sregs.cp15.c0_ccsid[0] = 0xe007e01a; /* 16k L1 dcache. */
		vcpu->sregs.cp15.c0_ccsid[1] = 0x2007e01a; /* 16k L1 icache. */
		vcpu->sregs.cp15.c0_ccsid[2] = 0xf0000000; /* No L2 icache. */
		vcpu->sregs.cp15.c1_sctlr = 0x00c50078;
		break;
	case CPUID_CORTEXA9:
		vmm_memcpy(vcpu->sregs.cp15.c0_c1, cortexa9_cp15_c0_c1, 
							8 * sizeof(u32));
		vmm_memcpy(vcpu->sregs.cp15.c0_c2, cortexa9_cp15_c0_c2, 
							8 * sizeof(u32));
		vcpu->sregs.cp15.c0_cachetype = 0x80038003;
		vcpu->sregs.cp15.c0_clid = (1 << 27) | (1 << 24) | 3;
		vcpu->sregs.cp15.c0_ccsid[0] = 0xe00fe015; /* 16k L1 dcache. */
		vcpu->sregs.cp15.c0_ccsid[1] = 0x200fe015; /* 16k L1 icache. */
		vcpu->sregs.cp15.c1_sctlr = 0x00c50078;
		break;
	default:
		break;
	};

	return VMM_OK;
}

