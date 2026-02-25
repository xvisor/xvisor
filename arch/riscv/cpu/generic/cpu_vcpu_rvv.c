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

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <libs/stringlib.h>

#include <cpu_hwcap.h>
#include <cpu_vcpu_switch.h>
#include <cpu_vcpu_rvv.h>


// create dummy functions when RVV is enabled as the real ones
// will not compile and because the riscv_isa_extension_available(isa, v)
// will always return false, no context switch happens anyways
//
// this way we don't have to "pollute" other code with ifdefs
#ifndef RVV_ENABLED
void __cpu_vcpu_rvv_save(struct riscv_priv_rvv *rvv){}
void __cpu_vcpu_rvv_restore(struct riscv_priv_rvv *rvv){}
#endif

void cpu_vcpu_rvv_reset(struct vmm_vcpu *vcpu)
{
    struct riscv_priv_rvv *rvv = &riscv_priv(vcpu)->rvv;

	riscv_regs(vcpu)->sstatus &= ~SSTATUS_VS;
	if (riscv_isa_extension_available(riscv_priv(vcpu)->isa, v))
		riscv_regs(vcpu)->sstatus |= SSTATUS_VS_INITIAL;
	else
		riscv_regs(vcpu)->sstatus |= SSTATUS_VS_OFF;

	/* Zero only CSR fields, preserve vlenb and v pointer */
    rvv->vtype  = 0;
    rvv->vl     = 0;
    rvv->vxrm   = 0;
    rvv->vxsat  = 0;
    rvv->vstart = 0;
    rvv->vcsr   = 0;

    /* Zero vector registers if allocated */
    if (rvv->v)
        memset(rvv->v, 0, 32 * rvv->vlenb);
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

int cpu_vcpu_rvv_init(struct vmm_vcpu *vcpu)
{
    struct riscv_priv_rvv *rvv = &riscv_priv(vcpu)->rvv;

    if (!riscv_isa_extension_available(riscv_priv(vcpu)->isa, v))
        return VMM_OK;

    /* Read actual hardware vlenb */
    rvv->vlenb = csr_read(CSR_VLENB);

    /* Allocate 32 * vlenb bytes for vector registers */
    rvv->v = vmm_zalloc(32 * rvv->vlenb);
    if (!rvv->v)
        return VMM_ENOMEM;

    return VMM_OK;
}

void cpu_vcpu_rvv_deinit(struct vmm_vcpu *vcpu)
{
    struct riscv_priv_rvv *rvv = &riscv_priv(vcpu)->rvv;

    if (rvv->v) {
        vmm_free(rvv->v);
        rvv->v = NULL;
    }
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
	if (!priv->rvv.v)
		return;

	vmm_cprintf(cdev, "\n");
	vmm_cprintf(cdev, "           vtype =0x%"PRIADDR"\n", priv->rvv.vtype);
	vmm_cprintf(cdev, "           vl    =0x%"PRIADDR"\n", priv->rvv.vl);
	vmm_cprintf(cdev, "           vstart=0x%"PRIADDR"\n", priv->rvv.vstart);
	vmm_cprintf(cdev, "           vcsr  =0x%"PRIADDR"\n", priv->rvv.vcsr);
	vmm_cprintf(cdev, "           vlenb =0x%"PRIADDR"\n", priv->rvv.vlenb);

	for (unsigned int vregn = 0; vregn < 32; vregn++) {
		vmm_cprintf(cdev, "            v%02d=0x", vregn);
		u8 *vreg = priv->rvv.v + vregn * priv->rvv.vlenb;
		for (unsigned int i = 0; i < priv->rvv.vlenb; i++) {
			vmm_cprintf(cdev, "%02x", vreg[i]);
		}
		vmm_cprintf(cdev, "\n");
	}
}
