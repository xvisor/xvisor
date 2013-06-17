/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file vgic.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Hardware assisted GICv2 emulator using GIC virt extensions.
 *
 * This source is based on GICv2 software emulator located at:
 * emulators/pic/gic.c
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_scheduler.h>
#include <vmm_vcpu_irq.h>
#include <vmm_devemu.h>
#include <vmm_modules.h>
#include <arch_regs.h>
#include <gic.h>

#define MODULE_DESC			"vGIC Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			vgic_emulator_init
#define	MODULE_EXIT			vgic_emulator_exit

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

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

#define GICH_LR_STATE			(3 << 28)
#define GICH_LR_PENDING_BIT		(1 << 28)
#define GICH_LR_ACTIVE_BIT		(1 << 29)
#define GICH_LR_PRIO_SHIFT		(23)
#define GICH_LR_PRIO			(0x1F << 23)
#define GICH_LR_EOI			(1 << 19)
#define GICH_LR_PHYSID_CPUID_SHIFT	(10)
#define GICH_LR_PHYSID_CPUID		(7 << GICH_LR_PHYSID_CPUID_SHIFT)
#define GICH_LR_VIRTUALID		(0x3ff << 0)

#define GICH_MISR_EOI			(1 << 0)
#define GICH_MISR_U			(1 << 1)

struct vgic_host_ctrl {
	bool avail;
	physical_addr_t hctrl_pa;
	virtual_addr_t  hctrl_va;
	physical_addr_t vcpu_pa;
	virtual_addr_t  vcpu_va;
	u32 maint_irq;
	u32 lr_cnt;
};

static struct vgic_host_ctrl vgich;

#define VGIC_MAX_NCPU			4
#define VGIC_MAX_NIRQ			96
#define VGIC_LR_UNKNOWN			0xFF

struct vgic_irq_state {
	u32 enabled:VGIC_MAX_NCPU;
	u32 pending:VGIC_MAX_NCPU;
	u32 active:VGIC_MAX_NCPU;
	u32 level:VGIC_MAX_NCPU;
	u32 model:1; /* 0 = N:N, 1 = 1:N */
	u32 trigger:1; /* nonzero = edge triggered.  */
};

struct vgic_vcpu_state {
	/* General Info */
	struct vmm_vcpu *vcpu;
	u32 parent_irq;

	/* Register state */
	u32 hcr;
	u32 vmcr;
	u32 apr;
	u32 lr[GICH_LR_MAX_COUNT];

	/* Maintainence Info */
	u32 lr_used[GICH_LR_MAX_COUNT / 32];
	u32 sgi_source[16];
	u8 irq_lr[VGIC_MAX_NIRQ];
};

struct vgic_guest_state {
	/* Guest to which this VGIC belongs */
	struct vmm_guest *guest;

	/* Lock to protect VGIC guest state */
	vmm_spinlock_t lock;

	/* Configuration */
	u8 id[8];
	u32 num_cpu;
	u32 num_irq;

	/* Context of each VCPU */
	struct vgic_vcpu_state vstate[VGIC_MAX_NCPU];

	/* Chip enable/disable */
	u32 enabled;

	/* Distribution Control */
	struct vgic_irq_state irq_state[VGIC_MAX_NIRQ];
	u32 irq_target[VGIC_MAX_NIRQ];
	u32 priority1[32][VGIC_MAX_NCPU];
	u32 priority2[VGIC_MAX_NIRQ - 32];
};

#define VGIC_ALL_CPU_MASK(s) ((1 << (s)->num_cpu) - 1)
#define VGIC_NUM_CPU(s) ((s)->num_cpu)
#define VGIC_NUM_IRQ(s) ((s)->num_irq)
#define VGIC_SET_ENABLED(s, irq, cm) (s)->irq_state[irq].enabled |= (cm)
#define VGIC_CLEAR_ENABLED(s, irq, cm) (s)->irq_state[irq].enabled &= ~(cm)
#define VGIC_TEST_ENABLED(s, irq, cm) ((s)->irq_state[irq].enabled & (cm))
#define VGIC_SET_PENDING(s, irq, cm) (s)->irq_state[irq].pending |= (cm)
#define VGIC_CLEAR_PENDING(s, irq, cm) (s)->irq_state[irq].pending &= ~(cm)
#define VGIC_TEST_PENDING(s, irq, cm) ((s)->irq_state[irq].pending & (cm))
#define VGIC_SET_ACTIVE(s, irq, cm) (s)->irq_state[irq].active |= (cm)
#define VGIC_CLEAR_ACTIVE(s, irq, cm) (s)->irq_state[irq].active &= ~(cm)
#define VGIC_TEST_ACTIVE(s, irq, cm) ((s)->irq_state[irq].active & (cm))
#define VGIC_SET_MODEL(s, irq) (s)->irq_state[irq].model = 1
#define VGIC_CLEAR_MODEL(s, irq) (s)->irq_state[irq].model = 0
#define VGIC_TEST_MODEL(s, irq) (s)->irq_state[irq].model
#define VGIC_SET_LEVEL(s, irq, cm) (s)->irq_state[irq].level = (cm)
#define VGIC_CLEAR_LEVEL(s, irq, cm) (s)->irq_state[irq].level &= ~(cm)
#define VGIC_TEST_LEVEL(s, irq, cm) ((s)->irq_state[irq].level & (cm))
#define VGIC_SET_TRIGGER(s, irq) (s)->irq_state[irq].trigger = 1
#define VGIC_CLEAR_TRIGGER(s, irq) (s)->irq_state[irq].trigger = 0
#define VGIC_TEST_TRIGGER(s, irq) (s)->irq_state[irq].trigger
#define VGIC_GET_PRIORITY(s, irq, cpu) \
  (((irq) < 32) ? (s)->priority1[irq][cpu] : (s)->priority2[(irq) - 32])
#define VGIC_TARGET(s, irq) (s)->irq_target[irq]

#define VGIC_TEST_EISR(eisr, lr) \
	((eisr)[((lr) >> 5) & 0x1] & (1 << ((lr) & 0x1F)))
#define VGIC_SET_EISR(vs, lr) \
	((eisr)[((lr) >> 5) & 0x1] |= (1 << ((lr) & 0x1F)))
#define VGIC_CLEAR_EISR(vs, lr) \
	((eisr)[((lr) >> 5) & 0x1] &= ~(1 << ((lr) & 0x1F)))

#define VGIC_TEST_ELRSR(elrsr, lr) \
	((elrsr)[((lr) >> 5) & 0x1] & (1 << ((lr) & 0x1F)))
#define VGIC_SET_ELRSR(vs, lr) \
	((elrsr)[((lr) >> 5) & 0x1] |= (1 << ((lr) & 0x1F)))
#define VGIC_CLEAR_ELRSR(vs, lr) \
	((elrsr)[((lr) >> 5) & 0x1] &= ~(1 << ((lr) & 0x1F)))

#define VGIC_MAKE_LR_PENDING(src, irq) \
	(GICH_LR_PENDING_BIT | \
	 ((src) << GICH_LR_PHYSID_CPUID_SHIFT) | \
	 (irq))
#define VGIC_LR_CPUID(lr_val) \
	(((lr_val) & GICH_LR_PHYSID_CPUID) >> GICH_LR_PHYSID_CPUID_SHIFT)

#define VGIC_TEST_LR_USED(vs, lr) \
	((vs)->lr_used[((lr) >> 5)] & (1 << ((lr) & 0x1F)))
#define VGIC_SET_LR_USED(vs, lr) \
	((vs)->lr_used[((lr) >> 5)] |= (1 << ((lr) & 0x1F)))
#define VGIC_CLEAR_LR_USED(vs, lr) \
	((vs)->lr_used[((lr) >> 5)] &= ~(1 << ((lr) & 0x1F)))

#define VGIC_SET_LR_MAP(vs, irq, lr) ((vs)->irq_lr[irq] = (lr))
#define VGIC_GET_LR_MAP(vs, irq) ((vs)->irq_lr[irq])

/* Save current VGIC VCPU HW state */
static void __vgic_save_vcpu_hwstate(struct vgic_vcpu_state *vs)
{
	u32 i;

	vs->hcr = vmm_readl((void *)vgich.hctrl_va + GICH_HCR);
	vs->vmcr = vmm_readl((void *)vgich.hctrl_va + GICH_VMCR);
	vs->apr = vmm_readl((void *)vgich.hctrl_va + GICH_APR);
	vmm_writel(0x0, (void *)vgich.hctrl_va + GICH_HCR);
	for (i = 0; i < vgich.lr_cnt; i++) {
		vs->lr[i] = vmm_readl((void *)vgich.hctrl_va + GICH_LR0 + 4*i);
	}
}

/* Restore current VGIC VCPU HW state */
static void __vgic_restore_vcpu_hwstate(struct vgic_vcpu_state *vs)
{
	u32 i;

	vmm_writel(vs->hcr, (void *)vgich.hctrl_va + GICH_HCR);
	vmm_writel(vs->vmcr, (void *)vgich.hctrl_va + GICH_VMCR);
	vmm_writel(vs->apr, (void *)vgich.hctrl_va + GICH_APR);
	for (i = 0; i < vgich.lr_cnt; i++) {
		vmm_writel(vs->lr[i], (void *)vgich.hctrl_va + GICH_LR0 + 4*i);
	}
}

/* Queue interrupt */
static bool __vgic_queue_irq(struct vgic_guest_state *s,
			     struct vgic_vcpu_state *vs,
			     u8 src_id, u32 irq)
{
	u32 lr, lrval;

	DPRINTF("%s: Queue IRQ%d to VCPU with SUBID=%d\n", 
		__func__, irq, vs->vcpu->subid);

	lr = VGIC_GET_LR_MAP(vs, irq);

	if (lr != VGIC_LR_UNKNOWN) {
		lrval = vmm_readl((void *)vgich.hctrl_va + GICH_LR0 + 4*lr);
		if (VGIC_LR_CPUID(lrval) == src_id) {
			BUG_ON(!VGIC_TEST_LR_USED(vs, lr));
			lrval |= GICH_LR_PENDING_BIT;
		}
		vmm_writel(lrval, (void *)vgich.hctrl_va + GICH_LR0 + 4*lr);
		return TRUE;
	}

	/* Try to use another LR for this interrupt */
	for (lr = 0; lr < vgich.lr_cnt; lr++) {
		if (!VGIC_TEST_LR_USED(vs, lr)) {
			break;
		}
	}
	if (lr >= vgich.lr_cnt) {
		return FALSE;
	}

	DPRINTF("%s: LR%d allocated for IRQ%d SRC=0x%x\n", 
		__func__, lr, irq, src_id);
	VGIC_SET_LR_MAP(vs, irq, lr);
	VGIC_SET_LR_USED(vs, lr);

	/* Set LR_EIO bit for level triggered interrupts */
	lrval = VGIC_MAKE_LR_PENDING(src_id, irq);
	if (!VGIC_TEST_TRIGGER(s, irq)) {
		lrval |= GICH_LR_EOI;
	} 

	DPRINTF("%s: LR%d = 0x%08x\n", __func__, lr, lrval);
	vmm_writel(lrval, (void *)vgich.hctrl_va + GICH_LR0 + 4*lr);

	return TRUE;
}

/* Queue software generated interrupt */
static bool __vgic_queue_sgi(struct vgic_guest_state *s, 
			     struct vgic_vcpu_state *vs,
			     u32 irq)
{
	u32 c, source = vs->sgi_source[irq];

	for (c = 0; c < VGIC_MAX_NCPU; c++) {
		if (!(source & (1 << c))) {
			continue;
		}
		if (__vgic_queue_irq(s, vs, c, irq)) {
			source &= ~(1 << c);
		}
	}

	vs->sgi_source[irq] = source;

	if (!source) {
		VGIC_CLEAR_PENDING(s, irq, (1 << vs->vcpu->subid));
		return TRUE;
	}

	return FALSE;
}

/* Queue hardware interrupt */
static bool __vgic_queue_hwirq(struct vgic_guest_state *s, 
			       struct vgic_vcpu_state *vs, 
			       u32 irq)
{
	u32 cm = (1 << vs->vcpu->subid);

	if (VGIC_TEST_ACTIVE(s, irq, cm)) {
		return TRUE; /* level interrupt, already queued */
	}

	if (__vgic_queue_irq(s, vs, 0, irq)) {
		if (VGIC_TEST_TRIGGER(s, irq)) {
			VGIC_CLEAR_PENDING(s, irq, cm);
		} else {
			VGIC_SET_ACTIVE(s, irq, cm);
		}

		return TRUE;
	}

	return FALSE;
}

/* Flush VGIC state to VGIC HW for IRQ on given VCPU */
static void __vgic_flush_vcpu_hwstate_irq(struct vgic_guest_state *s, 
					  struct vgic_vcpu_state *vs,
					  u32 irq)
{
	bool overflow = FALSE;
	u32 hcr, cm = (1 << vs->vcpu->subid);

	DPRINTF("%s: vcpu = %s\n", __func__, vs->vcpu->name);

	if (!s->enabled) {
		return;
	}

	if (!VGIC_TEST_ENABLED(s, irq, cm) || 
	    !VGIC_TEST_PENDING(s, irq, cm)) {
		return;
	}

	if (irq < 16) {
		if (!__vgic_queue_sgi(s, vs, irq)) {
			overflow = TRUE;
		}
	} else {
		if (!__vgic_queue_hwirq(s, vs, irq)) {
			overflow = TRUE;
		}
	}

	hcr = vmm_readl((void *)vgich.hctrl_va + GICH_HCR);
	if (overflow) {
		hcr |= GICH_HCR_UIE;
	} else {
		hcr &= ~GICH_HCR_UIE;
	}
	vmm_writel(hcr, (void *)vgich.hctrl_va + GICH_HCR);
}

/* Flush VGIC state to VGIC HW for given VCPU */
static void __vgic_flush_vcpu_hwstate(struct vgic_guest_state *s, 
				      struct vgic_vcpu_state *vs)
{
	bool overflow = FALSE;
	u32 hcr, irq, cm = (1 << vs->vcpu->subid);

	DPRINTF("%s: vcpu = %s\n", __func__, vs->vcpu->name);

	if (!s->enabled) {
		return;
	}

	for (irq = 0; irq < 16; irq++) {
		if (!VGIC_TEST_ENABLED(s, irq, cm) || 
		    !VGIC_TEST_PENDING(s, irq, cm)) {
			continue;
		}

		if (!__vgic_queue_sgi(s, vs, irq)) {
			overflow = TRUE;
		}
	}

	for (irq = 16; irq < VGIC_NUM_IRQ(s); irq++) {
		if (!VGIC_TEST_ENABLED(s, irq, cm) || 
		    !VGIC_TEST_PENDING(s, irq, cm)) {
			continue;
		}

		if (!__vgic_queue_hwirq(s, vs, irq)) {
			overflow = TRUE;
		}
	}

	hcr = vmm_readl((void *)vgich.hctrl_va + GICH_HCR);
	if (overflow) {
		hcr |= GICH_HCR_UIE;
	} else {
		hcr &= ~GICH_HCR_UIE;
	}
	vmm_writel(hcr, (void *)vgich.hctrl_va + GICH_HCR);
}

/* Sync current VCPU VGIC state with HW state */
static void __vgic_sync_vcpu_hwstate(struct vgic_guest_state *s, 
				     struct vgic_vcpu_state *vs)
{
	u32 hcr, misr, eisr[2], elrsr[2];
	u32 lr, lrval, irq, cm = (1 << vs->vcpu->subid);

	hcr = vmm_readl((void *)vgich.hctrl_va + GICH_HCR);
	misr = vmm_readl((void *)vgich.hctrl_va + GICH_MISR);
	elrsr[0] = vmm_readl((void *)vgich.hctrl_va + GICH_ELRSR0);
	if (32 < vgich.lr_cnt) {
		elrsr[1] = vmm_readl((void *)vgich.hctrl_va + GICH_ELRSR1);
	} else {
		elrsr[1] = 0x0;
	}

	DPRINTF("%s: vcpu = %s\n", __func__, vs->vcpu->name);
	DPRINTF("%s: MISR = %08x\n", __func__, misr);
	DPRINTF("%s: ELRSR0 = %08x\n", __func__, elrsr[0]);
	DPRINTF("%s: ELRSR1 = %08x\n", __func__, elrsr[1]);

	if (misr & GICH_MISR_EOI) {
		eisr[0] = vmm_readl((void *)vgich.hctrl_va + GICH_EISR0);
		if (32 < vgich.lr_cnt) {
			eisr[1] = vmm_readl((void *)vgich.hctrl_va + GICH_EISR1);
		} else {
			eisr[1] = 0x0;
		}
		DPRINTF("%s: EISR0 = %08x\n", __func__, eisr[0]);
		DPRINTF("%s: EISR1 = %08x\n", __func__, eisr[1]);

		/* Some level interrupts have been EOIed. 
		 * Clear their active bit.
		 */
		for (lr = 0; lr < vgich.lr_cnt; lr++) {
			if (!VGIC_TEST_EISR(eisr, lr)) {
				continue;
			}

			lrval = vmm_readl((void *)vgich.hctrl_va + GICH_LR0 + 4*lr);

			irq = lrval & GICH_LR_VIRTUALID;

			VGIC_CLEAR_ACTIVE(s, irq, cm);
			lrval &= ~GICH_LR_EOI;

			/* Mark level triggered interrupts as pending if 
			 * they are still raised.
			 */
			if (!VGIC_TEST_TRIGGER(s, irq) && 
			    VGIC_TEST_ENABLED(s, irq, cm) &&
			    VGIC_TEST_LEVEL(s, irq, cm) && 
			    (VGIC_TARGET(s, irq) & cm) != 0) {
				VGIC_SET_PENDING(s, irq, cm);
			} else {
				VGIC_CLEAR_PENDING(s, irq, cm);
			}

			/* Despite being EOIed, the LR may not have
			 * been marked as empty.
			 */
			VGIC_SET_ELRSR(elrsr, lr);
			lrval &= ~GICH_LR_ACTIVE_BIT;

			vmm_writel(lrval, (void *)vgich.hctrl_va + GICH_LR0 + 4*lr);
		}
	}

	if (misr & GICH_MISR_U) {
		hcr &= ~GICH_HCR_UIE;
	}

	for (lr = 0; lr < vgich.lr_cnt; lr++) {
		if (!VGIC_TEST_ELRSR(elrsr, lr)) {
			continue;
		}

		if (!VGIC_TEST_LR_USED(vs, lr)) {
			continue;
		}

		lrval = vmm_readl((void *)vgich.hctrl_va + GICH_LR0 + 4*lr);

		DPRINTF("%s: LR%d = 0x%08x\n", __func__, lr, lrval);

		VGIC_CLEAR_LR_USED(vs, lr);

		irq = lrval & GICH_LR_VIRTUALID;

		BUG_ON(irq >= VGIC_MAX_NIRQ);

		VGIC_SET_LR_MAP(vs, irq, VGIC_LR_UNKNOWN);
	}

	vmm_writel(hcr, (void *)vgich.hctrl_va + GICH_HCR);
}

/* Process IRQ asserted by device emulation framework */
static void vgic_irq_handle(u32 irq, int cpu, int level, void *opaque)
{
	int cm, target;
	bool wfi_resume = FALSE;
	irq_flags_t flags;
	struct vgic_vcpu_state *vs;
	struct vgic_guest_state *s = opaque;

	DPRINTF("%s: irq=%d cpu=%d level=%d\n", __func__, irq, cpu, level);

	/* Lock VGIC Guest state */
	vmm_spin_lock_irqsave(&s->lock, flags);

	if (irq < 32) {
		/* In case of PPIs and SGIs */
		cm = target = (1 << cpu);
	} else {
		/* In case of SGIs */
		cm = VGIC_ALL_CPU_MASK(s);
		target = VGIC_TARGET(s, irq);
		for (cpu = 0; cpu < VGIC_NUM_CPU(s); cpu++) {
			if (target & (1 << cpu)) {
				break;
			}
		}
		if (VGIC_NUM_CPU(s) <= cpu) {
			vmm_spin_unlock_irqrestore(&s->lock, flags);
			return;
		}
	}

	/* Find out VCPU pointer */
	BUG_ON(cpu < 0);
	vs = &s->vstate[cpu];

	/* If level not changed then skip */
	if (level == VGIC_TEST_LEVEL(s, irq, cm)) {
		goto done;
	}

	/* Forcefully resume VCPU if waiting for IRQ */
	wfi_resume = TRUE;

	/* Update IRQ state*/
	if (level) {
		VGIC_SET_LEVEL(s, irq, cm);
		if (VGIC_TEST_TRIGGER(s, irq) || 
		    VGIC_TEST_ENABLED(s, irq, cm)) {
			VGIC_SET_PENDING(s, irq, target);
		}
	} else {
		VGIC_CLEAR_LEVEL(s, irq, cm);
	}

	/* Directly updating VGIC HW for current VCPU */
	if (vs->vcpu == vmm_scheduler_current_vcpu()) {
		/* Flush IRQ state change to VGIC HW */
		__vgic_flush_vcpu_hwstate_irq(s, vs, irq);
	}

done:
	/* Unlock VGIC Guest state */
	vmm_spin_unlock_irqrestore(&s->lock, flags);

	/* Resume from WFI if required */
	if (wfi_resume) {
		vmm_vcpu_irq_wait_resume(vs->vcpu);
	}
}

/* Handle maintainence IRQ generated by hardware */
static vmm_irq_return_t vgic_maint_irq(int irq_no, void *dev)
{
	irq_flags_t flags;
	struct vgic_guest_state *s;
	struct vgic_vcpu_state *vs;
	struct vmm_vcpu *vcpu = vmm_scheduler_current_vcpu();

	/* We should not get this interrupt when not 
	 * running a VGIC enabled normal VCPU.
	 */
	BUG_ON(!vcpu);
	BUG_ON(!vcpu->is_normal);
	BUG_ON(!arm_vgic_avail(vcpu));

	s = arm_vgic_priv(vcpu);
	vs = &s->vstate[vcpu->subid];

	/* Lock VGIC Guest state */
	vmm_spin_lock_irqsave(&s->lock, flags);

	/* The VGIC HW state may have changed when the 
	 * VCPU was running hence, sync VGIC VCPU state.
	 */
	__vgic_sync_vcpu_hwstate(s, vs);

	/* Unlock VGIC Guest state */
	vmm_spin_unlock_irqrestore(&s->lock, flags);

	return VMM_IRQ_HANDLED;
}

/* Save VCPU context for current VCPU */
static void vgic_save_vcpu_context(void *vcpu_ptr)
{
	irq_flags_t flags;
	struct vgic_guest_state *s;
	struct vgic_vcpu_state *vs;
	struct vmm_vcpu *vcpu = vcpu_ptr;

	BUG_ON(!vcpu);

	s = arm_vgic_priv(vcpu);
	vs = &s->vstate[vcpu->subid];

	/* Lock VGIC Guest state */
	vmm_spin_lock_irqsave(&s->lock, flags);

	/* The VGIC HW state may have changed when the 
	 * VCPU was running hence, sync VGIC VCPU state.
	 */
	__vgic_sync_vcpu_hwstate(s, vs);

	/* Save VGIC HW registers */
	__vgic_save_vcpu_hwstate(vs);

	/* Unlock VGIC Guest state */
	vmm_spin_unlock_irqrestore(&s->lock, flags);
}

/* Restore VCPU context for current VCPU */
static void vgic_restore_vcpu_context(void *vcpu_ptr)
{
	irq_flags_t flags;
	struct vgic_guest_state *s;
	struct vgic_vcpu_state *vs;
	struct vmm_vcpu *vcpu = vcpu_ptr;

	BUG_ON(!vcpu);

	s = arm_vgic_priv(vcpu);
	vs = &s->vstate[vcpu->subid];

	/* Lock VGIC Guest state */
	vmm_spin_lock_irqsave(&s->lock, flags);

	/* Restore VGIC HW registers */
	__vgic_restore_vcpu_hwstate(vs);

	/* Flush VGIC state changes to VGIC HW for 
	 * reflecting latest changes while, the VCPU 
	 * was not running.
	 */
	__vgic_flush_vcpu_hwstate(s, vs);

	/* Unlock VGIC Guest state */
	vmm_spin_unlock_irqrestore(&s->lock, flags);
}

static int __vgic_dist_readb(struct vgic_guest_state *s, int cpu, 
			     u32 offset, u8 *dst)
{
	u32 done = 0, i, irq, mask;

	if (!s || !dst) {
		return VMM_EFAIL;
	}

	done = 1;
	switch (offset - (offset & 0x3)) {
	case 0x000: /* Distributor control */
		if (offset == 0x000) {
			*dst = s->enabled;
		} else {
			*dst = 0x0;
		}
		break;
	case 0x004: /* Controller type */
		if (offset == 0x004) {
			*dst = (VGIC_NUM_CPU(s) - 1) << 5;
			*dst |= (VGIC_NUM_IRQ(s) / 32) - 1;
		} else {
			*dst = 0x0;
		}
		break;
	case 0x100: /* Set-enable0 */
	case 0x104: /* Set-enable1 */
	case 0x108: /* Set-enable2 */
	case 0x180: /* Clear-enable0 */
	case 0x184: /* Clear-enable1 */
	case 0x188: /* Clear-enable2 */
		irq = (offset & 0xF) * 8;
		*dst = 0;
		for (i = 0; i < 8; i++) {
			*dst |= VGIC_TEST_ENABLED(s, irq + i, (1 << cpu)) ? 
				(1 << i) : 0x0;
		}
		break;
	case 0x200: /* Set-pending0 */
	case 0x204: /* Set-pending1 */
	case 0x208: /* Set-pending2 */
	case 0x280: /* Clear-pending0 */
	case 0x284: /* Clear-pending1 */
	case 0x288: /* Clear-pending2 */
		irq = (offset & 0xF) * 8;
		mask = (irq < 32) ? (1 << cpu) : VGIC_ALL_CPU_MASK(s);
		*dst = 0;
		for (i = 0; i < 8; i++) {
			*dst |= VGIC_TEST_PENDING(s, irq + i, mask) ? 
				(1 << i) : 0x0;
		}
		break;
	case 0x300: /* Active0 */
	case 0x304: /* Active1 */
	case 0x308: /* Active2 */
		irq = (offset & 0xF) * 8;
		mask = (irq < 32) ? (1 << cpu) : VGIC_ALL_CPU_MASK(s);
		*dst = 0;
		for (i = 0; i < 8; i++) {
			*dst |= VGIC_TEST_ACTIVE(s, irq + i, mask) ? 
				(1 << i) : 0x0;
		}
		break;
	default:
		done = 0;
		break;
	};

	if (!done) {
		done = 1;
		switch (offset >> 8) {
		case 0x4: /* Priority */
			irq = offset - 0x400;
			if (VGIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			*dst = VGIC_GET_PRIORITY(s, irq, cpu) << 4;
			break;
		case 0x8: /* CPU targets */
			irq = offset - 0x800;
			if (VGIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (irq < 32) {
				*dst = 1 << cpu;
			} else {
				*dst = VGIC_TARGET(s, irq);
			}
			break;
		case 0xC: /* Configuration */
			irq = (offset - 0xC00) * 4;
			if (VGIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			*dst = 0;
			for (i = 0; i < 4; i++) {
				if (VGIC_TEST_MODEL(s, irq + i)) {
					*dst |= (1 << (i * 2));
				}
				if (VGIC_TEST_TRIGGER(s, irq + i)) {
					*dst |= (2 << (i * 2));
				}
			}
			break;
		case 0xF:
			if (0xFE0 <= offset) {
				if (offset & 0x3) {
					*dst = 0;
				} else {
					*dst = s->id[(offset - 0xFE0) >> 2];
				}
			} else {
				done = 0;
			}
			break;
		default:
			done = 0;
			break;
		};
	}

	if (!done) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

static int __vgic_dist_writeb(struct vgic_guest_state *s, int cpu, 
			      u32 offset, u8 src)
{
	u32 done = 0, i, irq, mask, cm;

	if (!s) {
		return VMM_EFAIL;
	}

	done = 1;
	switch (offset - (offset & 0x3)) {
	case 0x000: /* Distributor control */
		if (offset == 0x000) {
			s->enabled = src & 0x1;
		}
		break;
	case 0x004: /* Controller type */
		/* Ignored. */
		break;
	case 0x100: /* Set-enable0 */
	case 0x104: /* Set-enable1 */
	case 0x108: /* Set-enable2 */
		irq = (offset & 0xF) *8;
		if (VGIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		if (irq < 16) {
			src = 0xFF;
		}
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				mask = ((irq + i) < 32) ? 
					(1 << cpu) : VGIC_TARGET(s, (irq + i));
				cm = ((irq + i) < 32) ? 
					(1 << cpu) : VGIC_ALL_CPU_MASK(s);
				VGIC_SET_ENABLED(s, irq + i, cm);
				/* If a raised level triggered IRQ enabled 
				 * then mark is as pending.  */
				if (VGIC_TEST_LEVEL(s, (irq + i), mask) &&
				    !VGIC_TEST_TRIGGER(s, (irq + i))) {
					VGIC_SET_PENDING(s, (irq + i), mask);
				}
			}
		}
		break;
	case 0x180: /* Clear-enable0 */
	case 0x184: /* Clear-enable1 */
	case 0x188: /* Clear-enable2 */
		irq = (offset & 0xF) *8;
		if (VGIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		if (irq < 16) {
			src = 0x00;
		}
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				int cm = ((irq + i) < 32) ? 
					(1 << cpu) : VGIC_ALL_CPU_MASK(s);
				VGIC_CLEAR_ENABLED(s, irq + i, cm);
			}
		}
		break;
	case 0x200: /* Set-pending0 */
	case 0x204: /* Set-pending1 */
	case 0x208: /* Set-pending2 */
		irq = (offset & 0xF) *8;
		if (VGIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		if (irq < 16) {
			src = 0x00;
		}
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				mask = VGIC_TARGET(s, irq + i);
				VGIC_SET_PENDING(s, irq + i, mask);
			}
		}
		break;
	case 0x280: /* Clear-pending0 */
	case 0x284: /* Clear-pending1 */
	case 0x288: /* Clear-pending2 */
		irq = (offset & 0xF) *8;
		if (VGIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		/* ??? This currently clears the pending bit for all CPUs, even
 		 * for per-CPU interrupts.  It's unclear whether this is the
		 * corect behavior.  */
		mask = VGIC_ALL_CPU_MASK(s);
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				VGIC_CLEAR_PENDING(s, irq + i, mask);
			}
		}
		break;
	default:
		done = 0;
		break;
	};

	if (!done) {
		done = 1;
		switch (offset >> 8) {
		case 0x1: /* Reserved */
		case 0x2: /* Reserved */
		case 0x3: /* Reserved */
			break;
		case 0x4: /* Priority */
			irq = offset - 0x400;
			if (VGIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (irq < 32) {
				s->priority1[irq][cpu] = src >> 4;
			} else {
				s->priority2[irq - 32] = src >> 4;
			}
			break;
		case 0x8: /* CPU targets */
			irq = offset - 0x800;
			if (VGIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (irq < 16) {
				src = 0x0;
			} else if (irq < 32) {
				src = VGIC_ALL_CPU_MASK(s);
			}
			s->irq_target[irq] = src & VGIC_ALL_CPU_MASK(s);
			break;
		case 0xC: /* Configuration */
			irq = (offset - 0xC00) * 4;
			if (VGIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (irq < 32) {
				src |= 0xAA;
			}
			for (i = 0; i < 4; i++) {
				if (src & (1 << (i * 2))) {
					VGIC_SET_MODEL(s, irq + i);
				} else {
					VGIC_CLEAR_MODEL(s, irq + i);
				}
				if (src & (2 << (i * 2))) {
					VGIC_SET_TRIGGER(s, irq + i);
				} else {
					VGIC_CLEAR_TRIGGER(s, irq + i);
				}
			}
			break;
		default:
			done = 0;
			break;
		};
	}

	if (!done) {
		return VMM_EFAIL;
	}

	return VMM_OK;
}

static int vgic_dist_read(struct vgic_guest_state *s, int cpu, 
			  u32 offset, u32 *dst)
{
	int rc = VMM_OK, i;
	irq_flags_t flags;
	u8 val;

	if (!s || !dst) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&s->lock, flags);

	*dst = 0;
	for (i = 0; i < 4; i++) {
		if ((rc = __vgic_dist_readb(s, cpu, offset + i, &val))) {
				break;
		}
		*dst |= val << (i * 8);
	}

	vmm_spin_unlock_irqrestore(&s->lock, flags);

	return VMM_OK;
}

static int vgic_dist_write(struct vgic_guest_state *s, int cpu, 
			   u32 offset, u32 src_mask, u32 src)
{
	int rc = VMM_OK, irq, mask, i;
	irq_flags_t flags;
	struct vgic_vcpu_state *vs;

	if (!s) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&s->lock, flags);

	vs = &s->vstate[cpu];

	if (offset == 0xF00) {
		/* Software Interrupt */
		irq = src & 0x3ff;
		switch ((src >> 24) & 3) {
		case 0:
			mask = (src >> 16) & VGIC_ALL_CPU_MASK(s);
			break;
		case 1:
			mask = VGIC_ALL_CPU_MASK(s) ^ (1 << cpu);
			break;
		case 2:
			mask = 1 << cpu;
			break;
		default:
			mask = VGIC_ALL_CPU_MASK(s);
			break;
		};
		VGIC_SET_PENDING(s, irq, mask);
		for (i = 0; (irq < 16) && (i < VGIC_NUM_CPU(s)); i++) {
			if (!(mask & (1 << i))) {
				continue;
			}
			s->vstate[i].sgi_source[irq] |= (1 << cpu);
			if (s->vstate[i].vcpu->subid != vs->vcpu->subid) {
				vmm_vcpu_irq_wait_resume(s->vstate[i].vcpu);
			}
		}
	} else {
		src_mask = ~src_mask;
		for (i = 0; i < 4; i++) {
			if (src_mask & 0xFF) {
				if ((rc = __vgic_dist_writeb(s, cpu, 
						offset + i, src & 0xFF))) {
					break;
				}
			}
			src_mask = src_mask >> 8;
			src = src >> 8;
		}
	}

	/* The VGIC HW state may have changed when the 
	 * VCPU was running hence, sync VGIC VCPU state.
	 */
	__vgic_sync_vcpu_hwstate(s, vs);

	/* Flush VGIC state changes to VGIC HW for 
	 * reflecting latest changes while, the VCPU 
	 * was not running.
	 */
	__vgic_flush_vcpu_hwstate(s, vs);

	vmm_spin_unlock_irqrestore(&s->lock, flags);

	return rc;
}

static int vgic_emulator_read(struct vmm_emudev *edev,
			       physical_addr_t offset, 
			       void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 regval = 0x0;
	struct vmm_vcpu *vcpu;
	struct vgic_guest_state *s = edev->priv;

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->id != vcpu->guest->id) {
		return VMM_EFAIL;
	}

	/* Read Distribution Control */
	rc = vgic_dist_read(s, vcpu->subid, offset & 0xFFC, &regval);
	if (rc) {
		return rc;
	}

	regval = (regval >> ((offset & 0x3) * 8));
	switch (dst_len) {
	case 1:
		*(u8 *)dst = regval & 0xFF;
		break;
	case 2:
		*(u16 *)dst = vmm_cpu_to_le16(regval & 0xFFFF);
		break;
	case 4:
		*(u32 *)dst = vmm_cpu_to_le32(regval);
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	return rc;
}

static int vgic_emulator_write(struct vmm_emudev *edev,
				physical_addr_t offset, 
				void *src, u32 src_len)
{
	int i;
	u32 regmask = 0x0, regval = 0x0;
	struct vgic_guest_state *s = edev->priv;
	struct vmm_vcpu *vcpu;

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->id != vcpu->guest->id) {
		return VMM_EFAIL;
	}

	switch (src_len) {
	case 1:
		regmask = 0xFFFFFF00;
		regval = *(u8 *)src;
		break;
	case 2:
		regmask = 0xFFFF0000;
		regval = vmm_le16_to_cpu(*(u16 *)src);
		break;
	case 4:
		regmask = 0x00000000;
		regval = vmm_le32_to_cpu(*(u32 *)src);
		break;
	default:
		return VMM_EFAIL;
		break;
	};

	for (i = 0; i < (offset & 0x3); i++) {
		regmask = (regmask << 8) | ((regmask >> 24) & 0xFF);
	}
	regval = (regval << ((offset & 0x3) * 8));

	return vgic_dist_write(s, vcpu->subid, 
				offset & 0xFFC, regmask, regval);
}

static int vgic_state_reset(struct vgic_guest_state *s)
{
	u32 i, j;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&s->lock, flags);

	/* Reset context for all VCPUs
	 *
	 * We force VMCR to zero. 
	 * This will restore the binary points to reset values.
	 */
	for (i = 0; i < VGIC_NUM_CPU(s); i++) {
		s->vstate[i].hcr = GICH_HCR_EN;
		s->vstate[i].vmcr = 0;
		s->vstate[i].apr = 0;
		for (j = 0; j < vgich.lr_cnt; j++) {
			s->vstate[i].lr[j] = 0x0;
		}
		for (j = 0; j < (GICH_LR_MAX_COUNT / 32); j++) {
			s->vstate[i].lr_used[j] = 0x0;
		}
		for (j = 0; j < 32; j++) {
			s->vstate[i].sgi_source[j] = 0x0;
		}
		for (j = 0; j < VGIC_NUM_IRQ(s); j++) {
			VGIC_SET_LR_MAP(&s->vstate[i], j, VGIC_LR_UNKNOWN);
		}
	}

	/*
	 * We should not reset level as for host to guest IRQ might
	 * have been raised already.
	 */
	for (i = 0; i < VGIC_NUM_IRQ(s); i++) {
		VGIC_CLEAR_ENABLED(s, i, VGIC_ALL_CPU_MASK(s));
		VGIC_CLEAR_PENDING(s, i, VGIC_ALL_CPU_MASK(s));
		VGIC_CLEAR_ACTIVE(s, i, VGIC_ALL_CPU_MASK(s));
		VGIC_CLEAR_MODEL(s, i);
		VGIC_CLEAR_TRIGGER(s, i);
	}

	/* Reset software generated interrupts */
	for (i = 0; i < 16; i++) {
		VGIC_SET_ENABLED(s, i, VGIC_ALL_CPU_MASK(s));
		VGIC_SET_TRIGGER(s, i);
	}

	s->enabled = 0;

	vmm_spin_unlock_irqrestore(&s->lock, flags);

	return VMM_OK;
}

static int vgic_emulator_reset(struct vmm_emudev *edev)
{
	struct vgic_guest_state *s = edev->priv;
	
	return vgic_state_reset(s);
}

static struct vgic_guest_state *vgic_state_alloc(const char *name,
						 struct vmm_guest *guest, 
						 u32 num_cpu, 
						 u32 num_irq,
						 u32 parent_irq)
{
	u32 i;
	struct dlist *l;
	struct vmm_vcpu *vcpu;
	struct vgic_guest_state *s = NULL;

	/* Alloc VGIC state */
	s = vmm_zalloc(sizeof(struct vgic_guest_state));
	if (!s) {
		return NULL;
	}

	s->guest = guest;

	INIT_SPIN_LOCK(&s->lock);

	s->num_cpu = num_cpu;
	s->num_irq = num_irq;
	s->id[0] = 0x90 /* id0 */;
	s->id[1] = 0x13 /* id1 */;
	s->id[2] = 0x04 /* id2 */;
	s->id[3] = 0x00 /* id3 */;
	s->id[4] = 0x0d /* id4 */;
	s->id[5] = 0xf0 /* id5 */;
	s->id[6] = 0x05 /* id6 */;
	s->id[7] = 0xb1 /* id7 */;

	for (i = 0; i < s->num_cpu; i++) {
		s->vstate[i].vcpu = vmm_manager_guest_vcpu(guest, i);
		s->vstate[i].parent_irq = parent_irq;
	}

	/* Register guest irq handler s*/
	for (i = 0; i < s->num_irq; i++) {
		vmm_devemu_register_irq_handler(guest, i, 
						name, vgic_irq_handle, s);
	}

	/* Setup save/restore hooks */
	list_for_each(l, &guest->vcpu_list) {
		vcpu = list_entry(l, struct vmm_vcpu, head);
		arm_vgic_setup(vcpu, 
			vgic_save_vcpu_context, 
			vgic_restore_vcpu_context, s);
	}

	return s;
}

static int vgic_state_free(struct vgic_guest_state *s)
{
	u32 i;
	struct dlist *l;
	struct vmm_vcpu *vcpu;

	if (!s) {
		return VMM_EFAIL;
	}

	/* Cleanup save/restore hooks */
	list_for_each(l, &s->guest->vcpu_list) {
		vcpu = list_entry(l, struct vmm_vcpu, head);
		arm_vgic_cleanup(vcpu);
	}

	/* Unregister guest irq handler */
	for (i = 0; i < s->num_irq; i++) {
		vmm_devemu_unregister_irq_handler(s->guest, i,
						  vgic_irq_handle, s);
	}

	/* Free VGIC state */
	vmm_free(s);

	return VMM_OK;
}

static int vgic_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	const char *attr;
	u32 parent_irq;
	struct vgic_guest_state *s;

	if (!vgich.avail) {
		return VMM_ENODEV;
	}

	attr = vmm_devtree_attrval(edev->node, "parent_irq");
	if (!attr) {
		return VMM_EFAIL;
	}
	parent_irq = *((u32 *)attr);

	if (guest->vcpu_count > VGIC_MAX_NCPU) {
		return VMM_EINVALID;
	}

	s = vgic_state_alloc(edev->node->name,
			     guest, guest->vcpu_count,
			     VGIC_MAX_NIRQ, parent_irq);
	if (!s) {
		return VMM_ENOMEM;
	}

	edev->priv = s;

	return VMM_OK;
}

static int vgic_emulator_remove(struct vmm_emudev *edev)
{
	struct vgic_guest_state *s = edev->priv;

	vgic_state_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid vgic_emuid_table[] = {
	{ .type = "pic", 
	  .compatible = "arm,vgic", 
	},
	{ /* end of list */ },
};

static struct vmm_emulator vgic_emulator = {
	.name = "vgic",
	.match_table = vgic_emuid_table,
	.probe = vgic_emulator_probe,
	.read = vgic_emulator_read,
	.write = vgic_emulator_write,
	.reset = vgic_emulator_reset,
	.remove = vgic_emulator_remove,
};

static const struct vmm_devtree_nodeid vgic_host_match[] = {
	{ .compatible	= "arm,cortex-a15-gic",	},
	{},
};

static void vgic_enable_maint_irq(void *arg0, void *arg1, void *arg3)
{
	if (vgich.avail) {
		gic_enable_ppi(vgich.maint_irq);
	}
}

static int __init vgic_emulator_init(void)
{
	int rc;
	struct vmm_devtree_node *node;

	vgich.avail = FALSE;

	node = vmm_devtree_find_matching(NULL, vgic_host_match);
	if (!node) {
		vmm_printf("%s: GIC node not found\n", __func__);
		rc = VMM_ENODEV;
		goto fail;
	}

	rc = vmm_devtree_regaddr(node, &vgich.hctrl_pa, 2);
	if (rc) {
		goto fail;
	}

	rc = vmm_devtree_regmap(node, &vgich.hctrl_va, 2);
	if (rc) {
		goto fail;
	}

	rc = vmm_devtree_regaddr(node, &vgich.vcpu_pa, 3);
	if (rc) {
		goto fail_unmap_hctrl;
	}

	rc = vmm_devtree_regmap(node, &vgich.vcpu_va, 3);
	if (rc) {
		goto fail_unmap_hctrl;
	}

	rc = vmm_devtree_irq_get(node, &vgich.maint_irq, 0);
	if (rc) {
		goto fail_unmap_vcpu;
	}

	rc = vmm_host_irq_register(vgich.maint_irq, "vGIC", 
				   vgic_maint_irq, NULL);
	if (rc) {
		goto fail_unmap_vcpu;
	}

	rc = vmm_host_irq_mark_per_cpu(vgich.maint_irq);
	if (rc) {
		goto fail_unreg_irq;
	}

	rc = vmm_devemu_register_emulator(&vgic_emulator);
	if (rc) {
		goto fail_unmark_irq;
	}

	vgich.avail = TRUE;

	vgich.lr_cnt = vmm_readl((void *)vgich.hctrl_va + GICH_VTR);
	vgich.lr_cnt = (vgich.lr_cnt & GICH_VTR_LRCNT_MASK) + 1;

	vmm_smp_ipi_async_call(cpu_possible_mask, vgic_enable_maint_irq,
				NULL, NULL, NULL);

	DPRINTF("VGIC: HCTRL=0x%lx VCPU=0x%lx LR_CNT=%d\n", 
		(unsigned long)vgich.hctrl_pa, 
		(unsigned long)vgich.vcpu_pa, vgich.lr_cnt);

	return VMM_OK;

fail_unmark_irq:
	vmm_host_irq_unmark_per_cpu(vgich.maint_irq);
fail_unreg_irq:
	vmm_host_irq_unregister(vgich.maint_irq, NULL);
fail_unmap_vcpu:
	vmm_devtree_regunmap(node, vgich.vcpu_va, 3);
fail_unmap_hctrl:
	vmm_devtree_regunmap(node, vgich.hctrl_va, 2);
fail:
	return rc;
}

static void __exit vgic_emulator_exit(void)
{
	struct vmm_devtree_node *node;

	node = vmm_devtree_find_matching(NULL, vgic_host_match);
	if (!node) {
		vmm_printf("%s: GIC node not found\n", __func__);
		return;
	}

	if (node && vgich.avail) {
		vmm_devemu_unregister_emulator(&vgic_emulator);

		vmm_host_irq_unmark_per_cpu(vgich.maint_irq);

		vmm_host_irq_unregister(vgich.maint_irq, NULL);

		vmm_devtree_regunmap(node, vgich.vcpu_va, 3);

		vmm_devtree_regunmap(node, vgich.hctrl_va, 2);
	}
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
