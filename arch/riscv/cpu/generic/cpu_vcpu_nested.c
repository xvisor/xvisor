/**
 * Copyright (c) 2022 Ventana Micro Systems Inc.
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
 * @file cpu_vcpu_nested.c
 * @author Anup Patel (apatel@ventanamicro.com)
 * @brief source of VCPU nested functions
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <generic_mmu.h>

#include <cpu_hwcap.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_nested.h>
#include <cpu_vcpu_trap.h>
#include <riscv_csr.h>

int cpu_vcpu_nested_init(struct vmm_vcpu *vcpu)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	npriv->pgtbl = mmu_pgtbl_alloc(MMU_STAGE2, -1);
	if (!npriv->pgtbl) {
		return VMM_ENOMEM;
	}

	return VMM_OK;
}

void cpu_vcpu_nested_reset(struct vmm_vcpu *vcpu)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	npriv->virt = FALSE;
#ifdef CONFIG_64BIT
	npriv->hstatus = HSTATUS_VSXL_RV64 << HSTATUS_VSXL_SHIFT;
#else
	npriv->hstatus = 0;
#endif
	npriv->hedeleg = 0;
	npriv->hideleg = 0;
	npriv->hvip = 0;
	npriv->hcounteren = 0;
	npriv->htimedelta = 0;
	npriv->htimedeltah = 0;
	npriv->htval = 0;
	npriv->htinst = 0;
	npriv->hgatp = 0;
	npriv->vsstatus = 0;
	npriv->vsie = 0;
	npriv->vstvec = 0;
	npriv->vsscratch = 0;
	npriv->vsepc = 0;
	npriv->vscause = 0;
	npriv->vstval = 0;
	npriv->vsatp = 0;

	npriv->hvictl = 0;
}

void cpu_vcpu_nested_deinit(struct vmm_vcpu *vcpu)
{
	mmu_pgtbl_free(riscv_nested_priv(vcpu)->pgtbl);
}

void cpu_vcpu_nested_dump_regs(struct vmm_chardev *cdev,
			       struct vmm_vcpu *vcpu)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, h))
		return;

	vmm_cprintf(cdev, "\n");
	vmm_cprintf(cdev, "    %s=%s\n",
		    "       virt", (npriv->virt) ? "on" : "off");
	vmm_cprintf(cdev, "\n");
#ifdef CONFIG_64BIT
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR"\n",
		    " htimedelta", npriv->htimedelta);
#else
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    " htimedelta", npriv->htimedelta,
		    "htimedeltah", npriv->htimedeltah);
#endif
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "    hstatus", npriv->hstatus, "      hgatp", npriv->hgatp);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "    hedeleg", npriv->hedeleg, "    hideleg", npriv->hideleg);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "       hvip", npriv->hvip, " hcounteren", npriv->hcounteren);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "      htval", npriv->htval, "     htinst", npriv->htinst);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "   vsstatus", npriv->vsstatus, "       vsie", npriv->vsie);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "      vsatp", npriv->vsatp, "     vstvec", npriv->vstvec);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "  vsscratch", npriv->vsscratch, "      vsepc", npriv->vsepc);
	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR" %s=0x%"PRIADDR"\n",
		    "    vscause", npriv->vscause, "     vstval", npriv->vstval);

	vmm_cprintf(cdev, "(V) %s=0x%"PRIADDR"\n",
		    "     hvictl", npriv->hvictl);
}

int cpu_vcpu_nested_smode_csr_rmw(struct vmm_vcpu *vcpu, arch_regs_t *regs,
			unsigned int csr_num, unsigned long *val,
			unsigned long new_val, unsigned long wr_mask)
{
	int csr_shift = 0;
	bool read_only = FALSE;
	unsigned long *csr, zero = 0, writeable_mask = 0;
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	/*
	 * These CSRs should never trap for virtual-HS/U modes because
	 * we only emulate these CSRs for virtual-VS/VU modes.
	 */
	if (!riscv_nested_virt(vcpu)) {
		return VMM_EINVALID;
	}

	/*
	 * Access of these CSRs from virtual-VU mode should be forwarded
	 * as illegal instruction trap to virtual-HS mode.
	 */
	if (!(regs->hstatus & HSTATUS_SPVP)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	switch (csr_num) {
	case CSR_SIE:
		if (npriv->hvictl & HVICTL_VTI) {
			return TRAP_RETURN_VIRTUAL_INSN;
		}
		csr = &npriv->vsie;
		writeable_mask = VSIE_WRITEABLE & (npriv->hideleg >> 1);
		break;
	case CSR_SIEH:
		if (npriv->hvictl & HVICTL_VTI) {
			return TRAP_RETURN_VIRTUAL_INSN;
		}
		csr = &zero;
		break;
	case CSR_SIP:
		if (npriv->hvictl & HVICTL_VTI) {
			return TRAP_RETURN_VIRTUAL_INSN;
		}
		csr = &npriv->hvip;
		csr_shift = 1;
		writeable_mask = HVIP_VSSIP & npriv->hideleg;
		break;
	case CSR_SIPH:
		if (npriv->hvictl & HVICTL_VTI) {
			return TRAP_RETURN_VIRTUAL_INSN;
		}
		csr = &zero;
		break;
	default:
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	if (val) {
		*val = (csr_shift < 0) ?
			(*csr) << -csr_shift : (*csr) >> csr_shift;
	}

	if (read_only) {
		return TRAP_RETURN_ILLEGAL_INSN;
	} else {
		writeable_mask = (csr_shift < 0) ?
				  writeable_mask >> -csr_shift :
				  writeable_mask << csr_shift;
		wr_mask = (csr_shift < 0) ?
			   wr_mask >> -csr_shift : wr_mask << csr_shift;
		new_val = (csr_shift < 0) ?
			   new_val >> -csr_shift : new_val << csr_shift;
		wr_mask &= writeable_mask;
		*csr = (*csr & ~wr_mask) | (new_val & wr_mask);
	}

	return VMM_OK;
}

int cpu_vcpu_nested_hext_csr_rmw(struct vmm_vcpu *vcpu, arch_regs_t *regs,
			unsigned int csr_num, unsigned long *val,
			unsigned long new_val, unsigned long wr_mask)
{
	int csr_shift = 0;
	bool read_only = FALSE;
	unsigned int csr_priv = (csr_num >> 8) & 0x3;
	unsigned long *csr, mode, zero = 0, writeable_mask = 0;
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	/*
	 * Trap from virtual-VS and virtual-VU modes should be forwarded
	 * to virtual-HS mode as a virtual instruction trap.
	 */
	if (riscv_nested_virt(vcpu)) {
		return (csr_priv == (PRV_S + 1)) ?
			TRAP_RETURN_VIRTUAL_INSN : TRAP_RETURN_ILLEGAL_INSN;
	}

	/*
	 * If H-extension is not available for VCPU then forward trap
	 * as illegal instruction trap to virtual-HS mode.
	 */
	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, h)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	/*
	 * H-extension CSRs not allowed in virtual-U mode so forward trap
	 * as illegal instruction trap to virtual-HS mode.
	 */
	if (!(regs->hstatus & HSTATUS_SPVP)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	switch (csr_num) {
	case CSR_HSTATUS:
		csr = &npriv->hstatus;
		writeable_mask = HSTATUS_VTSR | HSTATUS_VTW | HSTATUS_VTVM |
				 HSTATUS_HU | HSTATUS_SPVP | HSTATUS_SPV |
				 HSTATUS_GVA;
		if (wr_mask & HSTATUS_SPV) {
			/*
			 * Enable (or Disable) host SRET trapping for
			 * virtual-HS mode. This will be auto-disabled
			 * by cpu_vcpu_nested_set_virt() upon SRET trap
			 * from virtual-HS mode.
			 */
			regs->hstatus &= ~HSTATUS_VTSR;
			regs->hstatus |= (new_val & HSTATUS_SPV) ?
					HSTATUS_VTSR : 0;
		}
		break;
	case CSR_HEDELEG:
		csr = &npriv->hedeleg;
		writeable_mask = HEDELEG_WRITEABLE;
		break;
	case CSR_HIDELEG:
		csr = &npriv->hideleg;
		writeable_mask = HIDELEG_WRITEABLE;
		break;
	case CSR_HVIP:
		csr = &npriv->hvip;
		writeable_mask = HVIP_WRITEABLE;
		break;
	case CSR_HIE:
		csr = &npriv->vsie;
		csr_shift = -1;
		writeable_mask = HVIP_WRITEABLE;
		break;
	case CSR_HIP:
		csr = &npriv->hvip;
		writeable_mask = HVIP_VSSIP;
		break;
	case CSR_HGEIP:
		csr = &zero;
		read_only = TRUE;
		break;
	case CSR_HGEIE:
		csr = &zero;
		break;
	case CSR_HCOUNTEREN:
		csr = &npriv->hcounteren;
		writeable_mask = HCOUNTEREN_WRITEABLE;
		break;
	case CSR_HTIMEDELTA:
		csr = &npriv->htimedelta;
		writeable_mask = -1UL;
		break;
#ifndef CONFIG_64BIT
	case CSR_HTIMEDELTAH:
		csr = &npriv->htimedeltah;
		writeable_mask = -1UL;
		break;
#endif
	case CSR_HTVAL:
		csr = &npriv->htval;
		writeable_mask = -1UL;
		break;
	case CSR_HTINST:
		csr = &npriv->htinst;
		writeable_mask = -1UL;
		break;
	case CSR_HGATP:
		csr = &npriv->hgatp;
		writeable_mask = HGATP_MODE | HGATP_VMID | HGATP_PPN;
		if (wr_mask & HGATP_MODE) {
			mode = (new_val & HGATP_MODE) >> HGATP_MODE_SHIFT;
			switch (mode) {
			/*
			 * We (intentionally) support only Sv39x4 on RV64
			 * and Sv32x4 on RV32 for guest G-stage so that
			 * software page table walks on guest G-stage is
			 * faster.
			 */
#ifdef CONFIG_64BIT
			case HGATP_MODE_SV39X4:
				if (riscv_stage2_mode != HGATP_MODE_SV48X4 &&
				    riscv_stage2_mode != HGATP_MODE_SV39X4) {
					mode = HGATP_MODE_OFF;
				}
				break;
#else
			case HGATP_MODE_SV32X4:
				if (riscv_stage2_mode != HGATP_MODE_SV32X4) {
					mode = HGATP_MODE_OFF;
				}
				break;
#endif
			default:
				mode = HGATP_MODE_OFF;
				break;
			}
			new_val &= ~HGATP_MODE;
			new_val |= (mode << HGATP_MODE_SHIFT) & HGATP_MODE;
		}
		break;
	case CSR_VSSTATUS:
		csr = &npriv->vsstatus;
		writeable_mask = SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_UBE |
				 SSTATUS_SPP | SSTATUS_SUM | SSTATUS_MXR |
				 SSTATUS_FS | SSTATUS_UXL;
		break;
	case CSR_VSIP:
		csr = &npriv->hvip;
		csr_shift = 1;
		writeable_mask = HVIP_VSSIP & npriv->hideleg;
		break;
	case CSR_VSIE:
		csr = &npriv->vsie;
		writeable_mask = VSIE_WRITEABLE & (npriv->hideleg >> 1);
		break;
	case CSR_VSTVEC:
		csr = &npriv->vstvec;
		writeable_mask = -1UL;
		break;
	case CSR_VSSCRATCH:
		csr = &npriv->vsscratch;
		writeable_mask = -1UL;
		break;
	case CSR_VSEPC:
		csr = &npriv->vsepc;
		writeable_mask = -1UL;
		break;
	case CSR_VSCAUSE:
		csr = &npriv->vscause;
		writeable_mask = 0x1fUL;
		break;
	case CSR_VSTVAL:
		csr = &npriv->vstval;
		writeable_mask = -1UL;
		break;
	case CSR_VSATP:
		csr = &npriv->vsatp;
		writeable_mask = SATP_MODE | SATP_ASID | SATP_PPN;
		if (wr_mask & SATP_MODE) {
			mode = (new_val & SATP_MODE) >> SATP_MODE_SHIFT;
			switch (mode) {
#ifdef CONFIG_64BIT
			case SATP_MODE_SV48:
				if (riscv_stage1_mode != SATP_MODE_SV48) {
					mode = SATP_MODE_OFF;
				}
				break;
			case SATP_MODE_SV39:
				if (riscv_stage1_mode != SATP_MODE_SV48 &&
				    riscv_stage1_mode != SATP_MODE_SV39) {
					mode = SATP_MODE_OFF;
				}
				break;
#else
			case SATP_MODE_SV32:
				if (riscv_stage1_mode != SATP_MODE_SV32) {
					mode = SATP_MODE_OFF;
				}
				break;
#endif
			default:
				mode = SATP_MODE_OFF;
				break;
			}
			new_val &= ~SATP_MODE;
			new_val |= (mode << SATP_MODE_SHIFT) & SATP_MODE;
		}
		break;
	case CSR_HVICTL:
		csr = &npriv->hvictl;
		writeable_mask = HVICTL_VTI | HVICTL_IID |
				 HVICTL_IPRIOM | HVICTL_IPRIO;
		break;
	default:
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	if (val) {
		*val = (csr_shift < 0) ?
			(*csr) << -csr_shift : (*csr) >> csr_shift;
	}

	if (read_only) {
		return TRAP_RETURN_ILLEGAL_INSN;
	} else {
		writeable_mask = (csr_shift < 0) ?
				  writeable_mask >> -csr_shift :
				  writeable_mask << csr_shift;
		wr_mask = (csr_shift < 0) ?
			   wr_mask >> -csr_shift : wr_mask << csr_shift;
		new_val = (csr_shift < 0) ?
			   new_val >> -csr_shift : new_val << csr_shift;
		wr_mask &= writeable_mask;
		*csr = (*csr & ~wr_mask) | (new_val & wr_mask);
	}

	return VMM_OK;
}

int cpu_vcpu_nested_page_fault(struct vmm_vcpu *vcpu,
			       bool trap_from_smode,
			       const struct cpu_vcpu_trap *trap,
			       struct cpu_vcpu_trap *out_trap)
{
	/* TODO: */
	return VMM_OK;
}

void cpu_vcpu_nested_hfence_vvma(struct vmm_vcpu *vcpu,
				 unsigned long *vaddr, unsigned int *asid)
{
	/* TODO: */
}

void cpu_vcpu_nested_hfence_gvma(struct vmm_vcpu *vcpu,
				 physical_addr_t *gaddr, unsigned int *vmid)
{
	/* TODO: */
}

int cpu_vcpu_nested_hlv(struct vmm_vcpu *vcpu, unsigned long vaddr,
			bool hlvx, void *data, unsigned long len,
			unsigned long *out_scause,
			unsigned long *out_stval,
			unsigned long *out_htval)
{
	/* TODO: */
	return VMM_OK;
}

int cpu_vcpu_nested_hsv(struct vmm_vcpu *vcpu, unsigned long vaddr,
			const void *data, unsigned long len,
			unsigned long *out_scause,
			unsigned long *out_stval,
			unsigned long *out_htval)
{
	/* TODO: */
	return VMM_OK;
}

void cpu_vcpu_nested_set_virt(struct vmm_vcpu *vcpu, struct arch_regs *regs,
			      enum nested_set_virt_event event, bool virt,
			      bool spvp, bool gva)
{
	unsigned long tmp;
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);

	/* If H-extension is not available for VCPU then do nothing */
	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, h)) {
		return;
	}

	/* Skip hardware CSR update if no change in virt state */
	if (virt == npriv->virt)
		goto skip_csr_update;

	/* Swap hcounteren and hedeleg CSRs */
	npriv->hcounteren = csr_swap(CSR_HCOUNTEREN, npriv->hcounteren);
	npriv->hedeleg = csr_swap(CSR_HEDELEG, npriv->hedeleg);

	/* Update interrupt delegation */
	cpu_vcpu_irq_deleg_update(vcpu, virt);

	/* Update time delta */
	cpu_vcpu_time_delta_update(vcpu, virt);

	/* Update G-stage page table */
	cpu_vcpu_gstage_update(vcpu, virt);

	/* Swap hardware vs<xyz> CSRs except vsie and vsstatus */
	npriv->vstvec = csr_swap(CSR_VSTVEC, npriv->vstvec);
	npriv->vsscratch = csr_swap(CSR_VSSCRATCH, npriv->vsscratch);
	npriv->vsepc = csr_swap(CSR_VSEPC, npriv->vsepc);
	npriv->vscause = csr_swap(CSR_VSCAUSE, npriv->vscause);
	npriv->vstval = csr_swap(CSR_VSTVAL, npriv->vstval);
	npriv->vsatp = csr_swap(CSR_VSATP, npriv->vsatp);

	/* Update vsstatus CSR */
	if (virt) {
		/* Nested virtualization state changing from OFF to ON */

		/*
		 * Update vsstatus in following manner:
		 * 1) Swap hardware vsstatus (i.e. virtual-HS mode sstatus)
		 *    with vsstatus in nested virtualization context (i.e.
		 *    virtual-VS mode sstatus)
		 * 2) Swap host sstatus.FS (i.e. HS mode sstatus.FS) with
		 *    the vsstatus.FS saved in nested virtualization context
		 *    (i.e. virtual-HS mode sstatus.FS)
		 */
		npriv->vsstatus = csr_swap(CSR_VSSTATUS, npriv->vsstatus);
		tmp = regs->sstatus & SSTATUS_FS;
		regs->sstatus &= ~SSTATUS_FS;
		regs->sstatus |= (npriv->vsstatus & SSTATUS_FS);
		npriv->vsstatus &= ~SSTATUS_FS;
		npriv->vsstatus |= tmp;
	} else {
		/* Nested virtualization state changing from ON to OFF */

		/*
		 * Update vsstatus in following manner:
		 * 1) Swap host sstatus.FS (i.e. virtual-HS mode sstatus.FS)
		 *    with vsstatus.FS saved in the nested virtualization
		 *    context (i.e. HS mode sstatus.FS)
		 * 2) Swap hardware vsstatus (i.e. virtual-VS mode sstatus)
		 *    with vsstatus in nested virtualization context (i.e.
		 *    virtual-HS mode sstatus)
		 */
		tmp = regs->sstatus & SSTATUS_FS;
		regs->sstatus &= ~SSTATUS_FS;
		regs->sstatus |= (npriv->vsstatus & SSTATUS_FS);
		npriv->vsstatus &= ~SSTATUS_FS;
		npriv->vsstatus |= tmp;
		npriv->vsstatus = csr_swap(CSR_VSSTATUS, npriv->vsstatus);
	}

skip_csr_update:
	if (event != NESTED_SET_VIRT_EVENT_SRET) {
		/* Update Guest hstatus.SPV bit */
		npriv->hstatus &= ~HSTATUS_SPV;
		npriv->hstatus |= (npriv->virt) ? HSTATUS_SPV : 0;

		/* Update Guest hstatus.SPVP bit */
		if (npriv->virt) {
			npriv->hstatus &= ~HSTATUS_SPVP;
			if (spvp)
				npriv->hstatus |= HSTATUS_SPVP;
		}

		/* Update Guest hstatus.GVA bit */
		if (event == NESTED_SET_VIRT_EVENT_TRAP) {
			npriv->hstatus &= ~HSTATUS_GVA;
			npriv->hstatus |= (gva) ? HSTATUS_GVA : 0;
		}
	}

	/* Update host SRET and VM trapping */
	regs->hstatus &= ~HSTATUS_VTSR;
	if (virt && (npriv->hstatus & HSTATUS_VTSR)) {
		regs->hstatus |= HSTATUS_VTSR;
	}
	regs->hstatus &= ~HSTATUS_VTVM;
	if (virt && (npriv->hstatus & HSTATUS_VTVM)) {
		regs->hstatus |= HSTATUS_VTVM;
	}

	/* Update virt flag */
	npriv->virt = virt;
}
