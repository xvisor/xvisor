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
#include <cpu_mmu.h>
#include <cpu_vcpu_csr.h>
#include <cpu_vcpu_trap.h>

#include <riscv_unpriv.h>

int cpu_vcpu_page_fault(struct vmm_vcpu *vcpu,
			arch_regs_t *regs,
			unsigned long cause,
			unsigned long fault_addr)
{
	int rc, rc1;
	u32 reg_flags = 0x0, pg_reg_flags = 0x0;
	struct cpu_page pg;
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

	pg.user = 1;
	if (pg_reg_flags & VMM_REGION_VIRTUAL) {
		pg.read = 0;
		pg.write = 0;
		pg.execute = 0;
	} else if (pg_reg_flags & VMM_REGION_READONLY) {
		pg.read = 1;
		pg.write = 0;
		pg.execute = 1;
	} else {
		pg.read = 1;
		pg.write = 1;
		pg.execute = 1;
	}
	pg.valid = 1;

	/* Try to map the page in Stage2 */
	rc = cpu_mmu_map_page(riscv_guest_priv(vcpu->guest)->pgtbl, &pg);
	if (rc) {
		/* On SMP Guest, two different VCPUs may try to map same
		 * Guest region in Stage2 at the same time. This may cause
		 * cpu_mmu_map_page() to fail for one of the Guest VCPUs.
		 *
		 * To take care of this situation, we recheck Stage2 mapping
		 * when cpu_mmu_map_page() fails.
		 */
		memset(&pg, 0, sizeof(pg));
		rc1 = cpu_mmu_get_page(riscv_guest_priv(vcpu->guest)->pgtbl,
				       fault_addr, &pg);
		if (rc1) {
			return rc1;
		}
		rc = VMM_OK;
	}

	return rc;
}

int cpu_vcpu_load_access_fault(struct vmm_vcpu *vcpu,
			       arch_regs_t *regs,
			       unsigned long fault_addr)
{
	u8 data8;
	u16 data16;
	u32 data32;
	u64 data64;
	int rc = VMM_OK, shift = 0, len = 0;
	ulong insn = get_insn(regs->sepc, NULL, NULL);

	if ((insn & INSN_MASK_LW) == INSN_MATCH_LW) {
		len = 4;
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
#ifdef __riscv_compressed
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
#endif
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
		regs->sepc += INSN_LEN(insn);
	}

	return rc;
}

int cpu_vcpu_store_access_fault(struct vmm_vcpu *vcpu,
				arch_regs_t *regs,
				unsigned long fault_addr)
{
	u8 data8;
	u16 data16;
	u32 data32;
	u64 data64;
	ulong data;
	int rc = VMM_OK, len = 0;
	ulong insn = get_insn(regs->sepc, NULL, NULL);

	data8 = data16 = data32 = data64 = data = GET_RS2(insn, regs);

	if ((insn & INSN_MASK_SW) == INSN_MATCH_SW) {
		len = 4;
#if defined(CONFIG_64BIT)
	} else if ((insn & INSN_MASK_SD) == INSN_MATCH_SD) {
		len = 8;
#endif
	} else if ((insn & INSN_MASK_SH) == INSN_MATCH_SH) {
		len = 2;
#ifdef __riscv_compressed
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
#endif
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
		regs->sepc += INSN_LEN(insn);
	}

	return rc;
}

typedef int (*illegal_insn_func)(struct vmm_vcpu *vcpu,
				 arch_regs_t *regs,
				 ulong insn);


static int truly_illegal_insn(struct vmm_vcpu *vcpu,
			      arch_regs_t *regs,
			      ulong insn)
{
	/* TODO: Redirect trap to Guest VCPU */
	return VMM_ENOTSUPP;
}

static int system_opcode_insn(struct vmm_vcpu *vcpu,
			      arch_regs_t *regs,
			      ulong insn)
{
	int rc = VMM_OK, do_write, rs1_num;
	ulong rs1_val, csr_num, csr_val, new_csr_val;

	if ((insn & INSN_MASK_WFI) == INSN_MATCH_WFI) {
		/* Wait for irq with default timeout */
		vmm_vcpu_irq_wait_timeout(vcpu, 0);
		goto done;
	}

	rs1_num = (insn >> 15) & 0x1f;
	rs1_val = GET_RS1(insn, regs);
	csr_num = insn >> 20;

	rc = cpu_vcpu_csr_read(vcpu, csr_num, &csr_val);
	if (rc == VMM_EINVALID) {
		return truly_illegal_insn(vcpu, regs, insn);
	}
	if (rc) {
		return rc;
	}

	do_write = rs1_num;
	switch (GET_RM(insn)) {
	case 1:
		new_csr_val = rs1_val;
		do_write = 1;
		break;
	case 2:
		new_csr_val = csr_val | rs1_val;
		break;
	case 3: new_csr_val = csr_val & ~rs1_val;
		break;
	case 5:
		new_csr_val = rs1_num;
		do_write = 1;
		break;
	case 6:
		new_csr_val = csr_val | rs1_num;
		break;
	case 7:
		new_csr_val = csr_val & ~rs1_num;
		break;
	default:
		return truly_illegal_insn(vcpu, regs, insn);
	};

	if (do_write) {
		rc = cpu_vcpu_csr_write(vcpu, csr_num, new_csr_val);
		if (rc == VMM_EINVALID) {
			return truly_illegal_insn(vcpu, regs, insn);
		}
		if (rc) {
			return rc;
		}
	}

	SET_RD(insn, regs, csr_val);

done:
	regs->sepc += 4;

	return rc;
}

static illegal_insn_func illegal_insn_table[32] = {
	truly_illegal_insn, /* 0 */
	truly_illegal_insn, /* 1 */
	truly_illegal_insn, /* 2 */
	truly_illegal_insn, /* 3 */
	truly_illegal_insn, /* 4 */
	truly_illegal_insn, /* 5 */
	truly_illegal_insn, /* 6 */
	truly_illegal_insn, /* 7 */
	truly_illegal_insn, /* 8 */
	truly_illegal_insn, /* 9 */
	truly_illegal_insn, /* 10 */
	truly_illegal_insn, /* 11 */
	truly_illegal_insn, /* 12 */
	truly_illegal_insn, /* 13 */
	truly_illegal_insn, /* 14 */
	truly_illegal_insn, /* 15 */
	truly_illegal_insn, /* 16 */
	truly_illegal_insn, /* 17 */
	truly_illegal_insn, /* 18 */
	truly_illegal_insn, /* 19 */
	truly_illegal_insn, /* 20 */
	truly_illegal_insn, /* 21 */
	truly_illegal_insn, /* 22 */
	truly_illegal_insn, /* 23 */
	truly_illegal_insn, /* 24 */
	truly_illegal_insn, /* 25 */
	truly_illegal_insn, /* 26 */
	truly_illegal_insn, /* 27 */
	system_opcode_insn, /* 28 */
	truly_illegal_insn, /* 29 */
	truly_illegal_insn, /* 30 */
	truly_illegal_insn  /* 31 */
};

int cpu_vcpu_illegal_insn_fault(struct vmm_vcpu *vcpu,
				arch_regs_t *regs,
				unsigned long stval)
{
	ulong insn = stval;

	if (unlikely((insn & 3) != 3)) {
		if (insn == 0)
			insn = get_insn(regs->sepc, NULL, NULL);
		if ((insn & 3) != 3)
			return truly_illegal_insn(vcpu, regs, insn);
	}

	return illegal_insn_table[(insn & 0x7c) >> 2](vcpu, regs, insn);
}
