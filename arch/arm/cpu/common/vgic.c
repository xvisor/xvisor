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
#include <vmm_limits.h>
#include <vmm_smp.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_irq.h>
#include <vmm_scheduler.h>
#include <vmm_vcpu_irq.h>
#include <vmm_guest_aspace.h>
#include <vmm_devemu.h>
#include <vmm_modules.h>
#include <arch_regs.h>
#include <libs/bitmap.h>

#include <vgic.h>

#define MODULE_DESC			"GICv2 HW-assisted Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define MODULE_INIT			vgic_emulator_init
#define MODULE_EXIT			vgic_emulator_exit

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define VGIC_MAX_NCPU			8
#define VGIC_MAX_NIRQ			256
#define VGIC_LR_UNKNOWN			0xFF

struct vgic_host_ctrl {
	bool avail;
	struct vgic_ops ops;
	struct vgic_params params;
};

static struct vgic_host_ctrl vgich;

struct vgic_irq_state {
	u32 active:VGIC_MAX_NCPU;
	u32 level:VGIC_MAX_NCPU;
	u32 model:1; /* 0 = N:N, 1 = 1:N */
	u32 trigger:1; /* nonzero = edge triggered.  */
	u32 host_irq; /* If UINT_MAX then not mapped to host irq else mapped */
};

struct vgic_vcpu_state {
	/* General Info */
	struct vmm_vcpu *vcpu;
	u32 parent_irq;

	/* Register state */
	struct vgic_hw_state hw;

	/* Maintainence Info */
	u32 lr_used_count;
	u32 lr_used[VGIC_MAX_LRS / 32];
	u8 irq_lr[VGIC_MAX_NIRQ];
};

struct vgic_guest_state {
	/* Guest to which this VGIC belongs */
	struct vmm_guest *guest;

	/* Configuration */
	u8 id[8];
	u32 num_cpu;
	u32 num_irq;

	/* Context of each VCPU */
	struct vgic_vcpu_state vstate[VGIC_MAX_NCPU];

	/* Lock to protect VGIC distributor state */
	vmm_spinlock_t dist_lock;

	/* Chip enable/disable */
	u32 enabled;

	/* Distribution Control */
	struct vgic_irq_state irq_state[VGIC_MAX_NIRQ];
	u32 sgi_source[VGIC_MAX_NCPU][16];
	u32 irq_target[VGIC_MAX_NIRQ];
	u32 priority1[32][VGIC_MAX_NCPU];
	u32 priority2[VGIC_MAX_NIRQ - 32];
	u32 irq_enabled[VGIC_MAX_NCPU][VGIC_MAX_NIRQ / 32];
	u32 irq_pending[VGIC_MAX_NCPU][VGIC_MAX_NIRQ / 32];
};

/* Set interrupt enabled
 * Note: Must be called with VGIC distributor lock held
 */
static void __vgic_set_enabled(struct vgic_guest_state *s, u32 irq, u32 cm)
{
	u32 i;
	for (i = 0; i < s->num_cpu; i++) {
		if (!(cm & (1 << i)))
			continue;
		s->irq_enabled[i][irq >> 5] |= (1 << (irq & 0x1f));
	}
}

/* Clear interrupt enabled
 * Note: Must be called with VGIC distributor lock held
 */
static void __vgic_clear_enabled(struct vgic_guest_state *s, u32 irq, u32 cm)
{
	u32 i;
	for (i = 0; i < s->num_cpu; i++) {
		if (!(cm & (1 << i)))
			continue;
		s->irq_enabled[i][irq >> 5] &= ~(1 << (irq & 0x1f));
	}
}

/* Test interrupt enabled
 * Note: Must be called with VGIC distributor lock held
 */
static bool __vgic_test_enabled(struct vgic_guest_state *s, u32 irq, u32 cm)
{
	u32 i;
	for (i = 0; i < s->num_cpu; i++) {
		if (!(cm & (1 << i)))
			continue;
		if (s->irq_enabled[i][irq >> 5] & (1 << (irq & 0x1f)))
			return TRUE;
	}
	return FALSE;
}

/* Set interrupt pending
 * Note: Must be called with VGIC distributor lock held
 */
static void __vgic_set_pending(struct vgic_guest_state *s, u32 irq, u32 cm)
{
	u32 i;
	for (i = 0; i < s->num_cpu; i++) {
		if (!(cm & (1 << i)))
			continue;
		s->irq_pending[i][irq >> 5] |= (1 << (irq & 0x1f));
	}
}

/* Clear interrupt pending
 * Note: Must be called with VGIC distributor lock held
 */
static void __vgic_clear_pending(struct vgic_guest_state *s, u32 irq, u32 cm)
{
	u32 i;
	for (i = 0; i < s->num_cpu; i++) {
		if (!(cm & (1 << i)))
			continue;
		s->irq_pending[i][irq >> 5] &= ~(1 << (irq & 0x1f));
	}
}

/* Test interrupt pending
 * Note: Must be called with VGIC distributor lock held
 */
static bool __vgic_test_pending(struct vgic_guest_state *s, u32 irq, u32 cm)
{
	u32 i;
	for (i = 0; i < s->num_cpu; i++) {
		if (!(cm & (1 << i)))
			continue;
		if (s->irq_pending[i][irq >> 5] & (1 << (irq & 0x1f)))
			return TRUE;
	}
	return FALSE;
}

#define VGIC_ALL_CPU_MASK(s) ((1 << (s)->num_cpu) - 1)
#define VGIC_NUM_CPU(s) ((s)->num_cpu)
#define VGIC_NUM_IRQ(s) ((s)->num_irq)
#define VGIC_SET_ENABLED(s, irq, cm) __vgic_set_enabled(s, irq, cm)
#define VGIC_CLEAR_ENABLED(s, irq, cm) __vgic_clear_enabled(s, irq, cm)
#define VGIC_TEST_ENABLED(s, irq, cm)  __vgic_test_enabled(s, irq, cm)
#define VGIC_SET_PENDING(s, irq, cm) __vgic_set_pending(s, irq, cm)
#define VGIC_CLEAR_PENDING(s, irq, cm) __vgic_clear_pending(s, irq, cm)
#define VGIC_TEST_PENDING(s, irq, cm) __vgic_test_pending(s, irq, cm)
#define VGIC_SET_ACTIVE(s, irq, cm) (s)->irq_state[irq].active |= (cm)
#define VGIC_CLEAR_ACTIVE(s, irq, cm) (s)->irq_state[irq].active &= ~(cm)
#define VGIC_TEST_ACTIVE(s, irq, cm) ((s)->irq_state[irq].active & (cm))
#define VGIC_SET_MODEL(s, irq) (s)->irq_state[irq].model = 1
#define VGIC_CLEAR_MODEL(s, irq) (s)->irq_state[irq].model = 0
#define VGIC_TEST_MODEL(s, irq) (s)->irq_state[irq].model
#define VGIC_SET_LEVEL(s, irq, cm) (s)->irq_state[irq].level |= (cm)
#define VGIC_CLEAR_LEVEL(s, irq, cm) (s)->irq_state[irq].level &= ~(cm)
#define VGIC_TEST_LEVEL(s, irq, cm) \
	(((s)->irq_state[irq].level & (cm)) ? TRUE : FALSE)
#define VGIC_SET_TRIGGER(s, irq) (s)->irq_state[irq].trigger = 1
#define VGIC_CLEAR_TRIGGER(s, irq) (s)->irq_state[irq].trigger = 0
#define VGIC_TEST_RAW_TRIGGER(s, irq) (s)->irq_state[irq].trigger
#define VGIC_TEST_TRIGGER(s, irq) \
	(((irq < 32) || ((s)->irq_state[irq].host_irq != UINT_MAX)) ? \
	1 : (s)->irq_state[irq].trigger)
#define VGIC_GET_PRIORITY(s, irq, cpu) \
	(((irq) < 32) ? (s)->priority1[irq][cpu] : (s)->priority2[(irq) - 32])
#define VGIC_TARGET(s, irq) (s)->irq_target[irq]
#define VGIC_SET_HOST_IRQ(s, irq, hirq) (s)->irq_state[irq].host_irq = (hirq)
#define VGIC_GET_HOST_IRQ(s, irq) (s)->irq_state[irq].host_irq

#define VGIC_TEST_EISR(eisr, lr) \
	((eisr)[((lr) >> 5) & 0x1] & (1 << ((lr) & 0x1f)))
#define VGIC_SET_EISR(vs, lr) \
	((eisr)[((lr) >> 5) & 0x1] |= (1 << ((lr) & 0x1f)))
#define VGIC_CLEAR_EISR(vs, lr) \
	((eisr)[((lr) >> 5) & 0x1] &= ~(1 << ((lr) & 0x1f)))

#define VGIC_TEST_ELRSR(elrsr, lr) \
	((elrsr)[((lr) >> 5) & 0x1] & (1 << ((lr) & 0x1f)))
#define VGIC_SET_ELRSR(vs, lr) \
	((elrsr)[((lr) >> 5) & 0x1] |= (1 << ((lr) & 0x1f)))
#define VGIC_CLEAR_ELRSR(vs, lr) \
	((elrsr)[((lr) >> 5) & 0x1] &= ~(1 << ((lr) & 0x1f)))

#define VGIC_HAVE_LR_USED(vs) ((vs)->lr_used_count)
#define VGIC_TEST_LR_USED(vs, lr) \
	((vs)->lr_used[((lr) >> 5)] & (1 << ((lr) & 0x1f)))
#define VGIC_SET_LR_USED(vs, lr) \
	do { \
		(vs)->lr_used[((lr) >> 5)] |= (1 << ((lr) & 0x1f)); \
		(vs)->lr_used_count++;	\
	} while (0)
#define VGIC_CLEAR_LR_USED(vs, lr) \
	do { \
		(vs)->lr_used[((lr) >> 5)] &= ~(1 << ((lr) & 0x1f)); \
		(vs)->lr_used_count--;	\
	} while (0)

#define VGIC_SET_LR_MAP(vs, irq, lr) ((vs)->irq_lr[irq] = (lr))
#define VGIC_GET_LR_MAP(vs, irq) ((vs)->irq_lr[irq])

/* Queue interrupt to given VCPU
 * Note: Must be called only when given VCPU is current VCPU
 * Note: Must be called with VGIC distributor lock held
 */
static bool __vgic_queue_irq(struct vgic_guest_state *s,
			     struct vgic_vcpu_state *vs,
			     u32 irq)
{
	register u32 hirq, lr;
	struct vgic_lr lrv = { .virtid = 0, .physid = 0,
			       .prio = 0, .flags = 0 };

	DPRINTF("%s: IRQ=%d VCPU=%s\n", __func__, irq, vs->vcpu->name);

	lr = VGIC_GET_LR_MAP(vs, irq);
	if ((lr < vgich.params.lr_cnt) &&
	    VGIC_TEST_LR_USED(vs, lr)) {
		vgich.ops.get_lr(lr, &lrv, VGIC_MODEL_V2);
		if (lrv.virtid == irq) {
			lrv.flags |= VGIC_LR_STATE_PENDING;
			vgich.ops.set_lr(lr, &lrv, VGIC_MODEL_V2);
			return TRUE;
		}
	}

	/* Try to use another LR for this interrupt */
	for (lr = 0; lr < vgich.params.lr_cnt; lr++) {
		if (!VGIC_TEST_LR_USED(vs, lr)) {
			break;
		}
	}
	if (lr >= vgich.params.lr_cnt) {
		DPRINTF("%s: LR overflow IRQ=%d VCPU=%s\n",
			__func__, irq, vs->vcpu->name);
		return FALSE;
	}

	DPRINTF("%s: VCPU=%s LR%d allocated for IRQ%d\n",
		__func__, vs->vcpu->name, lr, irq);
	VGIC_SET_LR_MAP(vs, irq, lr);
	VGIC_SET_LR_USED(vs, lr);

	lrv.virtid = irq;
	lrv.physid = 0;
	lrv.prio = 0;
	lrv.flags = VGIC_LR_STATE_PENDING;
	hirq = VGIC_GET_HOST_IRQ(s, irq);
	if (hirq != UINT_MAX) {
		lrv.flags |= VGIC_LR_HW;
		lrv.physid = hirq;
	}

	vgich.ops.set_lr(lr, &lrv, VGIC_MODEL_V2);

	return TRUE;
}

/* Queue software generated interrupt to given VCPU
 * Note: Must be called only when given VCPU is current VCPU
 * Note: Must be called with VGIC distributor lock held
 */
static bool __vgic_queue_sgi(struct vgic_guest_state *s,
			     struct vgic_vcpu_state *vs,
			     u32 irq)
{
	u32 c, source = s->sgi_source[vs->vcpu->subid][irq];

	for (c = 0; c < VGIC_NUM_CPU(s); c++) {
		if (!(source & (1 << c))) {
			continue;
		}
		if (__vgic_queue_irq(s, vs, irq)) {
			source &= ~(1 << c);
		}
	}

	s->sgi_source[vs->vcpu->subid][irq] = source;

	if (!source) {
		VGIC_CLEAR_PENDING(s, irq, (1 << vs->vcpu->subid));
		return TRUE;
	}

	return FALSE;
}

/* Queue hardware interrupt to given VCPU
 * Note: Must be called only when given VCPU is current VCPU
 * Note: Must be called with VGIC distributor lock held
 */
static bool __vgic_queue_hwirq(struct vgic_guest_state *s,
			       struct vgic_vcpu_state *vs,
			       u32 irq)
{
	u32 cm = (1 << vs->vcpu->subid);

	if (VGIC_TEST_ACTIVE(s, irq, cm)) {
		return TRUE; /* level interrupt, already queued */
	}

	if (__vgic_queue_irq(s, vs, irq)) {
		VGIC_CLEAR_PENDING(s, irq, cm);
		if (!VGIC_TEST_TRIGGER(s, irq)) {
			VGIC_SET_ACTIVE(s, irq, cm);
		}

		return TRUE;
	}

	return FALSE;
}

/* Check for interrupt pending to the VCPU
 * Note: Must be called with VGIC distributor lock held
 */
static bool __vgic_vcpu_irq_pending(struct vgic_guest_state *s,
				    struct vgic_vcpu_state *vs)
{
	u32 i, mask;

	if (!s->enabled) {
		return false;
	}

	DPRINTF("%s: VCPU=%s\n", __func__, vs->vcpu->name);

	mask = s->irq_pending[vs->vcpu->subid][0];
	mask &= s->irq_enabled[vs->vcpu->subid][0];
	if (mask) {
		return true;
	}

	for (i = 1; i < VGIC_MAX_NIRQ / 32; i++) {
		mask = s->irq_pending[vs->vcpu->subid][i];
		mask &= s->irq_enabled[vs->vcpu->subid][i];
		if (mask) {
			return true;
		}
	}

	return VGIC_HAVE_LR_USED(vs) ? true : false;
}

/* Flush VGIC state to VGIC HW for given VCPU
 * Note: Must be called only when given VCPU is current VCPU
 * Note: Must be called with VGIC distributor lock held
 */
static void __vgic_flush_vcpu_hwstate(struct vgic_guest_state *s,
				      struct vgic_vcpu_state *vs)
{
	bool overflow = FALSE;
	u32 i, irq, mask;

	if (!s->enabled) {
		return;
	}

	DPRINTF("%s: VCPU=%s\n", __func__, vs->vcpu->name);

	mask = s->irq_pending[vs->vcpu->subid][0];
	mask &= s->irq_enabled[vs->vcpu->subid][0];
	if (mask) {
		for (irq = 0; irq < 16; irq++) {
			if (!(mask & (0x1 << irq))) {
				continue;
			}

			if (!__vgic_queue_sgi(s, vs, irq)) {
				overflow = TRUE;
				goto done;
			}
		}

		for (irq = 16; irq < 32; irq++) {
			if (!(mask & (0x1 << irq))) {
				continue;
			}

			if (!__vgic_queue_hwirq(s, vs, irq)) {
				overflow = TRUE;
				goto done;
			}
		}
	}

	for (i = 1; i < VGIC_MAX_NIRQ / 32; i++) {
		mask = s->irq_pending[vs->vcpu->subid][i];
		mask &= s->irq_enabled[vs->vcpu->subid][i];
		if (!mask) {
			continue;
		}

		for (irq = 0; irq < 32; irq++) {
			if (!(mask & (0x1 << irq))) {
				continue;
			}

			if (!__vgic_queue_hwirq(s, vs, i*32 + irq)) {
				overflow = TRUE;
				goto done;
			}
		}
	}

done:
	if (overflow) {
		vgich.ops.enable_underflow();
	}
}

/* Sync current VCPU VGIC state with HW state
 * Note: Must be called only when given VCPU is current VCPU
 * Note: Must be called with VGIC distributor lock held
 */
static void __vgic_sync_vcpu_hwstate(struct vgic_guest_state *s,
				     struct vgic_vcpu_state *vs)
{
	u32 elrsr[2], eisr[2];
	struct vgic_lr lrv = { .virtid = 0, .physid = 0,
			       .prio = 0, .flags = 0 };
	register u32 lr, irq, cm = (1 << vs->vcpu->subid);

	/* If no LR used then skip */
	if (!VGIC_HAVE_LR_USED(vs)) {
		return;
	}

	/* Useful debug prints */
	DPRINTF("%s: VCPU=%s LR_USED0=0x%08x LR_USED1=0x%08x\n",
		__func__, vs->vcpu->name, vs->lr_used[0], vs->lr_used[1]);

	/* Read EOI LR status registers */
	vgich.ops.read_eisr(&eisr[0], &eisr[1]);
	DPRINTF("%s: VCPU=%s EISR0=0x%08x EISR1=0x%08x\n",
		__func__, vs->vcpu->name, eisr[0], eisr[1]);

	/* Read empty LR status registers */
	vgich.ops.read_elrsr(&elrsr[0], &elrsr[1]);
	DPRINTF("%s: VCPU=%s ELRSR0=0x%08x ELRSR1=0x%08x\n",
		__func__, vs->vcpu->name, elrsr[0], elrsr[1]);

	/* Re-claim empty LR registers */
	elrsr[0] |= eisr[0];
	elrsr[1] |= eisr[1];
	elrsr[0] &= vs->lr_used[0];
	elrsr[1] &= vs->lr_used[1];
	for (lr = 0; lr < vgich.params.lr_cnt; lr++) {
		if (!VGIC_TEST_ELRSR(elrsr, lr)) {
			continue;
		}

		/* Read and clear the LR register */
		vgich.ops.get_lr(lr, &lrv, VGIC_MODEL_V2);
		vgich.ops.clear_lr(lr);

		/* Determine irq number */
		irq = lrv.virtid;
		DPRINTF("%s: VCPU=%s LR%d IRQ%d done\n",
			__func__, vs->vcpu->name, lr, irq);

		/* Should be a valid irq number */
		BUG_ON(irq >= VGIC_MAX_NIRQ);

		/* Mark level triggered interrupts as pending if
		 * they are still raised.
		 */
		if (!VGIC_TEST_TRIGGER(s, irq)) {
			/* Clear active bit */
			VGIC_CLEAR_ACTIVE(s, irq, cm);

			/* Update pending bit */
			if (VGIC_TEST_LEVEL(s, irq, cm) &&
			    (VGIC_TARGET(s, irq) & cm) != 0) {
				VGIC_SET_PENDING(s, irq, cm);
			} else {
				VGIC_CLEAR_PENDING(s, irq, cm);
			}
		}

		/* Mark this LR as free */
		VGIC_CLEAR_LR_USED(vs, lr);

		/* Map irq to unknown LR */
		VGIC_SET_LR_MAP(vs, irq, VGIC_LR_UNKNOWN);
	}
}

/* Sync & Flush VCPU VGIC state with HW state
 * Note: Must be called only when given VCPU is current VCPU
 * Note: Must be called with VGIC distributor lock held
 */
static void __vgic_sync_and_flush_vcpu(struct vgic_guest_state *s,
					struct vgic_vcpu_state *vs)
{
	/* The VGIC HW state may have changed when the
	 * VCPU was running hence, sync VGIC VCPU state.
	 */
	__vgic_sync_vcpu_hwstate(s, vs);

	/* Flush VGIC state changes to VGIC HW for
	 * reflecting latest changes while, the VCPU
	 * was not running.
	 */
	__vgic_flush_vcpu_hwstate(s, vs);
}

/* Assert/Deassert VCPU interrupt */
static void vgic_vcpu_irq_update(struct vmm_vcpu *vcpu, bool irq_pending)
{
	if (irq_pending) {
		vmm_vcpu_irq_assert(vcpu, CPU_EXTERNAL_IRQ, 0x0);
	} else {
		vmm_vcpu_irq_deassert(vcpu, CPU_EXTERNAL_IRQ);
	}
}

/* Process IRQ asserted by device emulation framework */
static void __vgic_irq_handle(struct vgic_guest_state *s, u32 irq, int cpu,
			      bool two_levels, int level0, int level1)
{
	irq_flags_t flags;
	bool irq_pending;
	int i, cm, target, level;
	struct vgic_vcpu_state *vs;

	/* Lock VGIC distributor state */
	vmm_spin_lock_irqsave_lite(&s->dist_lock, flags);

	if (!s->enabled) {
		vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);
		return;
	}

	if (irq < 32) {
		/* In case of PPIs and SGIs */
		cm = target = (1 << cpu);
	} else {
		/* In case of SPIs */
		cm = VGIC_ALL_CPU_MASK(s);
		target = VGIC_TARGET(s, irq);
		for (cpu = 0; cpu < VGIC_NUM_CPU(s); cpu++) {
			if (target & (1 << cpu)) {
				break;
			}
		}
		if (VGIC_NUM_CPU(s) <= cpu) {
			vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);
			return;
		}
	}

	/* Find out VCPU pointer */
	BUG_ON(cpu < 0);
	vs = &s->vstate[cpu];

	/* Check level changes */
	for (i = 0; i < ((two_levels) ? 2 : 1); i++) {
		level = (i == 0) ? level0 : level1;

		/* If level not changed then skip */
		if (level == VGIC_TEST_LEVEL(s, irq, cm)) {
			continue;
		}

		/* Debug print */
		DPRINTF("%s: irq=%d cpu=%d level=%d\n",
			__func__, irq, cpu, level);

		/* Update IRQ state */
		if (level) {
			VGIC_SET_LEVEL(s, irq, cm);
			VGIC_SET_PENDING(s, irq, target);
		} else {
			VGIC_CLEAR_LEVEL(s, irq, cm);
		}
	}

	/* Check for pending interrupts */
	irq_pending = __vgic_vcpu_irq_pending(s, vs);

	/* Sync & Flush VGIC state changes to VGIC HW */
	if (vmm_scheduler_current_vcpu() == vs->vcpu) {
		__vgic_sync_and_flush_vcpu(s, vs);
	}

	/* Unlock VGIC distributor state */
	vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);

	/* Update VCPU irqs */
	vgic_vcpu_irq_update(vs->vcpu, irq_pending);
}

/* Process single IRQ level change */
static void vgic_irq_handle(u32 irq, int cpu, int level, void *opaque)
{
	__vgic_irq_handle(opaque, irq, cpu, FALSE, level, 0);
}

/* Process two IRQ level changes */
static void vgic_irq_handle2(u32 irq, int cpu, int level0,
			     int level1, void *opaque)
{
	__vgic_irq_handle(opaque, irq, cpu, TRUE, level0, level1);
}

/* Process map_host2guest request from device emulation framework */
void vgic_irq_map_host2guest(u32 irq, u32 host_irq, void *opaque)
{
	irq_flags_t flags;
	struct vgic_guest_state *s = opaque;

	/* Lock VGIC distributor state */
	vmm_spin_lock_irqsave_lite(&s->dist_lock, flags);

	/* Skip invalid irq */
	if (VGIC_NUM_IRQ(s) <= irq) {
		goto done;
	}

	/* Update guest state */
	VGIC_SET_HOST_IRQ(s, irq, host_irq);

done:
	/* Unlock VGIC distributor state */
	vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);
}

/* Process unmap_host2guest request from device emulation framework */
void vgic_irq_unmap_host2guest(u32 irq, void *opaque)
{
	irq_flags_t flags;
	struct vgic_guest_state *s = opaque;

	/* Lock VGIC distributor state */
	vmm_spin_lock_irqsave_lite(&s->dist_lock, flags);

	/* Skip invalid irq */
	if (VGIC_NUM_IRQ(s) <= irq) {
		goto done;
	}

	/* Update guest state */
	VGIC_SET_HOST_IRQ(s, irq, UINT_MAX);

done:
	/* Unlock VGIC distributor state */
	vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);
}

/* Handle maintainence IRQ generated by VGIC */
static vmm_irq_return_t vgic_maint_irq(int irq_no, void *dev)
{
	irq_flags_t flags;
	bool irq_pending;
	struct vgic_guest_state *s;
	struct vgic_vcpu_state *vs;
	struct vmm_vcpu *vcpu = vmm_scheduler_current_vcpu();

	/* Clear underflow interrupt if enabled */
	if (vgich.ops.check_underflow()) {
		vgich.ops.disable_underflow();
	}

	/* We should not get this interrupt when not
	 * running a VGIC enabled normal VCPU.
	 */
	BUG_ON(!vcpu);
	BUG_ON(!vcpu->is_normal);
	BUG_ON(!arm_vgic_avail(vcpu));

	/* Get VGIC state pointers */
	s = arm_vgic_priv(vcpu);
	vs = &s->vstate[vcpu->subid];

	/* Lock VGIC distributor state */
	vmm_spin_lock_irqsave_lite(&s->dist_lock, flags);

	/* Sync & Flush VGIC state changes to VGIC HW */
	__vgic_sync_and_flush_vcpu(s, vs);

	/* Check for pending interrupts */
	irq_pending = __vgic_vcpu_irq_pending(s, vs);

	/* Unlock VGIC distributor state */
	vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);

	/* Update VCPU irqs */
	vgic_vcpu_irq_update(vs->vcpu, irq_pending);

	return VMM_IRQ_HANDLED;
}

/* Check whether interrupt for the VCPU is pending or not */
static bool vgic_irq_pending(void *vcpu_ptr)
{
	irq_flags_t flags;
	struct vgic_guest_state *s;
	struct vgic_vcpu_state *vs;
	struct vmm_vcpu *vcpu = vcpu_ptr;
	bool intr_pending = FALSE;

	BUG_ON(!vcpu);

	s = arm_vgic_priv(vcpu);
	vs = &s->vstate[vcpu->subid];

	/* Lock VGIC distributor state */
	vmm_spin_lock_irqsave_lite(&s->dist_lock, flags);

	/* Sync & Flush VGIC state changes to VGIC HW */
	__vgic_sync_and_flush_vcpu(s, vs);

	/* Check for pending interrupts */
	intr_pending = __vgic_vcpu_irq_pending(s, vs);

	/* Unlock VGIC distributor state */
	vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);

	return intr_pending;
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

	/* Lock VGIC distributor state */
	vmm_spin_lock_irqsave_lite(&s->dist_lock, flags);

	/* The VGIC HW state may have changed when the
	 * VCPU was running hence, sync VGIC VCPU state.
	 */
	__vgic_sync_vcpu_hwstate(s, vs);

	/* Unlock VGIC distributor state */
	vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);

	/* Save VGIC HW registers for VCPU */
	vgich.ops.save_state(&vs->hw, VGIC_MODEL_V2);
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

	/* Restore VGIC HW registers for VCPU */
	vgich.ops.restore_state(&vs->hw, VGIC_MODEL_V2);

	/* Lock VGIC distributor state */
	vmm_spin_lock_irqsave_lite(&s->dist_lock, flags);

	/* Flush VGIC state changes to VGIC HW for
	 * reflecting latest changes while, the VCPU
	 * was not running.
	 */
	__vgic_flush_vcpu_hwstate(s, vs);

	/* Unlock VGIC distributor state */
	vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);
}

static int __vgic_dist_readb(struct vgic_guest_state *s, int cpu,
			     u32 offset, u8 *dst)
{
	u32 done = 0, i, irq, mask;

	if (!s || !dst) {
		return VMM_EFAIL;
	}

	done = 1;
	switch (offset >> 8) {
	case 0x0: /* Control */
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
		default:
			done = 0;
			break;
		};
		break;
	case 0x1: /* Enable */
		irq = (offset & 0x7F) * 8;
		if (VGIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		*dst = 0;
		for (i = 0; i < 8; i++) {
			*dst |= VGIC_TEST_ENABLED(s, irq + i, (1 << cpu)) ?
				(1 << i) : 0x0;
		}
		break;
	case 0x2: /* Pending */
		irq = (offset & 0x7F) * 8;
		if (VGIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		mask = (irq < 32) ? (1 << cpu) : VGIC_ALL_CPU_MASK(s);
		*dst = 0;
		for (i = 0; i < 8; i++) {
			*dst |= VGIC_TEST_PENDING(s, irq + i, mask) ?
				(1 << i) : 0x0;
		}
		break;
	case 0x3: /* Active */
		irq = (offset & 0x7F) * 8;
		if (VGIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		mask = (irq < 32) ? (1 << cpu) : VGIC_ALL_CPU_MASK(s);
		*dst = 0;
		for (i = 0; i < 8; i++) {
			*dst |= VGIC_TEST_ACTIVE(s, irq + i, mask) ?
				(1 << i) : 0x0;
		}
		break;
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
			if (VGIC_TEST_RAW_TRIGGER(s, irq + i)) {
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

	return (done) ? VMM_OK : VMM_EFAIL;
}

static int __vgic_dist_writeb(struct vgic_guest_state *s, int cpu,
			      u32 offset, u8 src)
{
	u32 done = 0, i, irq, mask, cm;

	if (!s) {
		return VMM_EFAIL;
	}

	done = 1;
	switch (offset >> 8) {
	case 0x0: /* Control */
		switch (offset - (offset & 0x3)) {
		case 0x000: /* Distributor control */
			if (offset == 0x000) {
				s->enabled = src & 0x1;
			}
			break;
		case 0x004: /* Controller type */
			/* Ignored. */
			break;
		default:
			done = 0;
			break;
		};
		break;
	case 0x1: /* Enable */
		irq = (offset & 0x7F) * 8;
		if (VGIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		if (!(offset & 0x80)) { /* Set-enableX */
			if (irq < 16) {
				src = 0xFF;
			}
			for (i = 0; i < 8; i++) {
				if (!(src & (1 << i))) {
					continue;
				}
				mask = ((irq + i) < 32) ?
					(1 << cpu) : VGIC_TARGET(s, (irq + i));
				cm = ((irq + i) < 32) ?
					(1 << cpu) : VGIC_ALL_CPU_MASK(s);
				VGIC_SET_ENABLED(s, irq + i, cm);
				if (cm == VGIC_ALL_CPU_MASK(s)) {
					vmm_devemu_notify_irq_enabled(
						s->guest, irq + i, -1);
				} else {
					vmm_devemu_notify_irq_enabled(
						s->guest, irq + i, cpu);
				}
				/* If a raised level triggered IRQ enabled
				 * then mark is as pending.
				 */
				if (VGIC_TEST_LEVEL(s, (irq + i), mask) &&
				    !VGIC_TEST_TRIGGER(s, (irq + i))) {
					VGIC_SET_PENDING(s, (irq + i), mask);
				}
			}
		} else { /* Clear-enableX */
			if (irq < 16) {
				src = 0x00;
			}
			for (i = 0; i < 8; i++) {
				if (!(src & (1 << i))) {
					continue;
				}
				cm = ((irq + i) < 32) ?
					(1 << cpu) : VGIC_ALL_CPU_MASK(s);
				VGIC_CLEAR_ENABLED(s, irq + i, cm);
				if (cm == VGIC_ALL_CPU_MASK(s)) {
					vmm_devemu_notify_irq_disabled(
						s->guest, irq + i, -1);
				} else {
					vmm_devemu_notify_irq_disabled(
						s->guest, irq + i, cpu);
				}
			}
		}
		break;
	case 0x2: /* Pending */
		irq = (offset & 0x7F) * 8;
		if (VGIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		if (!(offset & 0x80)) { /* Set-pendingX */
			if (irq < 16) {
				src = 0x00;
			}
			for (i = 0; i < 8; i++) {
				if (!(src & (1 << i))) {
					continue;
				}
				mask = VGIC_TARGET(s, irq + i);
				VGIC_SET_PENDING(s, irq + i, mask);
			}
		} else { /* Clear-pendingX */
			/* ??? This currently clears the pending bit for
			 * all CPUs, even for per-CPU interrupts.  It's
			 * unclear whether this is the corect behavior.
			 */
			mask = VGIC_ALL_CPU_MASK(s);
			for (i = 0; i < 8; i++) {
				if (!(src & (1 << i))) {
					continue;
				}
				VGIC_CLEAR_PENDING(s, irq + i, mask);
			}
		}
		break;
	case 0x3: /* Active */
		/* Ignore. */
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

	return (done) ? VMM_OK : VMM_EFAIL;
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

	vmm_spin_lock_irqsave_lite(&s->dist_lock, flags);

	*dst = 0;
	for (i = 0; i < 4; i++) {
		if ((rc = __vgic_dist_readb(s, cpu, offset + i, &val))) {
				break;
		}
		*dst |= val << (i * 8);
	}

	vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);

	return VMM_OK;
}

static int vgic_dist_write(struct vgic_guest_state *s, int cpu,
			   u32 offset, u32 src_mask, u32 src)
{
	int rc = VMM_OK;
	u32 i, irq, sgi_mask, update_mask = 0;
	irq_flags_t flags;
	struct vgic_vcpu_state *vs;

	if (!s) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave_lite(&s->dist_lock, flags);

	vs = &s->vstate[cpu];

	if (offset == 0xF00) {
		/* Software Interrupt */
		irq = src & 0x3FF;
		switch ((src >> 24) & 3) {
		case 0:
			sgi_mask = (src >> 16) & VGIC_ALL_CPU_MASK(s);
			break;
		case 1:
			sgi_mask = VGIC_ALL_CPU_MASK(s) ^ (1 << cpu);
			break;
		case 2:
			sgi_mask = 1 << cpu;
			break;
		default:
			sgi_mask = VGIC_ALL_CPU_MASK(s);
			break;
		};
		VGIC_SET_PENDING(s, irq, sgi_mask);
		for (i = 0; (irq < 16) && (i < VGIC_NUM_CPU(s)); i++) {
			if (!(sgi_mask & (1 << i))) {
				continue;
			}
			if (s->vstate[i].vcpu->subid == vs->vcpu->subid) {
				continue;
			}
			s->sgi_source[i][irq] |= (1 << cpu);
			DPRINTF("%s: VCPU=%s IRQ%d SRC_ID=0x%d pending\n",
				__func__, s->vstate[i].vcpu->name, irq, cpu);
		}
	} else {
		sgi_mask = 0x0;
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

	/* Sync & Flush VGIC state changes to VGIC HW */
	__vgic_sync_and_flush_vcpu(s, vs);

	/* Check for pending interrupts */
	for (i = 0; i < VGIC_NUM_CPU(s); i++) {
		if (__vgic_vcpu_irq_pending(s, &s->vstate[i]))
			update_mask |= (1U << i);
	}

	vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);

	/* Update VCPU irqs */
	for (i = 0; i < VGIC_NUM_CPU(s); i++) {
		vgic_vcpu_irq_update(s->vstate[i].vcpu,
				     (update_mask & (1U << i)) ? TRUE : FALSE);
	}

	return rc;
}

static int vgic_dist_reg_read(struct vgic_guest_state *s,
			      u32 offset, u32 *dst)
{
	struct vmm_vcpu *vcpu;

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->id != vcpu->guest->id) {
		return VMM_EFAIL;
	}

	return vgic_dist_read(s, vcpu->subid, offset & 0xFFC, dst);
}

static int vgic_dist_reg_write(struct vgic_guest_state *s,
			       u32 offset, u32 regmask, u32 regval)
{
	struct vmm_vcpu *vcpu;

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->id != vcpu->guest->id) {
		return VMM_EFAIL;
	}

	return vgic_dist_write(s, vcpu->subid,
			       offset & 0xFFC, regmask, regval);
}

static int vgic_dist_emulator_read8(struct vmm_emudev *edev,
				    physical_addr_t offset,
				    u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = vgic_dist_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int vgic_dist_emulator_read16(struct vmm_emudev *edev,
				     physical_addr_t offset,
				     u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = vgic_dist_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int vgic_dist_emulator_read32(struct vmm_emudev *edev,
				     physical_addr_t offset,
				     u32 *dst)
{
	return vgic_dist_reg_read(edev->priv, offset, dst);
}

static int vgic_dist_emulator_write8(struct vmm_emudev *edev,
				     physical_addr_t offset,
				     u8 src)
{
	return vgic_dist_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int vgic_dist_emulator_write16(struct vmm_emudev *edev,
				      physical_addr_t offset,
				      u16 src)
{
	return vgic_dist_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int vgic_dist_emulator_write32(struct vmm_emudev *edev,
				      physical_addr_t offset,
				      u32 src)
{
	return vgic_dist_reg_write(edev->priv, offset, 0x00000000, src);
}

static int vgic_dist_emulator_reset(struct vmm_emudev *edev)
{
	u32 i, j;
	irq_flags_t flags;
	struct vgic_guest_state *s = edev->priv;

	DPRINTF("%s: guest=%s\n", __func__, s->guest->name);

	vmm_spin_lock_irqsave_lite(&s->dist_lock, flags);

	/* Reset context for all VCPUs
	 *
	 * 1. Force VMCR to zero. This will restore the
	 * binary points to reset values.
	 * 2. Deactivate host/HW interrupts for pending LRs.
	 */
	for (i = 0; i < VGIC_NUM_CPU(s); i++) {
		vgich.ops.reset_state(&s->vstate[i].hw, VGIC_MODEL_V2);
		s->vstate[i].lr_used_count = 0x0;
		for (j = 0; j < ((vgich.params.lr_cnt + 31) / 32); j++) {
			s->vstate[i].lr_used[j] = 0x0;
		}
		for (j = 0; j < VGIC_NUM_IRQ(s); j++) {
			VGIC_SET_LR_MAP(&s->vstate[i], j, VGIC_LR_UNKNOWN);
		}
	}

	/* Clear SGI sources */
	for (i = 0; i < VGIC_NUM_CPU(s); i++) {
		for (j = 0; j < 16; j++) {
			s->sgi_source[i][j] = 0x0;
		}
	}

	/* We should not reset level as guest IRQ might
	 * have been raised already.
	 */
	for (i = 0; i < VGIC_NUM_IRQ(s); i++) {
		VGIC_CLEAR_ENABLED(s, i, VGIC_ALL_CPU_MASK(s));
		vmm_devemu_notify_irq_disabled(s->guest, i, -1);
		VGIC_CLEAR_PENDING(s, i, VGIC_ALL_CPU_MASK(s));
		VGIC_CLEAR_ACTIVE(s, i, VGIC_ALL_CPU_MASK(s));
		VGIC_CLEAR_MODEL(s, i);
		VGIC_CLEAR_TRIGGER(s, i);
	}

	/* Reset software generated interrupts */
	for (i = 0; i < 16; i++) {
		VGIC_SET_ENABLED(s, i, VGIC_ALL_CPU_MASK(s));
		vmm_devemu_notify_irq_enabled(s->guest, i, -1);
		VGIC_SET_TRIGGER(s, i);
	}

	/* Disable guest dist interface */
	s->enabled = 0;

	vmm_spin_unlock_irqrestore_lite(&s->dist_lock, flags);

	return VMM_OK;
}

static struct vmm_devemu_irqchip vgic_irqchip = {
	.name = "VGIC",
	.handle = vgic_irq_handle,
	.handle2 = vgic_irq_handle2,
	.map_host2guest = vgic_irq_map_host2guest,
	.unmap_host2guest = vgic_irq_unmap_host2guest,
};

static struct vgic_guest_state *vgic_state_alloc(const char *name,
						 struct vmm_guest *guest,
						 u32 num_cpu,
						 u32 num_irq,
						 u32 parent_irq)
{
	u32 i;
	struct vmm_vcpu *vcpu;
	struct vgic_guest_state *s = NULL;

	/* Alloc VGIC state */
	s = vmm_zalloc(sizeof(struct vgic_guest_state));
	if (!s) {
		return NULL;
	}

	s->guest = guest;

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

	for (i = 0; i < VGIC_NUM_IRQ(s); i++) {
		VGIC_SET_HOST_IRQ(s, i, UINT_MAX);
	}

	for (i = 0; i < VGIC_NUM_CPU(s); i++) {
		s->vstate[i].vcpu = vmm_manager_guest_vcpu(guest, i);
		s->vstate[i].parent_irq = parent_irq;
	}

	INIT_SPIN_LOCK(&s->dist_lock);

	/* Register guest irqchip */
	for (i = 0; i < VGIC_NUM_IRQ(s); i++) {
		vmm_devemu_register_irqchip(guest, i, &vgic_irqchip, s);
	}

	/* Setup save/restore hooks */
	list_for_each_entry(vcpu, &guest->vcpu_list, head) {
		arm_vgic_setup(vcpu,
			vgic_save_vcpu_context,
			vgic_restore_vcpu_context,
			vgic_irq_pending, s);
	}

	return s;
}

static int vgic_state_free(struct vgic_guest_state *s)
{
	u32 i;
	struct vmm_vcpu *vcpu;

	if (!s) {
		return VMM_EFAIL;
	}

	/* Cleanup save/restore hooks */
	list_for_each_entry(vcpu, &s->guest->vcpu_list, head) {
		arm_vgic_cleanup(vcpu);
	}

	/* Unregister guest irqchip */
	for (i = 0; i < s->num_irq; i++) {
		vmm_devemu_unregister_irqchip(s->guest, i, &vgic_irqchip, s);
	}

	/* Free VGIC state */
	vmm_free(s);

	return VMM_OK;
}

static int vgic_dist_emulator_probe(struct vmm_guest *guest,
				    struct vmm_emudev *edev,
				    const struct vmm_devtree_nodeid *eid)
{
	int rc;
	u32 parent_irq, num_irq;
	struct vgic_guest_state *s;

	if (!vgich.avail) {
		return VMM_ENODEV;
	}
	if (!vgich.params.can_emulate_gic_v2) {
		return VMM_ENODEV;
	}
	if (guest->vcpu_count > VGIC_MAX_NCPU) {
		return VMM_ENODEV;
	}

	rc = vmm_devtree_read_u32(edev->node, "parent_irq", &parent_irq);
	if (rc) {
		return rc;
	}

	if (vmm_devtree_read_u32(edev->node, "num_irq", &num_irq)) {
		num_irq = VGIC_MAX_NIRQ;
	}
	if (num_irq > VGIC_MAX_NIRQ) {
		num_irq = VGIC_MAX_NIRQ;
	}

	s = vgic_state_alloc(edev->node->name,
			     guest, guest->vcpu_count,
			     num_irq, parent_irq);
	if (!s) {
		return VMM_ENOMEM;
	}

	edev->priv = s;

	return VMM_OK;
}

static int vgic_dist_emulator_remove(struct vmm_emudev *edev)
{
	struct vgic_guest_state *s = edev->priv;

	if (!s) {
		return VMM_EFAIL;
	}

	vgic_state_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid vgic_dist_emuid_table[] = {
	{ .type = "pic", .compatible = "arm,vgic,dist", },
	{ /* end of list */ },
};

static struct vmm_emulator vgic_dist_emulator = {
	.name = "vgic-dist",
	.match_table = vgic_dist_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = vgic_dist_emulator_probe,
	.remove = vgic_dist_emulator_remove,
	.reset = vgic_dist_emulator_reset,
	.read8 = vgic_dist_emulator_read8,
	.write8 = vgic_dist_emulator_write8,
	.read16 = vgic_dist_emulator_read16,
	.write16 = vgic_dist_emulator_write16,
	.read32 = vgic_dist_emulator_read32,
	.write32 = vgic_dist_emulator_write32,
};

static int vgic_cpu_emulator_reset(struct vmm_emudev *edev)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static int vgic_cpu_emulator_probe(struct vmm_guest *guest,
				   struct vmm_emudev *edev,
				   const struct vmm_devtree_nodeid *eid)
{
	if (!vgich.avail) {
		return VMM_ENODEV;
	}
	if (!vgich.params.can_emulate_gic_v2) {
		return VMM_ENODEV;
	}
	if (guest->vcpu_count > VGIC_MAX_NCPU) {
		return VMM_ENODEV;
	}
	if (!(edev->reg->flags & VMM_REGION_REAL)) {
		return VMM_ENODEV;
	}

	return vmm_guest_overwrite_real_device_mapping(guest, edev->reg,
					VMM_REGION_GPHYS_START(edev->reg),
					vgich.params.vcpu_pa);
}

static int vgic_cpu_emulator_remove(struct vmm_emudev *edev)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static struct vmm_devtree_nodeid vgic_cpu_emuid_table[] = {
	{ .type = "pic",
	  .compatible = "arm,vgic,cpu",
	},
	{ /* end of list */ },
};

static struct vmm_emulator vgic_cpu_emulator = {
	.name = "vgic-cpu",
	.match_table = vgic_cpu_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = vgic_cpu_emulator_probe,
	.remove = vgic_cpu_emulator_remove,
	.reset = vgic_cpu_emulator_reset,
};

static void vgic_enable_maint_irq(void *arg0, void *arg1, void *arg3)
{
	int rc;
	u32 maint_irq = (u32)(unsigned long)arg0;

	rc = vmm_host_irq_register(maint_irq, "VGIC",
				   arg1, NULL);
	if (rc) {
		vmm_printf("%s: cpu=%d maint_irq=%d failed (error %d)\n",
			   __func__, vmm_smp_processor_id(), maint_irq, rc);
	}
}

static int __init vgic_emulator_init(void)
{
	int rc;

	vgich.avail = FALSE;

	rc = vgic_v2_probe(&vgich.ops, &vgich.params);
	if (rc == VMM_ENODEV) {
		rc = vgic_v3_probe(&vgich.ops, &vgich.params);
		if (rc == VMM_ENODEV) {
			vmm_printf("vgic: GIC node not found\n");
			rc = VMM_OK;
			goto fail;
		} else if (rc != VMM_OK) {
			vmm_printf("vgic: vgic_v3_probe() return error %d\n",
				   rc);
			goto fail;
		}
	} else if (rc != VMM_OK) {
		vmm_printf("vgic: vgic_v2_probe() return error %d\n", rc);
		goto fail;
	}

	rc = vmm_devemu_register_emulator(&vgic_dist_emulator);
	if (rc) {
		goto fail_unprobe;
	}

	rc = vmm_devemu_register_emulator(&vgic_cpu_emulator);
	if (rc) {
		goto fail_unreg_dist;
	}

	vmm_smp_ipi_async_call(cpu_online_mask, vgic_enable_maint_irq,
			       (void *)(unsigned long)vgich.params.maint_irq,
			       vgic_maint_irq, NULL);

	vmm_printf("vgic: emulator available\n");

	vgich.avail = TRUE;

	return VMM_OK;

fail_unreg_dist:
	vmm_devemu_unregister_emulator(&vgic_dist_emulator);
fail_unprobe:
	vgic_v2_remove(&vgich.ops, &vgich.params);
fail:
	vmm_printf("vgic: emulator not available\n");
	return rc;
}

static void vgic_disable_maint_irq(void *arg0, void *arg1, void *arg3)
{
	int rc;
	u32 maint_irq = (u32)(unsigned long)arg0;

	rc = vmm_host_irq_unregister(maint_irq, NULL);
	if (rc) {
		vmm_printf("%s: cpu=%d maint_irq=%d failed (error %d)\n",
			   __func__, vmm_smp_processor_id(), maint_irq, rc);
	}
}

static void __exit vgic_emulator_exit(void)
{
	if (!vgich.avail) {
		vmm_printf("%s: GIC node not found\n", __func__);
		return;
	}

	vmm_smp_ipi_async_call(cpu_online_mask,
			       vgic_disable_maint_irq,
			       (void *)(unsigned long)vgich.params.maint_irq,
			       NULL, NULL);

	vmm_devemu_unregister_emulator(&vgic_cpu_emulator);

	vmm_devemu_unregister_emulator(&vgic_dist_emulator);

	vgic_v2_remove(&vgich.ops, &vgich.params);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
