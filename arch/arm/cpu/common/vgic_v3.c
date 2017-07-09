/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vgic_v3.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief GICv3 ops for Hardware assisted GICv2 emulator.
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <arch_gicv3.h>
#include <drv/irqchip/arm-gic-v3.h>

#include <vgic.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define vtr_to_max_lr_idx(v)		((v) & 0xf)
#define vtr_to_nr_pri_bits(v)		(((u32)(v) >> 29) + 1)

struct vgic_v3_priv {
	bool vcpu_mapped;
	physical_addr_t vcpu_pa;
	physical_size_t vcpu_sz;
	virtual_addr_t  vcpu_va;
	u32 maint_irq;
	u32 lr_cnt;
	u32 pri_bits;
};

static struct vgic_v3_priv vgicp;

static u64 vgic_v3_read_lr(u32 lr)
{
	switch (lr) {
	case 0:
		return arch_gic_read_sysreg(ICH_LR0_EL2);
	case 1:
		return arch_gic_read_sysreg(ICH_LR1_EL2);
	case 2:
		return arch_gic_read_sysreg(ICH_LR2_EL2);
	case 3:
		return arch_gic_read_sysreg(ICH_LR3_EL2);
	case 4:
		return arch_gic_read_sysreg(ICH_LR4_EL2);
	case 5:
		return arch_gic_read_sysreg(ICH_LR5_EL2);
	case 6:
		return arch_gic_read_sysreg(ICH_LR6_EL2);
	case 7:
		return arch_gic_read_sysreg(ICH_LR7_EL2);
	case 8:
		return arch_gic_read_sysreg(ICH_LR8_EL2);
	case 9:
		return arch_gic_read_sysreg(ICH_LR9_EL2);
	case 10:
		return arch_gic_read_sysreg(ICH_LR10_EL2);
	case 11:
		return arch_gic_read_sysreg(ICH_LR11_EL2);
	case 12:
		return arch_gic_read_sysreg(ICH_LR12_EL2);
	case 13:
		return arch_gic_read_sysreg(ICH_LR13_EL2);
	case 14:
		return arch_gic_read_sysreg(ICH_LR14_EL2);
	case 15:
		return arch_gic_read_sysreg(ICH_LR15_EL2);
	default:
		DPRINTF("%s: LR%d is invalid\n", __func__, lr);
		break;
	};

	return 0x0;
}

static void vgic_v3_write_lr(u32 lr, u64 val)
{
	switch (lr) {
	case 0:
		arch_gic_write_sysreg(val, ICH_LR0_EL2);
		return;
	case 1:
		arch_gic_write_sysreg(val, ICH_LR1_EL2);
		return;
	case 2:
		arch_gic_write_sysreg(val, ICH_LR2_EL2);
		return;
	case 3:
		arch_gic_write_sysreg(val, ICH_LR3_EL2);
		return;
	case 4:
		arch_gic_write_sysreg(val, ICH_LR4_EL2);
		return;
	case 5:
		arch_gic_write_sysreg(val, ICH_LR5_EL2);
		return;
	case 6:
		arch_gic_write_sysreg(val, ICH_LR6_EL2);
		return;
	case 7:
		arch_gic_write_sysreg(val, ICH_LR7_EL2);
		return;
	case 8:
		arch_gic_write_sysreg(val, ICH_LR8_EL2);
		return;
	case 9:
		arch_gic_write_sysreg(val, ICH_LR9_EL2);
		return;
	case 10:
		arch_gic_write_sysreg(val, ICH_LR10_EL2);
		return;
	case 11:
		arch_gic_write_sysreg(val, ICH_LR11_EL2);
		return;
	case 12:
		arch_gic_write_sysreg(val, ICH_LR12_EL2);
		return;
	case 13:
		arch_gic_write_sysreg(val, ICH_LR13_EL2);
		return;
	case 14:
		arch_gic_write_sysreg(val, ICH_LR14_EL2);
		return;
	case 15:
		arch_gic_write_sysreg(val, ICH_LR15_EL2);
		return;
	default:
		DPRINTF("%s: LR%d is invalid\n", __func__, lr);
		return;
	};
}

static void vgic_v3_reset_state(struct vgic_hw_state *hw,
				enum vgic_model_type model)
{
	u32 i;

	hw->v3.hcr = ICH_HCR_EN;
	hw->v3.vmcr = 0;

	for (i = 0; i < array_size(hw->v3.ap0r); i++) {
		hw->v3.ap0r[i] = 0x0;
	}

	for (i = 0; i < array_size(hw->v3.ap1r); i++) {
		hw->v3.ap1r[i] = 0x0;
	}

	for (i = 0; i < vgicp.lr_cnt; i++) {
		hw->v3.lr[i] = 0x0;
	}
}

static void vgic_v3_save_state(struct vgic_hw_state *hw,
			       enum vgic_model_type model)
{
	u32 i;

	/*
	 * Make sure stores to the GIC via the memory mapped interface
	 * are now visible to the system register interface.
	 */
	if (model == VGIC_MODEL_V2)
		dsb(st);

	hw->v3.hcr = arch_gic_read_sysreg(ICH_HCR_EL2);
	arch_gic_write_sysreg(0x0, ICH_HCR_EL2);

	hw->v3.vmcr = arch_gic_read_sysreg(ICH_VMCR_EL2);

	switch (vgicp.pri_bits) {
	case 7:
		hw->v3.ap0r[3] = arch_gic_read_sysreg(ICH_AP0R3_EL2);
		hw->v3.ap0r[2] = arch_gic_read_sysreg(ICH_AP0R2_EL2);
	case 6:
		hw->v3.ap0r[1] = arch_gic_read_sysreg(ICH_AP0R1_EL2);
	default:
		hw->v3.ap0r[0] = arch_gic_read_sysreg(ICH_AP0R0_EL2);
	};

	switch (vgicp.pri_bits) {
	case 7:
		hw->v3.ap1r[3] = arch_gic_read_sysreg(ICH_AP1R3_EL2);
		hw->v3.ap1r[2] = arch_gic_read_sysreg(ICH_AP1R2_EL2);
	case 6:
		hw->v3.ap1r[1] = arch_gic_read_sysreg(ICH_AP1R1_EL2);
	default:
		hw->v3.ap1r[0] = arch_gic_read_sysreg(ICH_AP1R0_EL2);
	};

	for (i = 0; i < vgicp.lr_cnt; i++) {
		hw->v3.lr[i] = vgic_v3_read_lr(i);
	}
}

static void vgic_v3_restore_state(struct vgic_hw_state *hw,
				  enum vgic_model_type model)
{
	u32 i;

	/*
	 * VFIQEn is RES1 if ICC_SRE_EL1.SRE is 1. This causes a
	 * Group0 interrupt (as generated in GICv2 mode) to be
	 * delivered as a FIQ to the guest, with potentially fatal
	 * consequences. So we must make sure that ICC_SRE_EL1 has
	 * been actually programmed with the value we want before
	 * starting to mess with the rest of the GIC.
	 */
	if (model == VGIC_MODEL_V2) {
		arch_gic_write_sysreg(0, ICC_SRE_EL1);
	} else {
		arch_gic_write_sysreg(1, ICC_SRE_EL1);
	}
	isb();

	arch_gic_write_sysreg(hw->v3.hcr, ICH_HCR_EL2);
	arch_gic_write_sysreg(hw->v3.vmcr, ICH_VMCR_EL2);

	switch (vgicp.pri_bits) {
	case 7:
		arch_gic_write_sysreg(hw->v3.ap0r[3], ICH_AP0R3_EL2);
		arch_gic_write_sysreg(hw->v3.ap0r[2], ICH_AP0R2_EL2);
	case 6:
		arch_gic_write_sysreg(hw->v3.ap0r[1], ICH_AP0R1_EL2);
	default:
		arch_gic_write_sysreg(hw->v3.ap0r[0], ICH_AP0R0_EL2);
	};

	switch (vgicp.pri_bits) {
	case 7:
		arch_gic_write_sysreg(hw->v3.ap1r[3], ICH_AP1R3_EL2);
		arch_gic_write_sysreg(hw->v3.ap1r[2], ICH_AP1R2_EL2);
	case 6:
		arch_gic_write_sysreg(hw->v3.ap1r[1], ICH_AP1R1_EL2);
	default:
		arch_gic_write_sysreg(hw->v3.ap1r[0], ICH_AP1R0_EL2);
	};

	for (i = 0; i < vgicp.lr_cnt; i++) {
		vgic_v3_write_lr(i, hw->v3.lr[i]);
	}

	/*
	 * Ensures that the above will have reached the
	 * (re)distributors. This ensure the guest will read the
	 * correct values from the memory-mapped interface.
	 */
	if (model == VGIC_MODEL_V2) {
		isb();
		dsb(sy);
	}
}

static bool vgic_v3_check_underflow(void)
{
	u32 misr = arch_gic_read_sysreg(ICH_MISR_EL2);
	return (misr & ICH_MISR_U) ? TRUE : FALSE;
}

static void vgic_v3_enable_underflow(void)
{
	u32 hcr = arch_gic_read_sysreg(ICH_HCR_EL2);
	arch_gic_write_sysreg(hcr | ICH_HCR_UIE, ICH_HCR_EL2);
}

static void vgic_v3_disable_underflow(void)
{
	u32 hcr = arch_gic_read_sysreg(ICH_HCR_EL2);
	arch_gic_write_sysreg(hcr & ~ICH_HCR_UIE, ICH_HCR_EL2);
}

static void vgic_v3_read_elrsr(u32 *elrsr0, u32 *elrsr1)
{
	*elrsr0 = arch_gic_read_sysreg(ICH_ELSR_EL2);
	*elrsr1 = 0x0;
}

static void vgic_v3_set_lr(u32 lr, struct vgic_lr *lrv,
			   enum vgic_model_type model)
{
	u64 lrval = 0;

	if (model == VGIC_MODEL_V2) {
		lrval |= lrv->virtid & GICH_LR_VIRTUALID;
	} else {
		lrval |= lrv->virtid & ICH_LR_VIRTUAL_ID_MASK;
	}

	lrval |= ((u64)lrv->prio << ICH_LR_PRIORITY_SHIFT);

	if (lrv->flags & VGIC_LR_STATE_PENDING) {
		lrval |= ICH_LR_PENDING_BIT;
	}
	if (lrv->flags & VGIC_LR_STATE_ACTIVE) {
		lrval |= ICH_LR_ACTIVE_BIT;
	}
	if (lrv->flags & VGIC_LR_HW) {
		lrval |= ICH_LR_HW;
		lrval |= ((u64)lrv->physid << ICH_LR_PHYS_ID_SHIFT) &
						ICH_LR_PHYS_ID_MASK;
	} else {
		if (lrv->flags & VGIC_LR_EOI_INT) {
			lrval |= ICH_LR_EOI;
		}
		if (model == VGIC_MODEL_V2) {
			lrval |= ((u64)lrv->cpuid << GICH_LR_PHYSID_CPUID_SHIFT) &
							GICH_LR_PHYSID_CPUID;
		}
	}

	/*
	 * We currently only support Group1 interrupts, which is a
	 * known defect. This needs to be addressed at some point.
	 */
	if (model == VGIC_MODEL_V3) {
		lrval |= ICH_LR_GROUP;
	}

	DPRINTF("%s: LR%d = 0x%"PRIx64"\n", __func__, lr, lrval);

	vgic_v3_write_lr(lr, lrval);
}

static void vgic_v3_get_lr(u32 lr, struct vgic_lr *lrv,
			   enum vgic_model_type model)
{
	u64 lrval = vgic_v3_read_lr(lr);

	DPRINTF("%s: LR%d = 0x%"PRIx64"\n", __func__, lr, lrval);

	if (model == VGIC_MODEL_V2) {
		lrv->virtid = lrval & GICH_LR_VIRTUALID;
	} else {
		lrv->virtid = lrval & ICH_LR_VIRTUAL_ID_MASK;
	}
	lrv->physid = 0;
	lrv->cpuid = 0;
	lrv->prio = (lrval >> ICH_LR_PRIORITY_SHIFT) & 0xFF;
	lrv->flags = 0;

	if (lrval & ICH_LR_PENDING_BIT) {
		lrv->flags |= VGIC_LR_STATE_PENDING;
	}
	if (lrval & ICH_LR_ACTIVE_BIT) {
		lrv->flags |= VGIC_LR_STATE_ACTIVE;
	}
	if (lrval & ICH_LR_HW) {
		lrv->flags |= VGIC_LR_HW;
		lrv->physid = (lrval & ICH_LR_PHYS_ID_MASK) >>
						ICH_LR_PHYS_ID_SHIFT;
	} else {
		if (lrval & ICH_LR_EOI) {
			lrv->flags |= VGIC_LR_EOI_INT;
		}
		if (model == VGIC_MODEL_V2) {
			lrv->cpuid = (lrval & GICH_LR_PHYSID_CPUID) >>
						GICH_LR_PHYSID_CPUID_SHIFT;
		}
	}
}

static void vgic_v3_clear_lr(u32 lr)
{
	DPRINTF("%s: LR%d\n", __func__, lr);

	vgic_v3_write_lr(lr, 0x0);
}

static const struct vmm_devtree_nodeid vgic_host_match[] = {
	{ .compatible = "arm,gic-v3", },
	{},
};

int vgic_v3_probe(struct vgic_ops *ops, struct vgic_params *params)
{
	int rc;
	u64 val;
	struct vmm_devtree_node *node;

	node = vmm_devtree_find_matching(NULL, vgic_host_match);
	if (!node) {
		rc = VMM_ENODEV;
		goto fail;
	}

	vgicp.vcpu_mapped = FALSE;

	rc = vmm_devtree_regaddr(node, &vgicp.vcpu_pa, 4);
	if (!rc) {
		rc = vmm_devtree_regsize(node, &vgicp.vcpu_sz, 4);
		if (rc) {
			goto fail_dref;
		}

		rc = vmm_devtree_request_regmap(node, &vgicp.vcpu_va, 4,
						"GIC VCPU");
		if (rc) {
			goto fail_dref;
		}

		vgicp.vcpu_mapped = TRUE;
	}

	vgicp.maint_irq = vmm_devtree_irq_parse_map(node, 0);
	if (!vgicp.maint_irq) {
		rc = VMM_ENODEV;
		goto fail_unmap_vcpu;
	}

	val = arch_gic_read_sysreg(ICH_VTR_EL2);
	vgicp.lr_cnt = vtr_to_max_lr_idx(val) + 1;
	vgicp.pri_bits = vtr_to_nr_pri_bits(val);

	vmm_devtree_dref_node(node);

	params->type = VGIC_V3;
	params->can_emulate_gic_v2 = vgicp.vcpu_mapped;
	params->can_emulate_gic_v3 = TRUE;
	params->vcpu_pa = vgicp.vcpu_pa;
	params->maint_irq = vgicp.maint_irq;
	params->lr_cnt = vgicp.lr_cnt;

	ops->reset_state = vgic_v3_reset_state;
	ops->save_state = vgic_v3_save_state;
	ops->restore_state = vgic_v3_restore_state;
	ops->check_underflow = vgic_v3_check_underflow;
	ops->enable_underflow = vgic_v3_enable_underflow;
	ops->disable_underflow = vgic_v3_disable_underflow;
	ops->read_elrsr = vgic_v3_read_elrsr;
	ops->set_lr = vgic_v3_set_lr;
	ops->get_lr = vgic_v3_get_lr;
	ops->clear_lr = vgic_v3_clear_lr;

	vmm_printf("vgic_v3: vcpu=0x%lx GICv2 emulation %s\n",
		   (unsigned long)vgicp.vcpu_pa,
		   (vgicp.vcpu_mapped) ? "available" : "not available");
	vmm_printf("vgic_v3: lr_cnt=%d pri_bits=%d maint_irq=%d\n",
		   vgicp.lr_cnt, vgicp.pri_bits, vgicp.maint_irq);

	return VMM_OK;

fail_unmap_vcpu:
	if (vgicp.vcpu_mapped) {
		vmm_devtree_regunmap_release(node, vgicp.vcpu_va, 3);
		vgicp.vcpu_mapped = FALSE;
	}
fail_dref:
	vmm_devtree_dref_node(node);
fail:
	return rc;
}

void vgic_v3_remove(struct vgic_ops *ops, struct vgic_params *params)
{
	struct vmm_devtree_node *node;

	node = vmm_devtree_find_matching(NULL, vgic_host_match);
	if (!node) {
		return;
	}

	if (vgicp.vcpu_mapped) {
		vmm_devtree_regunmap(node, vgicp.vcpu_va, 3);
		vgicp.vcpu_mapped = FALSE;
	}

	vmm_devtree_dref_node(node);
}
