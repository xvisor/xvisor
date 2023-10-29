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
 * @file cpu_vcpu_fp.c
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief source of VCPU FP functions
 */

#include <vmm_stdio.h>
#include <libs/stringlib.h>

#include <cpu_hwcap.h>
#include <cpu_vcpu_switch.h>
#include <cpu_vcpu_fp.h>

void cpu_vcpu_fp_reset(struct vmm_vcpu *vcpu)
{
	riscv_regs(vcpu)->sstatus &= ~SSTATUS_FS;
	if (riscv_isa_extension_available(riscv_priv(vcpu)->isa, f) ||
	    riscv_isa_extension_available(riscv_priv(vcpu)->isa, d))
		riscv_regs(vcpu)->sstatus |= SSTATUS_FS_INITIAL;
	else
		riscv_regs(vcpu)->sstatus |= SSTATUS_FS_OFF;
	memset(&riscv_priv(vcpu)->fp, 0, sizeof(riscv_priv(vcpu)->fp));
}

static inline void cpu_vcpu_fp_clean(arch_regs_t *regs)
{
	regs->sstatus &= ~SSTATUS_FS;
	regs->sstatus |= SSTATUS_FS_CLEAN;
}

static inline void cpu_vcpu_fp_force_save(struct vmm_vcpu *vcpu)
{
	unsigned long *isa = riscv_priv(vcpu)->isa;

	if (riscv_isa_extension_available(isa, d))
		__cpu_vcpu_fp_d_save(&riscv_priv(vcpu)->fp.d);
	else if (riscv_isa_extension_available(isa, f))
		__cpu_vcpu_fp_f_save(&riscv_priv(vcpu)->fp.f);
}

static inline void cpu_vcpu_fp_force_restore(struct vmm_vcpu *vcpu)
{
	unsigned long *isa = riscv_priv(vcpu)->isa;

	if (riscv_isa_extension_available(isa, d))
		__cpu_vcpu_fp_d_restore(&riscv_priv(vcpu)->fp.d);
	else if (riscv_isa_extension_available(isa, f))
		__cpu_vcpu_fp_f_restore(&riscv_priv(vcpu)->fp.f);
}

void cpu_vcpu_fp_save(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	if (riscv_nested_virt(vcpu)) {
		/* Always save FP state when nested virtualization is ON */
		cpu_vcpu_fp_force_save(vcpu);
	} else {
		/* Lazy save FP state when nested virtualization is OFF */
		if ((regs->sstatus & SSTATUS_FS) == SSTATUS_FS_DIRTY) {
			cpu_vcpu_fp_force_save(vcpu);
			cpu_vcpu_fp_clean(regs);
		}
	}
}

void cpu_vcpu_fp_restore(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	if (riscv_nested_virt(vcpu)) {
		/* Always restore FP state when nested virtualization is ON */
		cpu_vcpu_fp_force_restore(vcpu);
	} else {
		/* Lazy restore FP state when nested virtualization is OFF */
		if ((regs->sstatus & SSTATUS_FS) != SSTATUS_FS_OFF) {
			cpu_vcpu_fp_force_restore(vcpu);
			cpu_vcpu_fp_clean(regs);
		}
	}
}

void cpu_vcpu_fp_dump_regs(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	int i;
	struct riscv_priv *priv = riscv_priv(vcpu);

	if (!riscv_isa_extension_available(priv->isa, f) &&
	    !riscv_isa_extension_available(priv->isa, d))
		return;

	vmm_cprintf(cdev, "\n");
	vmm_cprintf(cdev, "           fcsr=0x%08x\n", priv->fp.d.fcsr);
	for (i = 0; i < array_size(priv->fp.d.f) / 2; i++) {
		vmm_cprintf(cdev, "            f%02d=0x%016"PRIx64
				  "         f%02d=0x%016"PRIx64"\n",
				(2*i), priv->fp.d.f[2*i],
				(2*i + 1), priv->fp.d.f[2*i + 1]);
	}
}
