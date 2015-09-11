/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file vgic_v2.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief GICv2 ops for Hardware assisted GICv2 emulator.
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <arch_regs.h>
#include <arch_atomic.h>
#include <libs/bitmap.h>

#include <vgic.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define GICC2_DIR			0x0

#define GICH_HCR			0x0
#define GICH_VTR			0x4
#define GICH_VMCR			0x8
#define GICH_MISR			0x10
#define GICH_EISR0 			0x20
#define GICH_EISR1 			0x24
#define GICH_ELRSR0 			0x30
#define GICH_ELRSR1 			0x34
#define GICH_APR			0xf0
#define GICH_LR0			0x100

#define GICH_HCR_EN			(1 << 0)
#define GICH_HCR_UIE			(1 << 1)

#define GICH_VTR_LRCNT_MASK		0x3f

#define GICH_LR_MAX_COUNT		0x40

#define GICH_LR_HW			(1 << 31)
#define GICH_LR_STATE			(3 << 28)
#define GICH_LR_PENDING			(1 << 28)
#define GICH_LR_ACTIVE			(1 << 29)
#define GICH_LR_PRIO_SHIFT		(23)
#define GICH_LR_PRIO			(0x1F << GICH_LR_PRIO_SHIFT)
#define GICH_LR_PHYSID_SHIFT		(10)
#define GICH_LR_PHYSID			(0x3ff << GICH_LR_PHYSID_SHIFT)
#define GICH_LR_PHYSID_EOI_SHIFT	(19)
#define GICH_LR_PHYSID_EOI		(1 << GICH_LR_PHYSID_EOI_SHIFT)
#define GICH_LR_PHYSID_CPUID_SHIFT	(10)
#define GICH_LR_PHYSID_CPUID		(7 << GICH_LR_PHYSID_CPUID_SHIFT)
#define GICH_LR_VIRTUALID		(0x3ff << 0)

#define GICH_MISR_EOI			(1 << 0)
#define GICH_MISR_U			(1 << 1)

struct vgic_v2_priv {
	bool cpu2_mapped;
	physical_addr_t cpu_pa;
	virtual_addr_t  cpu_va;
	physical_addr_t cpu2_pa;
	virtual_addr_t  cpu2_va;
	physical_addr_t hctrl_pa;
	virtual_addr_t  hctrl_va;
	physical_addr_t vcpu_pa;
	physical_size_t vcpu_sz;
	virtual_addr_t  vcpu_va;
	u32 maint_irq;
	u32 lr_cnt;
};

static struct vgic_v2_priv vgicp;

void vgic_v2_reset_state(struct vgic_hw_state *hw)
{
	u32 i, hirq;
	for (i = 0 ; i < vgicp.lr_cnt; i++) {
		if ((hw->v2.lr[i] & GICH_LR_HW) &&
		    (hw->v2.lr[i] & GICH_LR_PENDING)) {
			hirq = hw->v2.lr[i] & GICH_LR_PHYSID;
			hirq = hirq >> GICH_LR_PHYSID_SHIFT;
			vmm_writel_relaxed(hirq,
					   (void *)vgicp.cpu2_va + GICC2_DIR);
		}
	}
	hw->v2.hcr = GICH_HCR_EN;
	hw->v2.vmcr = 0;
	hw->v2.apr = 0;
	for (i = 0; i < vgicp.lr_cnt; i++) {
		hw->v2.lr[i] = 0x0;
	}
}

void vgic_v2_save_state(struct vgic_hw_state *hw)
{
	u32 i;

	hw->v2.hcr = vmm_readl_relaxed((void *)vgicp.hctrl_va + GICH_HCR);
	hw->v2.vmcr = vmm_readl_relaxed((void *)vgicp.hctrl_va + GICH_VMCR);
	hw->v2.apr = vmm_readl_relaxed((void *)vgicp.hctrl_va + GICH_APR);
	vmm_writel_relaxed(0x0, (void *)vgicp.hctrl_va + GICH_HCR);
	for (i = 0; i < vgicp.lr_cnt; i++) {
		hw->v2.lr[i] =
		vmm_readl_relaxed((void *)vgicp.hctrl_va + GICH_LR0 + 4*i);
	}
}

void vgic_v2_restore_state(struct vgic_hw_state *hw)
{
	u32 i;

	vmm_writel_relaxed(hw->v2.hcr, (void *)vgicp.hctrl_va + GICH_HCR);
	vmm_writel_relaxed(hw->v2.vmcr, (void *)vgicp.hctrl_va + GICH_VMCR);
	vmm_writel_relaxed(hw->v2.apr, (void *)vgicp.hctrl_va + GICH_APR);
	for (i = 0; i < vgicp.lr_cnt; i++) {
		vmm_writel_relaxed(hw->v2.lr[i],
				   (void *)vgicp.hctrl_va + GICH_LR0 + 4*i);
	}
}

bool vgic_v2_check_underflow(void)
{
	u32 misr;
	misr = vmm_readl_relaxed((void *)vgicp.hctrl_va + GICH_MISR);
	return (misr & GICH_MISR_U) ? TRUE : FALSE;
}

void vgic_v2_enable_underflow(void)
{
	u32 hcr;
	hcr = vmm_readl_relaxed((void *)vgicp.hctrl_va + GICH_HCR);
	hcr |= GICH_HCR_UIE;
	vmm_writel_relaxed(hcr, (void *)vgicp.hctrl_va + GICH_HCR);
}

void vgic_v2_disable_underflow(void)
{
	u32 hcr;
	hcr = vmm_readl_relaxed((void *)vgicp.hctrl_va + GICH_HCR);
	hcr &= ~GICH_HCR_UIE;
	vmm_writel_relaxed(hcr, (void *)vgicp.hctrl_va + GICH_HCR);
}

void vgic_v2_read_elrsr(u32 *elrsr0, u32 *elrsr1)
{
	*elrsr0 = vmm_readl_relaxed((void *)vgicp.hctrl_va + GICH_ELRSR0);
	if (32 < vgicp.lr_cnt) {
		*elrsr1 =
		vmm_readl_relaxed((void *)vgicp.hctrl_va + GICH_ELRSR1);
	} else {
		*elrsr1 = 0x0;
	}
}

void vgic_v2_set_lr(u32 lr, struct vgic_lr *lrv)
{
	u32 lrval = lrv->virtid & GICH_LR_VIRTUALID;

	lrval |= (lrv->prio << GICH_LR_PRIO_SHIFT) & GICH_LR_PRIO;

	if (lrv->flags & VGIC_LR_STATE_PENDING) {
		lrval |= GICH_LR_PENDING;
	}
	if (lrv->flags & VGIC_LR_STATE_ACTIVE) {
		lrval |= GICH_LR_ACTIVE;
	}
	if (lrv->flags & VGIC_LR_HW) {
		lrval |= GICH_LR_HW;
		lrval |= (lrv->physid << GICH_LR_PHYSID_SHIFT) &
							GICH_LR_PHYSID;
	} else {
		if (lrv->flags & VGIC_LR_EOI_INT) {
			lrval |= GICH_LR_PHYSID_EOI;
		}
		lrval |= (lrv->cpuid << GICH_LR_PHYSID_CPUID_SHIFT) &
							GICH_LR_PHYSID_CPUID;
	}

	DPRINTF("%s: LR%d = 0x%08x\n", __func__, lr, lrval);

	vmm_writel_relaxed(lrval, (void *)vgicp.hctrl_va + GICH_LR0 + 4*lr);
}

void vgic_v2_get_lr(u32 lr, struct vgic_lr *lrv)
{
	u32 lrval;

	lrval = vmm_readl_relaxed((void *)vgicp.hctrl_va + GICH_LR0 + 4*lr);

	DPRINTF("%s: LR%d = 0x%08x\n", __func__, lr, lrval);

	lrv->virtid = lrval & GICH_LR_VIRTUALID;
	lrv->physid = 0;
	lrv->cpuid = 0;
	lrv->prio = (lrval & GICH_LR_PRIO) >> GICH_LR_PRIO_SHIFT;
	lrv->flags = 0;

	if (lrval & GICH_LR_PENDING) {
		lrv->flags |= VGIC_LR_STATE_PENDING;
	}
	if (lrval & GICH_LR_ACTIVE) {
		lrv->flags |= VGIC_LR_STATE_ACTIVE;
	}
	if (lrval & GICH_LR_HW) {
		lrv->flags |= VGIC_LR_HW;
		lrv->physid = (lrval & GICH_LR_PHYSID) >>
						GICH_LR_PHYSID_SHIFT;
	} else {
		if (lrval & GICH_LR_PHYSID_EOI) {
			lrv->flags |= VGIC_LR_EOI_INT;
		}
		lrv->cpuid = (lrval & GICH_LR_PHYSID_CPUID) >>
						GICH_LR_PHYSID_CPUID_SHIFT;
	}
}

void vgic_v2_clear_lr(u32 lr)
{
	DPRINTF("%s: LR%d\n", __func__, lr);

	vmm_writel_relaxed(0x0, (void *)vgicp.hctrl_va + GICH_LR0 + 4*lr);
}

static const struct vmm_devtree_nodeid vgic_host_match[] = {
	{ .compatible	= "arm,cortex-a15-gic",	},
	{},
};

int vgic_v2_probe(struct vgic_ops *ops, struct vgic_params *params)
{
	int rc;
	struct vmm_devtree_node *node;

	node = vmm_devtree_find_matching(NULL, vgic_host_match);
	if (!node) {
		rc = VMM_ENODEV;
		goto fail;
	}

	rc = vmm_devtree_regaddr(node, &vgicp.cpu_pa, 1);
	if (rc) {
		goto fail_dref;
	}

	rc = vmm_devtree_regmap(node, &vgicp.cpu_va, 1);
	if (rc) {
		goto fail_dref;
	}

	rc = vmm_devtree_regaddr(node, &vgicp.cpu2_pa, 4);
	if (!rc) {
		vgicp.cpu2_mapped = TRUE;
		rc = vmm_devtree_regmap(node, &vgicp.cpu2_va, 4);
		if (rc) {
			goto fail_unmap_cpu;
		}
	} else {
		vgicp.cpu2_mapped = FALSE;
		vgicp.cpu2_va = vgicp.cpu_va + 0x1000;
	}

	rc = vmm_devtree_regaddr(node, &vgicp.hctrl_pa, 2);
	if (rc) {
		goto fail_unmap_cpu2;
	}

	rc = vmm_devtree_request_regmap(node, &vgicp.hctrl_va, 2,
					"GIC HCTRL");
	if (rc) {
		goto fail_unmap_cpu2;
	}

	rc = vmm_devtree_regaddr(node, &vgicp.vcpu_pa, 3);
	if (rc) {
		goto fail_unmap_hctrl;
	}

	rc = vmm_devtree_regsize(node, &vgicp.vcpu_sz, 3);
	if (rc) {
		goto fail_unmap_hctrl;
	}

	rc = vmm_devtree_request_regmap(node, &vgicp.vcpu_va, 3,
					"GIC VCPU");
	if (rc) {
		goto fail_unmap_hctrl;
	}

	vgicp.maint_irq = vmm_devtree_irq_parse_map(node, 0);
	if (!vgicp.maint_irq) {
		rc = VMM_ENODEV;
		goto fail_unmap_vcpu;
	}

	vgicp.lr_cnt = vmm_readl_relaxed((void *)vgicp.hctrl_va + GICH_VTR);
	vgicp.lr_cnt = (vgicp.lr_cnt & GICH_VTR_LRCNT_MASK) + 1;

	vmm_devtree_dref_node(node);

	params->type = VGIC_V2;
	params->vcpu_pa = vgicp.vcpu_pa;
	params->maint_irq = vgicp.maint_irq;
	params->lr_cnt = vgicp.lr_cnt;

	ops->reset_state = vgic_v2_reset_state;
	ops->save_state = vgic_v2_save_state;
	ops->restore_state = vgic_v2_restore_state;
	ops->check_underflow = vgic_v2_check_underflow;
	ops->enable_underflow = vgic_v2_enable_underflow;
	ops->disable_underflow = vgic_v2_disable_underflow;
	ops->read_elrsr = vgic_v2_read_elrsr;
	ops->set_lr = vgic_v2_set_lr;
	ops->get_lr = vgic_v2_get_lr;
	ops->clear_lr = vgic_v2_clear_lr;

	vmm_printf("vgic_v2: hctrl=0x%lx vcpu=0x%lx\n",
		   (unsigned long)vgicp.hctrl_pa,
		   (unsigned long)vgicp.vcpu_pa);
	vmm_printf("vgic_v2: lr_cnt=%d maint_irq=%d\n",
		   vgicp.lr_cnt, vgicp.maint_irq);

	return VMM_OK;

fail_unmap_vcpu:
	vmm_devtree_regunmap_release(node, vgicp.vcpu_va, 3);
fail_unmap_hctrl:
	vmm_devtree_regunmap_release(node, vgicp.hctrl_va, 2);
fail_unmap_cpu2:
	if (vgicp.cpu2_mapped) {
		vmm_devtree_regunmap(node, vgicp.cpu2_va, 4);
	}
fail_unmap_cpu:
	vmm_devtree_regunmap(node, vgicp.cpu_va, 1);
fail_dref:
	vmm_devtree_dref_node(node);
fail:
	return rc;
}

void vgic_v2_remove(struct vgic_ops *ops, struct vgic_params *params)
{
	struct vmm_devtree_node *node;

	node = vmm_devtree_find_matching(NULL, vgic_host_match);
	if (!node) {
		return;
	}

	vmm_devtree_regunmap(node, vgicp.vcpu_va, 3);

	vmm_devtree_regunmap(node, vgicp.hctrl_va, 2);

	if (vgicp.cpu2_mapped) {
		vmm_devtree_regunmap(node, vgicp.cpu2_va, 4);
	}

	vmm_devtree_regunmap(node, vgicp.cpu_va, 1);

	vmm_devtree_dref_node(node);
}
