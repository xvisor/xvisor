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
 * @file cpu_vcpu_rvv.c
 * @author Jaromir Mikusik
 * @brief source of VCPU RVV functions
 */

#include <vmm_stdio.h>
#include <libs/stringlib.h>

#include <cpu_hwcap.h>
#include <cpu_vcpu_switch.h>
#include <cpu_vcpu_rvv.h>

void cpu_vcpu_rvv_reset(struct vmm_vcpu *vcpu)
{
	riscv_regs(vcpu)->sstatus &= ~SSTATUS_VS;
	if (riscv_isa_extension_available(riscv_priv(vcpu)->isa, v))
		riscv_regs(vcpu)->sstatus |= SSTATUS_VS_INITIAL;
	else
		riscv_regs(vcpu)->sstatus |= SSTATUS_VS_OFF;
	memset(&riscv_priv(vcpu)->rvv, 0, sizeof(riscv_priv(vcpu)->rvv));
}

static inline void cpu_vcpu_rvv_clean(arch_regs_t *regs)
{
	regs->sstatus &= ~SSTATUS_VS;
	regs->sstatus |= SSTATUS_VS_CLEAN;
}

static inline void cpu_vcpu_rvv_force_save(struct vmm_vcpu *vcpu)
{
	unsigned long *isa = riscv_priv(vcpu)->isa;

	if (riscv_isa_extension_available(isa, v))
		__cpu_vcpu_rvv_save(&riscv_priv(vcpu)->rvv);
}

static inline void cpu_vcpu_rvv_force_restore(struct vmm_vcpu *vcpu)
{
	unsigned long *isa = riscv_priv(vcpu)->isa;

	if (riscv_isa_extension_available(isa, v))
		__cpu_vcpu_rvv_restore(&riscv_priv(vcpu)->rvv);
}

void cpu_vcpu_rvv_save(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	if (riscv_nested_virt(vcpu)) {
		/* Always save RVV state when nested virtualization is ON */
		cpu_vcpu_rvv_force_save(vcpu);
	} else {
		/* Lazy save RVV state when nested virtualization is OFF */
		if ((regs->sstatus & SSTATUS_VS) == SSTATUS_VS_DIRTY) {
			cpu_vcpu_rvv_force_save(vcpu);
			cpu_vcpu_rvv_clean(regs);
		}
	}
}

void cpu_vcpu_rvv_restore(struct vmm_vcpu *vcpu, arch_regs_t *regs)
{
	if (riscv_nested_virt(vcpu)) {
		/* Always restore RVV state when nested virtualization is ON */
		cpu_vcpu_rvv_force_restore(vcpu);
	} else {
		/* Lazy restore RVV state when nested virtualization is OFF */
		if ((regs->sstatus & SSTATUS_VS) != SSTATUS_VS_OFF) {
			cpu_vcpu_rvv_force_restore(vcpu);
			cpu_vcpu_rvv_clean(regs);
		}
	}
}

void cpu_vcpu_rvv_dump_regs(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	struct riscv_priv *priv = riscv_priv(vcpu);

	if (!riscv_isa_extension_available(priv->isa, v))
		return;

	vmm_cprintf(cdev, "\n");
	vmm_cprintf(cdev, "           vtype =0x%016lx\n", priv->rvv.vtype);
	vmm_cprintf(cdev, "           vl    =0x%016lx\n", priv->rvv.vl);
	vmm_cprintf(cdev, "           vstart=0x%016lx\n", priv->rvv.vstart);
	vmm_cprintf(cdev, "           vcsr  =0x%016lx\n", priv->rvv.vcsr);
    for (unsigned int vregn = 0; vregn < 32; vregn++){
        vmm_cprintf(cdev, "            v%02d=0x", vregn);
        for (unsigned int i = 0; i < RVV_VREG_LEN_U64; i++) {
            unsigned int arr_idx = vregn * RVV_VREG_LEN_U64 + i;
            vmm_cprintf(cdev, "%016lx", priv->rvv.v[arr_idx]);
        }
        vmm_cprintf(cdev, "\n");
    }
}
