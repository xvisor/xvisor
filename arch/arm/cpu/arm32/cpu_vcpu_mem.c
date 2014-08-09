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
 * @file cpu_vcpu_mem.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VCPU CP15 Emulation
 * @details This source file for VCPU memory read/write emulation
 */

#include <vmm_error.h>
#include <vmm_devemu.h>
#include <cpu_defines.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_cp15.h>
#include <cpu_vcpu_mem.h>

int cpu_vcpu_mem_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *dst, u32 dst_len, 
			bool force_unpriv)
{
	struct cpu_page pg;
	register int rc = VMM_OK;
	register u32 vind;
	register struct arm_priv_cp15 *cp15 = &arm_priv(vcpu)->cp15;
	register struct cpu_page *pgp;

	if ((addr & ~(TTBL_L2TBL_SMALL_PAGE_SIZE - 1)) == cp15->ovect_base) {
		if ((arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) ==
							CPSR_MODE_USER) {
			force_unpriv = TRUE;
		}
		if ((vind = cpu_vcpu_cp15_find_page(vcpu, addr,
						     CP15_ACCESS_READ,
						     force_unpriv, &pg))) {
			cpu_vcpu_cp15_assert_fault(vcpu, regs, addr,
					(vind >> 4), (vind & 0xF), 0, 1);
			return VMM_EFAIL;
		}
		vind = addr & (TTBL_L2TBL_SMALL_PAGE_SIZE - 1);
		switch (dst_len) {
		case 4:
			vind = vind >> 2;
			*((u32 *)dst) = arm_guest_priv(vcpu->guest)->ovect[vind];
			break;
		case 2:
			vind = vind >> 1;
			*((u16 *) dst) =
			    ((u16 *)arm_guest_priv(vcpu->guest)->ovect)[vind];
			break;
		case 1:
			*((u8 *) dst) =
			    ((u8 *)arm_guest_priv(vcpu->guest)->ovect)[vind];
			break;
		default:
			return VMM_EFAIL;
			break;
		};
	} else {
		if (cp15->virtio_active) {
			pgp = &cp15->virtio_page;
		} else {
			pgp = &pg;
			rc = cpu_mmu_get_page(cp15->l1, addr, &pg);
			if (rc == VMM_ENOTAVAIL) {
				if (pgp->va) {
					rc = cpu_vcpu_cp15_trans_fault(vcpu, regs,
						addr, DFSR_FS_TRANS_FAULT_PAGE,
						0, 0, 1, force_unpriv);
				} else {
					rc = cpu_vcpu_cp15_trans_fault(vcpu, regs,
						addr, DFSR_FS_TRANS_FAULT_SECTION,
						0, 0, 1, force_unpriv);
				}
				if (!rc) {
					rc = cpu_mmu_get_page(cp15->l1, addr, pgp);
				}
			}
			if (rc) {
				cpu_vcpu_halt(vcpu, regs);
				return rc;
			}
		}
		switch (pgp->ap) {
#if !defined(CONFIG_ARMV5)
		case TTBL_AP_SR_U:
#endif
		case TTBL_AP_SRW_U:
			return vmm_devemu_emulate_read(vcpu,
						       (addr - pgp->va) +
						       pgp->pa, dst, dst_len,
						       VMM_DEVEMU_NATIVE_ENDIAN);
			break;
		case TTBL_AP_SRW_UR:
		case TTBL_AP_SRW_URW:
			switch (dst_len) {
			case 4:
				*((u32 *) dst) = *((u32 *)addr);
				break;
			case 2:
				*((u16 *) dst) = *((u16 *)addr);
				break;
			case 1:
				*((u8 *) dst) = *((u8 *)addr);
				break;
			default:
				if (dst_len < 4) {
					return VMM_EFAIL;
				}
				vind = dst_len >> 2;
				while (vind) {
					((u32 *) dst)[vind-1] =
					    ((u32 *) addr)[vind-1];
					vind--;
				}
				break;
			};
			break;
		default:
			/* Remove fault address from VTLB and restart.
			 * Doing this will force us to do TTBL walk If MMU 
			 * is enabled then appropriate fault will be generated 
			 */
			cpu_vcpu_cp15_vtlb_flush_va(cp15, addr);
			return VMM_EFAIL;
			break;
		};
	}
	return VMM_OK;
}

int cpu_vcpu_mem_write(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *src, u32 src_len,
			bool force_unpriv)
{
	struct cpu_page pg;
	register int rc = VMM_OK;
	register u32 vind;
	register struct arm_priv_cp15 *cp15 = &arm_priv(vcpu)->cp15;
	register struct cpu_page *pgp;

	if ((addr & ~(TTBL_L2TBL_SMALL_PAGE_SIZE - 1)) == cp15->ovect_base) {
		if ((arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) ==
							CPSR_MODE_USER) {
			force_unpriv = TRUE;
		}
		if ((vind = cpu_vcpu_cp15_find_page(vcpu, addr,
						     CP15_ACCESS_WRITE,
						     force_unpriv, &pg))) {
			cpu_vcpu_cp15_assert_fault(vcpu, regs, addr,
						   (vind >> 4), (vind & 0xF),
						   1, 1);
			return VMM_EFAIL;
		}
		vind = addr & (TTBL_L2TBL_SMALL_PAGE_SIZE - 1);
		switch (src_len) {
		case 4:
			vind = vind >> 2;
			arm_guest_priv(vcpu->guest)->ovect[vind] =
							*((u32 *) src);
			break;
		case 2:
			vind = vind >> 1;
			((u16 *)arm_guest_priv(vcpu->guest)->ovect)[vind] =
			    *((u16 *) src);
			break;
		case 1:
			((u8 *)arm_guest_priv(vcpu->guest)->ovect)[vind] =
			    *((u8 *) src);
			break;
		default:
			return VMM_EFAIL;
			break;
		};
	} else {
		if (cp15->virtio_active) {
			pgp = &cp15->virtio_page;
		} else {
			pgp = &pg;
			rc = cpu_mmu_get_page(cp15->l1, addr, &pg);
			if (rc == VMM_ENOTAVAIL) {
				if (pgp->va) {
					rc = cpu_vcpu_cp15_trans_fault(vcpu, regs,
						addr, DFSR_FS_TRANS_FAULT_PAGE,
						0, 1, 1, force_unpriv);
				} else {
					rc = cpu_vcpu_cp15_trans_fault(vcpu, regs,
						addr, DFSR_FS_TRANS_FAULT_SECTION,
						0, 1, 1, force_unpriv);
				}
				if (!rc) {
					rc = cpu_mmu_get_page(cp15->l1, addr, pgp);
				}
			}
			if (rc) {
				cpu_vcpu_halt(vcpu, regs);
				return rc;
			}
		}
		switch (pgp->ap) {
		case TTBL_AP_SRW_U:
			return vmm_devemu_emulate_write(vcpu,
							(addr - pgp->va) +
							pgp->pa, src, src_len,
							VMM_DEVEMU_NATIVE_ENDIAN);
			break;
		case TTBL_AP_SRW_URW:
			switch (src_len) {
			case 4:
				*((u32 *) addr) = *((u32 *) src);
				break;
			case 2:
				*((u16 *) addr) = *((u16 *) src);
				break;
			case 1:
				*((u8 *) addr) = *((u8 *) src);
				break;
			default:
				if (src_len < 4) {
					return VMM_EFAIL;
				}
				vind = src_len >> 2;
				while (vind) {
					((u32 *) addr)[vind-1] =
					    ((u32 *) src)[vind-1];
					vind--;
				}
				break;
			};
			break;
		default:
			/* Remove fault address from VTLB and restart.
			 * Doing this will force us to do TTBL walk If MMU 
			 * is enabled then appropriate fault will be generated 
			 */
			cpu_vcpu_cp15_vtlb_flush_va(cp15, addr);
			return VMM_EFAIL;
			break;
		};
	}
	return VMM_OK;
}

int cpu_vcpu_mem_readex(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *dst, u32 dst_len, 
			bool force_unpriv)
{
	register int ecode;
	struct cpu_page pg;
	u32 vind, data;
	struct arm_priv_cp15 *cp15 = &arm_priv(vcpu)->cp15;

	if ((addr & ~(TTBL_L2TBL_SMALL_PAGE_SIZE - 1)) == cp15->ovect_base) {
		if ((arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) ==
							CPSR_MODE_USER) {
			force_unpriv = TRUE;
		}
		if ((ecode = cpu_vcpu_cp15_find_page(vcpu, addr,
						CP15_ACCESS_READ,
						force_unpriv, &pg))) {
			cpu_vcpu_cp15_assert_fault(vcpu, regs, addr,
					(ecode >> 4), (ecode & 0xF),
					0, 1);
			return VMM_EFAIL;
		}
		vind = (addr & (TTBL_L2TBL_SMALL_PAGE_SIZE - 1))/4;
		addr = (u32)(&(arm_guest_priv(vcpu->guest)->ovect[vind]));

		ldrex(addr, data);

		switch (dst_len) {
		case 1:
			*((u8 *)dst) = (u8)data;
			break;
		case 2:
			*((u16 *)dst) = (u16)data;
			break;
		case 4:
			*((u32 *)dst) = data;
			break;
		default:
			return VMM_EFAIL;
		};
	} else {
		/* We do not allow any faulting ldrex on non-ovect region */
		return VMM_EFAIL;
	}

	return VMM_OK;
}

int cpu_vcpu_mem_writeex(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *src, u32 src_len,
			bool force_unpriv)
{
	register int ecode;
	struct cpu_page pg;
	u32 vind, data;
	struct arm_priv_cp15 *cp15 = &arm_priv(vcpu)->cp15;

	if ((addr & ~(TTBL_L2TBL_SMALL_PAGE_SIZE - 1)) == cp15->ovect_base) {
		if ((arm_priv(vcpu)->cpsr & CPSR_MODE_MASK) ==
							CPSR_MODE_USER) {
			force_unpriv = TRUE;
		}
		if ((ecode = cpu_vcpu_cp15_find_page(vcpu, addr,
						CP15_ACCESS_READ,
						force_unpriv, &pg))) {
			cpu_vcpu_cp15_assert_fault(vcpu, regs, addr,
					(ecode >> 4), (ecode & 0xF),
					0, 1);
			return VMM_EFAIL;
		}
		vind = (addr & (TTBL_L2TBL_SMALL_PAGE_SIZE - 1))/4;
		addr = (u32)(&(arm_guest_priv(vcpu->guest)->ovect[vind]));

		switch (src_len) {
		case 1:
			data = *((u8 *)src);
			break;
		case 2:
			data = *((u16 *)src);
			break;
		case 4:
			data = *((u32 *)src);
			break;
		default:
			return VMM_EFAIL;
		};

		strex(addr, data, ecode);

		return (ecode == 0) ? VMM_OK : VMM_EFAIL;
	} else {
		/* We do not allow any faulting strex on non-ovect region */
		return VMM_EFAIL;
	}

	return VMM_OK;
}

