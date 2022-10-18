/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file cpu_vcpu_trap.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source of VCPU trap handling
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <vmm_devemu.h>
#include <vmm_vcpu_irq.h>
#include <libs/stringlib.h>

#include <generic_mmu.h>
#include <cpu_hwcap.h>
#include <cpu_vcpu_nested.h>
#include <cpu_vcpu_trap.h>
#include <cpu_vcpu_unpriv.h>

void cpu_vcpu_redirect_smode_trap(arch_regs_t *regs,
				  struct cpu_vcpu_trap *trap, bool prev_spp)
{
	/* Read Guest sstatus */
	unsigned long vsstatus = csr_read(CSR_VSSTATUS);

	/* Change Guest sstatus.SPP bit */
	vsstatus &= ~SSTATUS_SPP;
	if (prev_spp)
		vsstatus |= SSTATUS_SPP;

	/* Change Guest sstatus.SPIE bit */
	vsstatus &= ~SSTATUS_SPIE;
	if (vsstatus & SSTATUS_SIE)
		vsstatus |= SSTATUS_SPIE;

	/* Clear Guest sstatus.SIE bit */
	vsstatus &= ~SSTATUS_SIE;

	/* Update Guest sstatus */
	csr_write(CSR_VSSTATUS, vsstatus);

	/* Update Guest scause, stval, and sepc */
	csr_write(CSR_VSCAUSE, trap->scause);
	csr_write(CSR_VSTVAL, trap->stval);
	csr_write(CSR_VSEPC, trap->sepc);

	/* Set next PC to exception vector */
	regs->sepc = csr_read(CSR_VSTVEC);

	/* Set next privilege mode to supervisor */
	regs->sstatus |= SSTATUS_SPP;
}

void cpu_vcpu_redirect_trap(struct vmm_vcpu *vcpu, arch_regs_t *regs,
			    struct cpu_vcpu_trap *trap)
{
	struct riscv_priv_nested *npriv = riscv_nested_priv(vcpu);
	bool prev_spp = (regs->sstatus & SSTATUS_SPP) ? TRUE : FALSE;
	bool gva = FALSE;

	/* Determine GVA bit state */
	switch (trap->scause) {
	case CAUSE_MISALIGNED_FETCH:
	case CAUSE_FETCH_ACCESS:
	case CAUSE_MISALIGNED_LOAD:
	case CAUSE_LOAD_ACCESS:
	case CAUSE_MISALIGNED_STORE:
	case CAUSE_STORE_ACCESS:
	case CAUSE_FETCH_PAGE_FAULT:
	case CAUSE_LOAD_PAGE_FAULT:
	case CAUSE_STORE_PAGE_FAULT:
	case CAUSE_FETCH_GUEST_PAGE_FAULT:
	case CAUSE_LOAD_GUEST_PAGE_FAULT:
	case CAUSE_STORE_GUEST_PAGE_FAULT:
		gva = TRUE;
		break;
	default:
		break;
	}

	/* Turn-off nested virtualization for virtual-HS mode */
	cpu_vcpu_nested_set_virt(vcpu, regs, NESTED_SET_VIRT_EVENT_TRAP,
				 FALSE, prev_spp, gva);

	/* Update Guest HTVAL and HTINST */
	npriv->htval = trap->htval;
	npriv->htinst = trap->htinst;

	/* Update Guest supervisor state */
	cpu_vcpu_redirect_smode_trap(regs, trap, prev_spp);
}

static int cpu_vcpu_stage2_map(struct vmm_vcpu *vcpu,
			       physical_addr_t fault_addr)
{
	int rc, rc1;
	u32 reg_flags = 0x0, pg_reg_flags = 0x0;
	struct mmu_page pg;
	physical_addr_t inaddr, outaddr;
	physical_size_t size, availsz;

	memset(&pg, 0, sizeof(pg));

	inaddr = fault_addr & PGTBL_L0_MAP_MASK;
	size = PGTBL_L0_BLOCK_SIZE;

	rc = vmm_guest_physical_map(vcpu->guest, inaddr, size,
				    &outaddr, &availsz, &reg_flags);
	if (rc) {
		vmm_printf("%s: guest_phys=0x%"PRIPADDR" size=0x%"PRIPSIZE
			   " map failed\n", __func__, inaddr, size);
		return rc;
	}

	if (availsz < PGTBL_L0_BLOCK_SIZE) {
		vmm_printf("%s: availsz=0x%"PRIPSIZE" insufficent for "
			   "guest_phys=0x%"PRIPADDR"\n",
			   __func__, availsz, inaddr);
		return VMM_ENOSPC;
	}

	pg.ia = inaddr;
	pg.sz = size;
	pg.oa = outaddr;
	pg_reg_flags = reg_flags;

	if (reg_flags & (VMM_REGION_ISRAM | VMM_REGION_ISROM)) {
		inaddr = fault_addr & PGTBL_L1_MAP_MASK;
		size = PGTBL_L1_BLOCK_SIZE;
		rc = vmm_guest_physical_map(vcpu->guest, inaddr, size,
				    &outaddr, &availsz, &reg_flags);
		if (!rc && (availsz >= PGTBL_L1_BLOCK_SIZE)) {
			pg.ia = inaddr;
			pg.sz = size;
			pg.oa = outaddr;
			pg_reg_flags = reg_flags;
		}
#ifdef CONFIG_64BIT
		inaddr = fault_addr & PGTBL_L2_MAP_MASK;
		size = PGTBL_L2_BLOCK_SIZE;
		rc = vmm_guest_physical_map(vcpu->guest, inaddr, size,
				    &outaddr, &availsz, &reg_flags);
		if (!rc && (availsz >= PGTBL_L2_BLOCK_SIZE)) {
			pg.ia = inaddr;
			pg.sz = size;
			pg.oa = outaddr;
			pg_reg_flags = reg_flags;
		}
#endif
	}

	arch_mmu_pgflags_set(&pg.flags, MMU_STAGE2, pg_reg_flags);

	/* Try to map the page in Stage2 */
	rc = mmu_map_page(riscv_guest_priv(vcpu->guest)->pgtbl, &pg);
	if (rc) {
		/* On SMP Guest, two different VCPUs may try to map same
		 * Guest region in Stage2 at the same time. This may cause
		 * mmu_map_page() to fail for one of the Guest VCPUs.
		 *
		 * To take care of this situation, we recheck Stage2 mapping
		 * when mmu_map_page() fails.
		 */
		memset(&pg, 0, sizeof(pg));
		rc1 = mmu_get_page(riscv_guest_priv(vcpu->guest)->pgtbl,
				   fault_addr, &pg);
		if (rc1) {
			return rc1;
		}
		rc = VMM_OK;
	}

	return rc;
}

static int cpu_vcpu_emulate_load(struct vmm_vcpu *vcpu,
				 arch_regs_t *regs,
				 physical_addr_t fault_addr,
				 unsigned long htinst)
{
	u8 data8;
	u16 data16;
	u32 data32;
	u64 data64;
	struct cpu_vcpu_trap trap;
	unsigned long insn, insn_len;
	int rc = VMM_OK, shift = 0, len = 0;

	if (htinst & 0x1) {
		/*
		 * Bit[0] == 1 implies trapped instruction value is
		 * transformed instruction or custom instruction.
		 */
		insn = htinst | INSN_16BIT_MASK;
		insn_len = (htinst & 0x2) ? INSN_LEN(insn) : 2;
	} else {
		/*
		 * Bit[0] == 0 implies trapped instruction value is
		 * zero or special value.
		 */
		trap.sepc = 0;
		trap.scause = 0;
		trap.stval = 0;
		trap.htval = 0;
		trap.htinst = 0;
		insn = __cpu_vcpu_unpriv_read_insn(regs->sepc, &trap);
		if (trap.scause) {
			if (trap.scause == CAUSE_LOAD_PAGE_FAULT)
				trap.scause = CAUSE_FETCH_PAGE_FAULT;
			trap.sepc = trap.stval = regs->sepc;
			cpu_vcpu_redirect_trap(vcpu, regs, &trap);
			return VMM_OK;
		}
		insn_len = INSN_LEN(insn);
	}

	if ((insn & INSN_MASK_LW) == INSN_MATCH_LW) {
		len = 4;
		shift = 8 * (sizeof(ulong) - len);
	} else if ((insn & INSN_MASK_LB) == INSN_MATCH_LB) {
		len = 1;
		shift = 8 * (sizeof(ulong) - len);
	} else if ((insn & INSN_MASK_LBU) == INSN_MATCH_LBU) {
		len = 1;
		shift = 8 * (sizeof(ulong) - len);
#if defined(CONFIG_64BIT)
	} else if ((insn & INSN_MASK_LD) == INSN_MATCH_LD) {
		len = 8;
		shift = 8 * (sizeof(ulong) - len);
	} else if ((insn & INSN_MASK_LWU) == INSN_MATCH_LWU) {
		len = 4;
#endif
	} else if ((insn & INSN_MASK_LH) == INSN_MATCH_LH) {
		len = 2;
		shift = 8 * (sizeof(ulong) - len);
	} else if ((insn & INSN_MASK_LHU) == INSN_MATCH_LHU) {
		len = 2;
# if defined(CONFIG_64BIT)
	} else if ((insn & INSN_MASK_C_LD) == INSN_MATCH_C_LD) {
		len = 8;
		shift = 8 * (sizeof(ulong) - len);
		insn = RVC_RS2S(insn) << SH_RD;
	} else if ((insn & INSN_MASK_C_LDSP) == INSN_MATCH_C_LDSP &&
		   ((insn >> SH_RD) & 0x1f)) {
		len = 8;
		shift = 8 * (sizeof(ulong) - len);
# endif
	} else if ((insn & INSN_MASK_C_LW) ==INSN_MATCH_C_LW) {
		len = 4;
		shift = 8 * (sizeof(ulong) - len);
		insn = RVC_RS2S(insn) << SH_RD;
	} else if ((insn & INSN_MASK_C_LWSP) == INSN_MATCH_C_LWSP &&
		   ((insn >> SH_RD) & 0x1f)) {
		len = 4;
		shift = 8 * (sizeof(ulong) - len);
	} else {
		return VMM_ENOTSUPP;
	}

	if (fault_addr & (len - 1)) {
		return VMM_EIO;
	}

	switch (len) {
	case 1:
		rc = vmm_devemu_emulate_read(vcpu, fault_addr,
					     &data8, sizeof(data8),
					     VMM_DEVEMU_LITTLE_ENDIAN);
		if (!rc) {
			SET_RD(insn, regs, (ulong)data8 << shift >> shift);
		}
		break;
	case 2:
		rc = vmm_devemu_emulate_read(vcpu, fault_addr,
					     &data16, sizeof(data16),
					     VMM_DEVEMU_LITTLE_ENDIAN);
		if (!rc) {
			SET_RD(insn, regs, (ulong)data16 << shift >> shift);
		}
		break;
	case 4:
		rc = vmm_devemu_emulate_read(vcpu, fault_addr,
					     &data32, sizeof(data32),
					     VMM_DEVEMU_LITTLE_ENDIAN);
		if (!rc) {
			SET_RD(insn, regs, (ulong)data32 << shift >> shift);
		}
		break;
	case 8:
		rc = vmm_devemu_emulate_read(vcpu, fault_addr,
					     &data64, sizeof(data64),
					     VMM_DEVEMU_LITTLE_ENDIAN);
		if (!rc) {
			SET_RD(insn, regs, (ulong)data64 << shift >> shift);
		}
	default:
		rc = VMM_EINVALID;
		break;
	};

	if (!rc) {
		regs->sepc += insn_len;
	}

	return rc;
}

static int cpu_vcpu_emulate_store(struct vmm_vcpu *vcpu,
				  arch_regs_t *regs,
				  physical_addr_t fault_addr,
				  unsigned long htinst)
{
	u8 data8;
	u16 data16;
	u32 data32;
	u64 data64;
	int rc = VMM_OK, len = 0;
	struct cpu_vcpu_trap trap;
	unsigned long data, insn, insn_len;

	if (htinst & 0x1) {
		/*
		 * Bit[0] == 1 implies trapped instruction value is
		 * transformed instruction or custom instruction.
		 */
		insn = htinst | INSN_16BIT_MASK;
		insn_len = (htinst & 0x2) ? INSN_LEN(insn) : 2;
	} else {
		 /*
		 * Bit[0] == 0 implies trapped instruction value is
		 * zero or special value.
		 */
		trap.sepc = 0;
		trap.scause = 0;
		trap.stval = 0;
		trap.htval = 0;
		trap.htinst = 0;
		insn = __cpu_vcpu_unpriv_read_insn(regs->sepc, &trap);
		if (trap.scause) {
			if (trap.scause == CAUSE_LOAD_PAGE_FAULT)
				trap.scause = CAUSE_FETCH_PAGE_FAULT;
			trap.sepc = trap.stval = regs->sepc;
			cpu_vcpu_redirect_trap(vcpu, regs, &trap);
			return VMM_OK;
		}
		insn_len = INSN_LEN(insn);
	}

	data8 = data16 = data32 = data64 = data = GET_RS2(insn, regs);

	if ((insn & INSN_MASK_SW) == INSN_MATCH_SW) {
		len = 4;
	} else if ((insn & INSN_MASK_SB) == INSN_MATCH_SB) {
		len = 1;
#if defined(CONFIG_64BIT)
	} else if ((insn & INSN_MASK_SD) == INSN_MATCH_SD) {
		len = 8;
#endif
	} else if ((insn & INSN_MASK_SH) == INSN_MATCH_SH) {
		len = 2;
# if defined(CONFIG_64BIT)
	} else if ((insn & INSN_MASK_C_SD) == INSN_MATCH_C_SD) {
		len = 8;
		data64 = GET_RS2S(insn, regs);
	} else if ((insn & INSN_MASK_C_SDSP) == INSN_MATCH_C_SDSP &&
		   ((insn >> SH_RD) & 0x1f)) {
		len = 8;
		data64 = GET_RS2C(insn, regs);
# endif
	} else if ((insn & INSN_MASK_C_SW) == INSN_MATCH_C_SW) {
		len = 4;
		data32 = GET_RS2S(insn, regs);
	} else if ((insn & INSN_MASK_C_SWSP) == INSN_MATCH_C_SWSP &&
		   ((insn >> SH_RD) & 0x1f)) {
		len = 4;
		data32 = GET_RS2C(insn, regs);
	} else {
		return VMM_ENOTSUPP;
	}

	if (fault_addr & (len - 1)) {
		return VMM_EIO;
	}

	switch (len) {
	case 1:
		rc = vmm_devemu_emulate_write(vcpu, fault_addr,
					      &data8, sizeof(data8),
					      VMM_DEVEMU_LITTLE_ENDIAN);
		break;
	case 2:
		rc = vmm_devemu_emulate_write(vcpu, fault_addr,
					      &data16, sizeof(data16),
					      VMM_DEVEMU_LITTLE_ENDIAN);
		break;
	case 4:
		rc = vmm_devemu_emulate_write(vcpu, fault_addr,
					      &data32, sizeof(data32),
					      VMM_DEVEMU_LITTLE_ENDIAN);
		break;
	case 8:
		rc = vmm_devemu_emulate_write(vcpu, fault_addr,
					      &data64, sizeof(data64),
					      VMM_DEVEMU_LITTLE_ENDIAN);
	default:
		rc = VMM_EINVALID;
		break;
	};

	if (!rc) {
		regs->sepc += insn_len;
	}

	return rc;
}

int cpu_vcpu_page_fault(struct vmm_vcpu *vcpu,
			arch_regs_t *regs,
			struct cpu_vcpu_trap *trap)
{
	int rc;
	struct vmm_region *reg;
	struct cpu_vcpu_trap otrap;
	physical_addr_t fault_addr;

	if (riscv_nested_virt(vcpu)) {
		otrap.sepc = 0;
		otrap.scause = 0;
		otrap.stval = 0;
		otrap.htval = 0;
		otrap.htinst = 0;
		rc = cpu_vcpu_nested_page_fault(vcpu,
			(regs->hstatus & HSTATUS_SPVP) ? TRUE : FALSE,
			trap, &otrap);
		if (rc) {
			return rc;
		}

		if (otrap.scause) {
			cpu_vcpu_redirect_trap(vcpu, regs, &otrap);
		}

		return VMM_OK;
	}

	fault_addr = ((physical_addr_t)trap->htval << 2);
	fault_addr |= ((physical_addr_t)trap->stval & 0x3);

	reg = vmm_guest_find_region(vcpu->guest, fault_addr,
				    VMM_REGION_VIRTUAL | VMM_REGION_MEMORY,
				    FALSE);
	if (reg) {
		/* Emulate load/store instructions for virtual device */
		switch (trap->scause) {
		case CAUSE_LOAD_GUEST_PAGE_FAULT:
			return cpu_vcpu_emulate_load(vcpu, regs, fault_addr,
						     trap->htinst);
		case CAUSE_STORE_GUEST_PAGE_FAULT:
			return cpu_vcpu_emulate_store(vcpu, regs, fault_addr,
						      trap->htinst);
		default:
			return VMM_ENOTSUPP;
		};
	}

	/* Mapping does not exist hence create one */
	return cpu_vcpu_stage2_map(vcpu, fault_addr);
}

static int truly_illegal_insn(struct vmm_vcpu *vcpu,
			      arch_regs_t *regs,
			      ulong insn)
{
	struct cpu_vcpu_trap trap;

	/* Redirect trap to Guest VCPU */
	trap.sepc = regs->sepc;
	trap.scause = CAUSE_ILLEGAL_INSTRUCTION;
	trap.stval = insn;
	trap.htval = 0;
	trap.htinst = 0;
	cpu_vcpu_redirect_trap(vcpu, regs, &trap);

	return VMM_OK;
}

static int truly_virtual_insn(struct vmm_vcpu *vcpu,
			      arch_regs_t *regs,
			      ulong insn)
{
	struct cpu_vcpu_trap trap;

	/* Redirect trap to Guest VCPU */
	trap.sepc = regs->sepc;
	trap.scause = CAUSE_VIRTUAL_INST_FAULT;
	trap.stval = insn;
	trap.htval = 0;
	trap.htinst = 0;
	cpu_vcpu_redirect_trap(vcpu, regs, &trap);

	return VMM_OK;
}

struct system_opcode_func {
	ulong mask;
	ulong match;
	/*
	 * Possible return values are as follows:
	 * 1) Returns < 0 for error case
	 * 2) Return == 0 to increment PC and continue
	 * 3) Return == 1 to inject illegal instruction trap and continue
	 * 4) Return == 2 to inject virtual instruction trap and continue
	 * 5) Return == 3 to do nothing and continue
	 */
	int (*func)(struct vmm_vcpu *vcpu, arch_regs_t *regs, ulong insn);
};

struct csr_func {
	unsigned int csr_num;
	/*
	 * Possible return values are as follows:
	 * 1) Returns < 0 for error case
	 * 2) Return == 0 to increment PC and continue
	 * 3) Return == 1 to inject illegal instruction trap and continue
	 * 4) Return == 2 to inject virtual instruction trap and continue
	 * 5) Return == 3 to do nothing and continue
	 */
	int (*rmw_func)(struct vmm_vcpu *vcpu, arch_regs_t *regs,
			unsigned int csr_num, unsigned long *val,
			unsigned long new_val, unsigned long wr_mask);
};

static const struct csr_func csr_funcs[] = {
	{
		.csr_num  = CSR_SIE,
		.rmw_func = cpu_vcpu_nested_smode_csr_rmw,
	},
	{
		.csr_num  = CSR_SIEH,
		.rmw_func = cpu_vcpu_nested_smode_csr_rmw,
	},
	{
		.csr_num  = CSR_SIP,
		.rmw_func = cpu_vcpu_nested_smode_csr_rmw,
	},
	{
		.csr_num  = CSR_SIPH,
		.rmw_func = cpu_vcpu_nested_smode_csr_rmw,
	},
	{
		.csr_num  = CSR_STIMECMP,
		.rmw_func = cpu_vcpu_nested_smode_csr_rmw,
	},
	{
		.csr_num  = CSR_STIMECMPH,
		.rmw_func = cpu_vcpu_nested_smode_csr_rmw,
	},
	{
		.csr_num  = CSR_HSTATUS,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HEDELEG,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HIDELEG,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HVIP,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HIE,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HIP,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HGEIP,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HGEIE,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HCOUNTEREN,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HTIMEDELTA,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HTIMEDELTAH,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HTVAL,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HTINST,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HGATP,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HENVCFG,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_HENVCFGH,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_VSSTATUS,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_VSIP,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_VSIE,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_VSTVEC,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_VSSCRATCH,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_VSEPC,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_VSCAUSE,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_VSTVAL,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_VSATP,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_VSTIMECMP,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
	{
		.csr_num  = CSR_VSTIMECMPH,
		.rmw_func = cpu_vcpu_nested_hext_csr_rmw,
	},
};

static int csr_insn(struct vmm_vcpu *vcpu, arch_regs_t *regs, ulong insn)
{
	int i, rc = TRAP_RETURN_ILLEGAL_INSN;
	unsigned int csr_num = insn >> SH_RS2;
	unsigned int rs1_num = (insn >> SH_RS1) & MASK_RX;
	ulong rs1_val = GET_RS1(insn, regs);
	const struct csr_func *tcfn, *cfn = NULL;
	ulong val = 0, wr_mask = 0, new_val = 0;

	/* Decode the CSR instruction */
	switch (GET_RM(insn)) {
	case 1:
		wr_mask = -1UL;
		new_val = rs1_val;
		break;
	case 2:
		wr_mask = rs1_val;
		new_val = -1UL;
		break;
	case 3:
		wr_mask = rs1_val;
		new_val = 0;
		break;
	case 5:
		wr_mask = -1UL;
		new_val = rs1_num;
		break;
	case 6:
		wr_mask = rs1_num;
		new_val = -1UL;
		break;
	case 7:
		wr_mask = rs1_num;
		new_val = 0;
		break;
	default:
		return rc;
	}

	/* Find CSR function */
	for (i = 0; i < array_size(csr_funcs); i++) {
		tcfn = &csr_funcs[i];
		if (tcfn->csr_num == csr_num) {
			cfn = tcfn;
			break;
		}
	}
	if (!cfn || !cfn->rmw_func) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	/* Do CSR emulation */
	rc = cfn->rmw_func(vcpu, regs, csr_num, &val, new_val, wr_mask);
	if (rc) {
		return rc;
	}

	/* Update destination register for CSR reads */
	if ((insn >> SH_RD) & MASK_RX) {
		SET_RD(insn, regs, val);
	}

	return VMM_OK;
}

int cpu_vcpu_sret_insn(struct vmm_vcpu *vcpu, arch_regs_t *regs, ulong insn)
{
	bool next_virt;
	unsigned long vsstatus, next_sepc, next_spp;

	/*
	 * Trap from virtual-VS and virtual-VU modes should be forwarded to
	 * virtual-HS mode as a virtual instruction trap.
	 */
	if (riscv_nested_virt(vcpu)) {
		return TRAP_RETURN_VIRTUAL_INSN;
	}

	/*
	 * Trap from virtual-U mode should be forwarded to virtual-HS mode
	 * as illegal instruction trap.
	 */
	if (!(regs->hstatus & HSTATUS_SPVP)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}
	vsstatus = csr_read(CSR_VSSTATUS);

	/*
	 * Find next nested virtualization mode, next privilege mode,
	 * and next sepc
	 */
	next_virt = (riscv_nested_priv(vcpu)->hstatus & HSTATUS_SPV) ?
		    TRUE : FALSE;
	next_sepc = csr_read(CSR_VSEPC);
	next_spp = vsstatus & SSTATUS_SPP;

	/* Update Guest sstatus.sie */
	vsstatus &= ~SSTATUS_SIE;
	vsstatus |= (vsstatus & SSTATUS_SPIE) ? SSTATUS_SIE : 0;
	csr_write(CSR_VSSTATUS, vsstatus);

	/* Update return address and return privilege mode*/
	regs->sepc = next_sepc;
	regs->sstatus &= ~SSTATUS_SPP;
	regs->sstatus |= next_spp;

	/* Set nested virtualization state based on guest hstatus.SPV */
	cpu_vcpu_nested_set_virt(vcpu, regs, NESTED_SET_VIRT_EVENT_SRET,
				 next_virt, FALSE, FALSE);

	return TRAP_RETURN_CONTINUE;
}

static int wfi_insn(struct vmm_vcpu *vcpu, arch_regs_t *regs, ulong insn)
{
	/*
	 * Trap from virtual-VS and virtual-VU modes should be forwarded to
	 * virtual-HS mode as a virtual instruction trap.
	 */
	if (riscv_nested_virt(vcpu)) {
		return TRAP_RETURN_VIRTUAL_INSN;
	}

	/*
	 * Trap from virtual-U mode should be forwarded to virtual-HS mode
	 * as illegal instruction trap.
	 */
	if (!(regs->hstatus & HSTATUS_SPVP)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	/* Wait for irq with default timeout */
	vmm_vcpu_irq_wait_timeout(vcpu, 0);
	return VMM_OK;
}

static int hfence_vvma_insn(struct vmm_vcpu *vcpu,
			    arch_regs_t *regs, ulong insn)
{
	unsigned int rs1_num = (insn >> SH_RS1) & MASK_RX;
	unsigned int rs2_num = (insn >> SH_RS2) & MASK_RX;
	unsigned long vaddr = GET_RS1(insn, regs);
	unsigned int asid = GET_RS2(insn, regs);

	/*
	 * If H-extension is not available for VCPU then forward trap
	 * as illegal instruction trap to virtual-HS mode.
	 */
	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, h)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	/*
	 * Trap from virtual-VS and virtual-VU modes should be forwarded to
	 * virtual-HS mode as a virtual instruction trap.
	 */
	if (riscv_nested_virt(vcpu)) {
		return TRAP_RETURN_VIRTUAL_INSN;
	}

	/*
	 * Trap from virtual-U mode should be forwarded to virtual-HS mode
	 * as illegal instruction trap.
	 */
	if (!(regs->hstatus & HSTATUS_SPVP)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	if (!rs1_num && !rs2_num) {
		cpu_vcpu_nested_hfence_vvma(vcpu, NULL, NULL);
	} else if (!rs1_num && rs2_num) {
		cpu_vcpu_nested_hfence_vvma(vcpu, NULL, &asid);
	} else if (rs1_num && !rs2_num) {
		cpu_vcpu_nested_hfence_vvma(vcpu, &vaddr, NULL);
	} else {
		cpu_vcpu_nested_hfence_vvma(vcpu, &vaddr, &asid);
	}

	return VMM_OK;
}

static int hfence_gvma_insn(struct vmm_vcpu *vcpu,
			    arch_regs_t *regs, ulong insn)
{
	unsigned int rs1_num = (insn >> SH_RS1) & MASK_RX;
	unsigned int rs2_num = (insn >> SH_RS2) & MASK_RX;
	physical_addr_t gaddr = GET_RS1(insn, regs) << 2;
	unsigned int vmid = GET_RS2(insn, regs);

	/*
	 * If H-extension is not available for VCPU then forward trap
	 * as illegal instruction trap to virtual-HS mode.
	 */
	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, h)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	/*
	 * Trap from virtual-VS and virtual-VU modes should be forwarded to
	 * virtual-HS mode as a virtual instruction trap.
	 */
	if (riscv_nested_virt(vcpu)) {
		return TRAP_RETURN_VIRTUAL_INSN;
	}

	/*
	 * Trap from virtual-U mode should be forwarded to virtual-HS mode
	 * as illegal instruction trap.
	 */
	if (!(regs->hstatus & HSTATUS_SPVP)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	if (!rs1_num && !rs2_num) {
		cpu_vcpu_nested_hfence_gvma(vcpu, NULL, NULL);
	} else if (!rs1_num && rs2_num) {
		cpu_vcpu_nested_hfence_gvma(vcpu, NULL, &vmid);
	} else if (rs1_num && !rs2_num) {
		cpu_vcpu_nested_hfence_gvma(vcpu, &gaddr, NULL);
	} else {
		cpu_vcpu_nested_hfence_gvma(vcpu, &gaddr, &vmid);
	}

	return VMM_OK;
}

union hxv_reg_data {
	ulong data_ulong;
	u64 data_u64;
	u32 data_u32;
	u16 data_u16;
	u8 data_u8;
};

static int hlv_insn(struct vmm_vcpu *vcpu, arch_regs_t *regs, ulong insn)
{
	int rc;
	void *data;
	bool hlvx = FALSE;
	union hxv_reg_data v;
	struct cpu_vcpu_trap trap;
	unsigned long shift = 0, len, vaddr;

	/*
	 * If H-extension is not available for VCPU then forward trap
	 * as illegal instruction trap to virtual-HS mode.
	 */
	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, h)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	/*
	 * Trap from virtual-VS and virtual-VU modes should be forwarded to
	 * virtual-HS mode as a virtual instruction trap.
	 */
	if (riscv_nested_virt(vcpu)) {
		return TRAP_RETURN_VIRTUAL_INSN;
	}

	/*
	 * Trap from virtual-U mode should be forwarded to virtual-HS mode
	 * as illegal instruction trap when guest hstatus.HU == 0.
	 */
	if (!(regs->hstatus & HSTATUS_SPVP) &&
	    !(riscv_nested_priv(vcpu)->hstatus & HSTATUS_HU)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	vaddr = GET_RS1(insn, regs);
	v.data_u64 = 0;

	if ((insn & INSN_MASK_HLV_B) == INSN_MATCH_HLV_B) {
		data = &v.data_u8;
		len = 1;
		shift = (sizeof(long) - 1) * 8;
	} else if ((insn & INSN_MASK_HLV_BU) == INSN_MATCH_HLV_BU) {
		data = &v.data_u8;
		len = 1;
	} else if ((insn & INSN_MASK_HLV_H) == INSN_MATCH_HLV_H) {
		data = &v.data_u16;
		len = 2;
		shift = (sizeof(long) - 2) * 8;
	} else if ((insn & INSN_MASK_HLV_HU) == INSN_MATCH_HLV_HU) {
		data = &v.data_u16;
		len = 2;
	} else if ((insn & INSN_MASK_HLVX_HU) == INSN_MATCH_HLVX_HU) {
		data = &v.data_u16;
		len = 2;
		hlvx = TRUE;
	} else if ((insn & INSN_MASK_HLV_W) == INSN_MATCH_HLV_W) {
		data = &v.data_u32;
		len = 4;
		shift = (sizeof(long) - 4) * 8;
	} else if ((insn & INSN_MASK_HLV_WU) == INSN_MATCH_HLV_WU) {
		data = &v.data_u32;
		len = 4;
	} else if ((insn & INSN_MASK_HLVX_WU) == INSN_MATCH_HLVX_WU) {
		data = &v.data_u32;
		len = 4;
		hlvx = TRUE;
	} else if ((insn & INSN_MASK_HLV_D) == INSN_MATCH_HLV_D) {
		data = &v.data_u64;
		len = 8;
	} else {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	trap.sepc = regs->sepc;
	trap.scause = 0;
	trap.stval = 0;
	trap.htval = 0;
	trap.htinst = insn;

	rc = cpu_vcpu_nested_hlv(vcpu, vaddr, hlvx, data, len,
				 &trap.scause, &trap.stval, &trap.htval);
	if (rc) {
		return rc;
	}

	if (trap.scause) {
		cpu_vcpu_redirect_trap(vcpu, regs, &trap);
		return TRAP_RETURN_CONTINUE;
	} else {
		SET_RD(insn, regs, ((long)(v.data_ulong << shift)) >> shift);
	}

	return VMM_OK;
}

static int hsv_insn(struct vmm_vcpu *vcpu, arch_regs_t *regs, ulong insn)
{
	int rc;
	void *data;
	union hxv_reg_data v;
	unsigned long len, vaddr;
	struct cpu_vcpu_trap trap;

	/*
	 * If H-extension is not available for VCPU then forward trap
	 * as illegal instruction trap to virtual-HS mode.
	 */
	if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, h)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	/*
	 * Trap from virtual-VS and virtual-VU modes should be forwarded to
	 * virtual-HS mode as a virtual instruction trap.
	 */
	if (riscv_nested_virt(vcpu)) {
		return TRAP_RETURN_VIRTUAL_INSN;
	}

	/*
	 * Trap from virtual-U mode should be forwarded to virtual-HS mode
	 * as illegal instruction trap when guest hstatus.HU == 0.
	 */
	if (!(regs->hstatus & HSTATUS_SPVP) &&
	    !(riscv_nested_priv(vcpu)->hstatus & HSTATUS_HU)) {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	vaddr = GET_RS1(insn, regs);
	v.data_ulong = GET_RS2(insn, regs);

	if ((insn & INSN_MASK_HSV_B) == INSN_MATCH_HSV_B) {
		data = &v.data_u8;
		len = 1;
	} else if ((insn & INSN_MASK_HSV_H) == INSN_MATCH_HSV_H) {
		data = &v.data_u16;
		len = 2;
	} else if ((insn & INSN_MASK_HSV_W) == INSN_MATCH_HSV_W) {
		data = &v.data_u32;
		len = 4;
	} else if ((insn & INSN_MASK_HSV_D) == INSN_MATCH_HSV_D) {
		data = &v.data_u64;
		len = 8;
	} else {
		return TRAP_RETURN_ILLEGAL_INSN;
	}

	trap.sepc = regs->sepc;
	trap.scause = 0;
	trap.stval = 0;
	trap.htval = 0;
	trap.htinst = insn;

	rc = cpu_vcpu_nested_hsv(vcpu, vaddr, data, len,
				 &trap.scause, &trap.stval, &trap.htval);
	if (rc) {
		return rc;
	}

	if (trap.scause) {
		cpu_vcpu_redirect_trap(vcpu, regs, &trap);
		return TRAP_RETURN_CONTINUE;
	}

	return VMM_OK;
}

static const struct system_opcode_func system_opcode_funcs[] = {
	{
		.mask  = INSN_MASK_CSRRW,
		.match = INSN_MATCH_CSRRW,
		.func  = csr_insn,
	},
	{
		.mask  = INSN_MASK_CSRRS,
		.match = INSN_MATCH_CSRRS,
		.func  = csr_insn,
	},
	{
		.mask  = INSN_MASK_CSRRC,
		.match = INSN_MATCH_CSRRC,
		.func  = csr_insn,
	},
	{
		.mask  = INSN_MASK_CSRRWI,
		.match = INSN_MATCH_CSRRWI,
		.func  = csr_insn,
	},
	{
		.mask  = INSN_MASK_CSRRSI,
		.match = INSN_MATCH_CSRRSI,
		.func  = csr_insn,
	},
	{
		.mask  = INSN_MASK_CSRRCI,
		.match = INSN_MATCH_CSRRCI,
		.func  = csr_insn,
	},
	{
		.mask  = INSN_MASK_SRET,
		.match = INSN_MATCH_SRET,
		.func  = cpu_vcpu_sret_insn,
	},
	{
		.mask  = INSN_MASK_WFI,
		.match = INSN_MATCH_WFI,
		.func  = wfi_insn,
	},
	{
		.mask  = INSN_MASK_HFENCE_VVMA,
		.match = INSN_MATCH_HFENCE_VVMA,
		.func  = hfence_vvma_insn,
	},
	{
		.mask  = INSN_MASK_HFENCE_GVMA,
		.match = INSN_MATCH_HFENCE_GVMA,
		.func  = hfence_gvma_insn,
	},
	{
		.mask  = INSN_MASK_HLV_B,
		.match = INSN_MATCH_HLV_B,
		.func  = hlv_insn,
	},
	{
		.mask  = INSN_MASK_HLV_BU,
		.match = INSN_MATCH_HLV_BU,
		.func  = hlv_insn,
	},
	{
		.mask  = INSN_MASK_HLV_H,
		.match = INSN_MATCH_HLV_H,
		.func  = hlv_insn,
	},
	{
		.mask  = INSN_MASK_HLV_HU,
		.match = INSN_MATCH_HLV_HU,
		.func  = hlv_insn,
	},
	{
		.mask  = INSN_MASK_HLVX_HU,
		.match = INSN_MATCH_HLVX_HU,
		.func  = hlv_insn,
	},
	{
		.mask  = INSN_MASK_HLV_W,
		.match = INSN_MATCH_HLV_W,
		.func  = hlv_insn,
	},
	{
		.mask  = INSN_MASK_HLV_WU,
		.match = INSN_MATCH_HLV_WU,
		.func  = hlv_insn,
	},
	{
		.mask  = INSN_MASK_HLVX_WU,
		.match = INSN_MATCH_HLVX_WU,
		.func  = hlv_insn,
	},
	{
		.mask  = INSN_MASK_HLV_D,
		.match = INSN_MATCH_HLV_D,
		.func  = hlv_insn,
	},
	{
		.mask  = INSN_MASK_HSV_B,
		.match = INSN_MATCH_HSV_B,
		.func  = hsv_insn,
	},
	{
		.mask  = INSN_MASK_HSV_H,
		.match = INSN_MATCH_HSV_H,
		.func  = hsv_insn,
	},
	{
		.mask  = INSN_MASK_HSV_W,
		.match = INSN_MATCH_HSV_W,
		.func  = hsv_insn,
	},
	{
		.mask  = INSN_MASK_HSV_D,
		.match = INSN_MATCH_HSV_D,
		.func  = hsv_insn,
	},
};

static int system_opcode_insn(struct vmm_vcpu *vcpu,
			      arch_regs_t *regs,
			      ulong insn)
{
	int i, rc = TRAP_RETURN_ILLEGAL_INSN;
	const struct system_opcode_func *ifn;

	for (i = 0; i < array_size(system_opcode_funcs); i++) {
		ifn = &system_opcode_funcs[i];
		if ((insn & ifn->mask) == ifn->match) {
			rc = ifn->func(vcpu, regs, insn);
			break;
		}
	}

	if (rc == TRAP_RETURN_ILLEGAL_INSN)
		return truly_illegal_insn(vcpu, regs, insn);
	else if (rc == TRAP_RETURN_VIRTUAL_INSN)
		return truly_virtual_insn(vcpu, regs, insn);

	if (!rc) {
		regs->sepc += INSN_LEN(insn);
	}

	return (rc < 0) ? rc : VMM_OK;
}

int cpu_vcpu_general_fault(struct vmm_vcpu *vcpu,
			   arch_regs_t *regs, struct cpu_vcpu_trap *trap)
{
	struct cpu_vcpu_trap itrap = { 0 };

	/*
	 * Trap from virtual-VS and virtual-VU modes should be forwarded
	 * to virtual-HS mode.
	 */

	if (!riscv_nested_virt(vcpu)) {
		return VMM_EINVALID;
	}

	/*
	 * Blindly forward all general faults to virtual-HS mode
	 * except illegal instruction fault
	 */
	if (trap->scause != CAUSE_ILLEGAL_INSTRUCTION) {
		cpu_vcpu_redirect_trap(vcpu, regs, trap);
		return VMM_OK;
	}

	/* Update trap->stval for illegal instruction fault */
	if (unlikely((trap->stval & INSN_16BIT_MASK) != INSN_16BIT_MASK)) {
		if (trap->stval == 0) {
			trap->stval = __cpu_vcpu_unpriv_read_insn(regs->sepc,
								  &itrap);
			if (itrap.scause) {
				if (itrap.scause == CAUSE_LOAD_PAGE_FAULT)
					itrap.scause = CAUSE_FETCH_PAGE_FAULT;
				itrap.stval = itrap.sepc = regs->sepc;
				cpu_vcpu_redirect_trap(vcpu, regs, &itrap);
				return VMM_OK;
			}
		}
	}

	/* Forward illegal instruction fault */
	return truly_illegal_insn(vcpu, regs, trap->stval);
}

int cpu_vcpu_illegal_insn_fault(struct vmm_vcpu *vcpu,
				arch_regs_t *regs,
				unsigned long stval)
{
	unsigned long insn = stval;
	struct cpu_vcpu_trap trap = { 0 };

	/*
	 * Trap from virtual-VS and virtual-VU modes should be forwarded to
	 * virtual-HS mode as a illegal instruction trap.
	 */

	if (!riscv_nested_virt(vcpu)) {
		return VMM_EINVALID;
	}

	if (unlikely((insn & INSN_16BIT_MASK) != INSN_16BIT_MASK)) {
		if (insn == 0) {
			insn = __cpu_vcpu_unpriv_read_insn(regs->sepc, &trap);
			if (trap.scause) {
				if (trap.scause == CAUSE_LOAD_PAGE_FAULT)
					trap.scause = CAUSE_FETCH_PAGE_FAULT;
				trap.stval = trap.sepc = regs->sepc;
				cpu_vcpu_redirect_trap(vcpu, regs, &trap);
				return VMM_OK;
			}
		}
	}

	return truly_illegal_insn(vcpu, regs, insn);
}

int cpu_vcpu_virtual_insn_fault(struct vmm_vcpu *vcpu,
				arch_regs_t *regs,
				unsigned long stval)
{
	unsigned long insn = stval;
	struct cpu_vcpu_trap trap = { 0 };

	if (unlikely((insn & INSN_16BIT_MASK) != INSN_16BIT_MASK)) {
		if (insn == 0) {
			insn = __cpu_vcpu_unpriv_read_insn(regs->sepc, &trap);
			if (trap.scause) {
				if (trap.scause == CAUSE_LOAD_PAGE_FAULT)
					trap.scause = CAUSE_FETCH_PAGE_FAULT;
				trap.stval = trap.sepc = regs->sepc;
				cpu_vcpu_redirect_trap(vcpu, regs, &trap);
				return VMM_OK;
			}
		}
		if ((insn & INSN_16BIT_MASK) != INSN_16BIT_MASK)
			return truly_illegal_insn(vcpu, regs, insn);
	}

	switch ((insn & INSN_OPCODE_MASK) >> INSN_OPCODE_SHIFT) {
	case INSN_OPCODE_SYSTEM:
		return system_opcode_insn(vcpu, regs, insn);
	default:
		return truly_illegal_insn(vcpu, regs, insn);
	};
}

int cpu_vcpu_redirect_vsirq(struct vmm_vcpu *vcpu, arch_regs_t *regs,
			    unsigned long irq)
{
	struct cpu_vcpu_trap trap = { 0 };

	if (!vcpu || !vcpu->is_normal || !riscv_nested_virt(vcpu)) {
		return VMM_EFAIL;
	}

	trap.sepc = regs->sepc;
	trap.scause = SCAUSE_INTERRUPT_MASK | (irq - 1);
	cpu_vcpu_redirect_trap(vcpu, regs, &trap);

	return VMM_OK;
}
