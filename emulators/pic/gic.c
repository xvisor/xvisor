/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file gic.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief GIC (Generic Interrupt Controller) Emulator.
 *
 * The source has been largely adapted from QEMU hw/intc/arm_gic.c
 * 
 * ARM Generic/Distributed Interrupt Controller. 
 *
 * Copyright (c) 2006-2007 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_vcpu_irq.h>
#include <vmm_devemu.h>
#include <libs/stringlib.h>
#include <emu/gic_emulator.h>

#define MODULE_DESC			"GICv2 Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			gic_emulator_init
#define	MODULE_EXIT			gic_emulator_exit

#define GIC_MAX_NCPU			8
#define GIC_MAX_NIRQ			128

struct memory_region {
	u32 offset;
	u16 length;
};

struct gic_irq_state {
	u32 enabled:GIC_MAX_NCPU;
	u32 pending:GIC_MAX_NCPU;
	u32 active:GIC_MAX_NCPU;
	u32 level:GIC_MAX_NCPU;
	u32 target:GIC_MAX_NCPU;
	u32 priority:8;
	u32 model:1; /* 0 = N:N, 1 = 1:N */
	u32 trigger:1; /* nonzero = edge triggered.  */
};

struct gic_cpu_state {
	vmm_rwlock_t cpu_lock;
	u8 parent_irq;
	bool enabled;
	u16 priority_mask;
	u16 running_irq;
	u16 running_priority;
	u16 current_pending;
	u16 last_active[GIC_MAX_NIRQ];
	u8 priority[32];
};

struct gic_state {
	struct vmm_guest *guest;

	/* Configuration */
	enum gic_type type;
	bool is_child_pic;

	/* Chip Info */
	bool enabled;
	u8 id[8];

	/* CPU Interface */
	u8 num_cpu;
	struct memory_region cpu;
	struct gic_cpu_state cpu_state[GIC_MAX_NCPU];

	/* Distribution Control */
	u8 num_irq;
	u8 base_irq;
	struct memory_region dist;
	vmm_rwlock_t dist_lock;
	struct gic_irq_state irq_state[GIC_MAX_NIRQ];
};

#define GIC_ALL_CPU_MASK(s) ((1 << (s)->num_cpu) - 1)
#define GIC_NUM_CPU(s) ((s)->num_cpu)
#define GIC_NUM_IRQ(s) ((s)->num_irq)
#define GIC_BASE_IRQ(s) ((s)->base_irq)
#define GIC_SET_ENABLED(s, irq, cm) (s)->irq_state[irq].enabled |= (cm)
#define GIC_CLEAR_ENABLED(s, irq, cm) (s)->irq_state[irq].enabled &= ~(cm)
#define GIC_TEST_ENABLED(s, irq, cm) ((s)->irq_state[irq].enabled & (cm))
#define GIC_SET_PENDING(s, irq, cm) (s)->irq_state[irq].pending |= (cm)
#define GIC_CLEAR_PENDING(s, irq, cm) (s)->irq_state[irq].pending &= ~(cm)
#define GIC_TEST_PENDING(s, irq, cm) ((s)->irq_state[irq].pending & (cm))
#define GIC_SET_ACTIVE(s, irq, cm) (s)->irq_state[irq].active |= (cm)
#define GIC_CLEAR_ACTIVE(s, irq, cm) (s)->irq_state[irq].active &= ~(cm)
#define GIC_TEST_ACTIVE(s, irq, cm) ((s)->irq_state[irq].active & (cm))
#define GIC_SET_MODEL(s, irq) (s)->irq_state[irq].model = 1
#define GIC_CLEAR_MODEL(s, irq) (s)->irq_state[irq].model = 0
#define GIC_TEST_MODEL(s, irq) (s)->irq_state[irq].model
#define GIC_SET_LEVEL(s, irq, cm) (s)->irq_state[irq].level |= (cm)
#define GIC_CLEAR_LEVEL(s, irq, cm) (s)->irq_state[irq].level &= ~(cm)
#define GIC_TEST_LEVEL(s, irq, cm) ((s)->irq_state[irq].level & (cm))
#define GIC_SET_TRIGGER(s, irq) (s)->irq_state[irq].trigger = 1
#define GIC_CLEAR_TRIGGER(s, irq) (s)->irq_state[irq].trigger = 0
#define GIC_TEST_TRIGGER(s, irq) (s)->irq_state[irq].trigger
#define GIC_GET_PRIORITY(s, irq, cpu) \
  (((irq) < 32) ? (s)->cpu_state[cpu].priority[irq] : (s)->irq_state[irq].priority)
#define GIC_TARGET(s, irq) (s)->irq_state[irq].target

/* Update interrupt status after enabled or pending bits have been changed. */
static void gic_update(struct gic_state *s)
{
	irq_flags_t dist_flags, cpu_flags;
	int best_irq, best_prio;
	int irq, level, cpu, cm;
	struct vmm_vcpu *vcpu;
	struct gic_cpu_state *cpu_state;

	for (cpu = 0; cpu < GIC_NUM_CPU(s); cpu++) {
		cpu_state = &s->cpu_state[cpu];
		cm = 1 << cpu;

		vmm_write_lock_irqsave(&cpu_state->cpu_lock, cpu_flags);

		cpu_state->current_pending = 1023;
		if (!s->enabled || !cpu_state->enabled) {
			vmm_write_unlock_irqrestore(&cpu_state->cpu_lock, cpu_flags);
			if (s->is_child_pic) {
				vmm_devemu_emulate_percpu_irq(s->guest, 
						       cpu_state->parent_irq,
						       cpu, 0);
			}
			break;
		}
		best_prio = 0x100;
		best_irq = 1023;

		vmm_read_lock_irqsave(&s->dist_lock, dist_flags);

		for (irq = 0; irq < GIC_NUM_IRQ(s); irq++) {
			if (GIC_TEST_ENABLED(s, irq, cm) && 
			    GIC_TEST_PENDING(s, irq, cm)) {
				if (GIC_GET_PRIORITY(s, irq, cpu) < best_prio) {
					best_prio = 
						GIC_GET_PRIORITY(s, irq, cpu);
					best_irq = irq;
				}
			}
		}

		vmm_read_unlock_irqrestore(&s->dist_lock, dist_flags);

		level = 0;
		if (best_prio < cpu_state->priority_mask) {
			cpu_state->current_pending = best_irq;
			if (best_prio < cpu_state->running_priority) {
				level = 1;
			}
		}

		vmm_write_unlock_irqrestore(&cpu_state->cpu_lock, cpu_flags);

		if (s->is_child_pic) {
			/* Assert irq to Parent PIC */
			vmm_devemu_emulate_percpu_irq(s->guest, 
						      cpu_state->parent_irq,
						      cpu, level);
		} else {
			vcpu = vmm_manager_guest_vcpu(s->guest, cpu);
			if (level && vcpu) {
				/* Assert irq to VCPU */
				vmm_vcpu_irq_assert(vcpu, 
						    cpu_state->parent_irq, 0x0);
			} 
			if (!level && vcpu) {
				/* Deassert irq to VCPU */
				vmm_vcpu_irq_deassert(vcpu, 
						      cpu_state->parent_irq);
			}
		}
	}
}

/* Process IRQ asserted via device emulation framework */
static void gic_irq_handle(u32 irq, int cpu, int level, void *opaque)
{
	irq_flags_t flags;
	int cm, target;
	struct gic_state *s = opaque;

	irq -= GIC_BASE_IRQ(s);

	if (irq < 32) {
		/* In case of PPIs and SGIs */
		cm = target = (1 << cpu);
	} else {
		/* In case of SGIs */
		cm = GIC_ALL_CPU_MASK(s);
		target = GIC_TARGET(s, irq);
	}	

	vmm_write_lock_irqsave(&s->dist_lock, flags);

	if (level == GIC_TEST_LEVEL(s, irq, cm)) {
		vmm_write_unlock_irqrestore(&s->dist_lock, flags);
		return;
	}

	if (level) {
		GIC_SET_LEVEL(s, irq, cm);
		if (GIC_TEST_TRIGGER(s, irq) || GIC_TEST_ENABLED(s, irq, cm)) {
			GIC_SET_PENDING(s, irq, target);
		}
	} else {
		GIC_CLEAR_LEVEL(s, irq, cm);
	}

	vmm_write_unlock_irqrestore(&s->dist_lock, flags);

	gic_update(s);
}

static void gic_set_running_irq(struct gic_state *s, int cpu, int irq)
{
	struct gic_cpu_state *cpu_state = &s->cpu_state[cpu];

	cpu_state->running_irq = irq;
	if (irq == 1023) {
		cpu_state->running_priority = 0x100;
	} else {
		cpu_state->running_priority = GIC_GET_PRIORITY(s, irq, cpu);
	}
}

static u32 gic_acknowledge_irq(struct gic_state *s, int cpu)
{
	int new_irq;
	int cm = 1 << cpu;
	irq_flags_t dist_flags, cpu_flags;
	struct gic_cpu_state *cpu_state = &s->cpu_state[cpu];

	vmm_write_lock_irqsave(&cpu_state->cpu_lock, cpu_flags);

	new_irq = cpu_state->current_pending;

	if ((new_irq == 1023) ||
	    GIC_GET_PRIORITY(s, new_irq, cpu) >= cpu_state->running_priority) {
		new_irq = 1023;
		goto release_lock;
	}

	cpu_state->last_active[new_irq] = cpu_state->running_irq;

	vmm_write_lock_irqsave(&s->dist_lock, dist_flags);

	/* Clear pending flags for both level and edge triggered interrupts.
	 * Level triggered IRQs will be reasserted once they become inactive. */
	GIC_CLEAR_PENDING(s, new_irq, 
			  GIC_TEST_MODEL(s, new_irq) ? GIC_ALL_CPU_MASK(s) : cm);

	vmm_write_unlock_irqrestore(&s->dist_lock, dist_flags);

	gic_set_running_irq(s, cpu, new_irq);

 release_lock:
	vmm_write_unlock_irqrestore(&cpu_state->cpu_lock, cpu_flags);

	if (new_irq != 1023) {
		gic_update(s);
	}

	return new_irq;
}

static void gic_complete_irq(struct gic_state *s, int cpu, int irq)
{
	int update = 0;
	int cm = 1 << cpu;
	irq_flags_t dist_flags, cpu_flags;
	struct gic_cpu_state *cpu_state = &s->cpu_state[cpu];

	vmm_write_lock_irqsave(&cpu_state->cpu_lock, cpu_flags);

	if (cpu_state->running_irq == 1023) {
		goto release_lock;
	}

	vmm_write_lock_irqsave(&s->dist_lock, dist_flags);

	/* Mark level triggered interrupts as pending if 
	 * they are still raised. */
	if (!GIC_TEST_TRIGGER(s, irq) && 
	    GIC_TEST_ENABLED(s, irq, cm) &&
	    GIC_TEST_LEVEL(s, irq, cm) && 
	    (GIC_TARGET(s, irq) & cm) != 0) {
		GIC_SET_PENDING(s, irq, cm);
		update = 1;
	}

	vmm_write_unlock_irqrestore(&s->dist_lock, dist_flags);

	if (irq != cpu_state->running_irq) {
		/* Complete an IRQ that is not currently running.  */
		int tmp = cpu_state->running_irq;
		while (cpu_state->last_active[tmp] != 1023) {
			if (cpu_state->last_active[tmp] == irq) {
				cpu_state->last_active[tmp] =
						cpu_state->last_active[irq];
				break;
			}
			tmp = cpu_state->last_active[tmp];
		}
		if (update) {
			update = 2;
		}
	} else {
		/* Complete the current running IRQ.  */
		gic_set_running_irq(s, cpu, 
				cpu_state->last_active[cpu_state->running_irq]);
		update = 2;
	}

 release_lock:
	vmm_write_unlock_irqrestore(&cpu_state->cpu_lock, cpu_flags);

	if (update == 2) {
		gic_update(s);
	}
}

static int __gic_dist_readb(struct gic_state *s, int cpu, u32 offset, u8 *dst)
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
			*dst = (GIC_NUM_CPU(s) - 1) << 5;
			*dst |= (GIC_NUM_IRQ(s) / 32) - 1;
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
			*dst |= GIC_TEST_ENABLED(s, irq + i, (1 << cpu)) ? 
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
		mask = (irq < 32) ? (1 << cpu) : GIC_ALL_CPU_MASK(s);
		*dst = 0;
		for (i = 0; i < 8; i++) {
			*dst |= GIC_TEST_PENDING(s, irq + i, mask) ? 
				(1 << i) : 0x0;
		}
		break;
	case 0x300: /* Active0 */
	case 0x304: /* Active1 */
	case 0x308: /* Active2 */
		irq = (offset & 0xF) * 8;
		mask = (irq < 32) ? (1 << cpu) : GIC_ALL_CPU_MASK(s);
		*dst = 0;
		for (i = 0; i < 8; i++) {
			*dst |= GIC_TEST_ACTIVE(s, irq + i, mask) ? 
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
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			*dst = GIC_GET_PRIORITY(s, irq, cpu) << 4;
			break;
		case 0x8: /* CPU targets */
			irq = offset - 0x800;
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (irq < 32) {
				*dst = 1 << cpu;
			} else {
				*dst = GIC_TARGET(s, irq);
			}
			break;
		case 0xC: /* Configuration */
			irq = (offset - 0xC00) * 4;
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			*dst = 0;
			for (i = 0; i < 4; i++) {
				if (GIC_TEST_MODEL(s, irq + i)) {
					*dst |= (1 << (i * 2));
				}
				if (GIC_TEST_TRIGGER(s, irq + i)) {
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

static int __gic_dist_writeb(struct gic_state *s, int cpu, u32 offset, u8 src)
{
	u32 done = 0, i, irq, mask, cm;

	if (!s) {
		return VMM_EFAIL;
	}

	done = 1;
	switch (offset - (offset & 0x3)) {
	case 0x000: /* Distributor control */
		if (offset == 0x000) {
			s->enabled = src & 0x1 ? TRUE : FALSE;
		}
		break;
	case 0x004: /* Controller type */
		/* Ignored. */
		break;
	case 0x100: /* Set-enable0 */
	case 0x104: /* Set-enable1 */
	case 0x108: /* Set-enable2 */
		irq = (offset & 0xF) *8;
		if (GIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		if (irq < 16) {
			src = 0xFF;
		}
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				mask = ((irq + i) < 32) ? 
					(1 << cpu) : GIC_TARGET(s, (irq + i));
				cm = ((irq + i) < 32) ? 
					(1 << cpu) : GIC_ALL_CPU_MASK(s);
				GIC_SET_ENABLED(s, irq + i, cm);
				/* If a raised level triggered IRQ enabled 
				 * then mark is as pending.  */
				if (GIC_TEST_LEVEL(s, (irq + i), mask) &&
				    !GIC_TEST_TRIGGER(s, (irq + i))) {
					GIC_SET_PENDING(s, (irq + i), mask);
				}
			}
		}
		break;
	case 0x180: /* Clear-enable0 */
	case 0x184: /* Clear-enable1 */
	case 0x188: /* Clear-enable2 */
		irq = (offset & 0xF) *8;
		if (GIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		if (irq < 16) {
			src = 0x00;
		}
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				int cm = ((irq + i) < 32) ? 
					(1 << cpu) : GIC_ALL_CPU_MASK(s);
				GIC_CLEAR_ENABLED(s, irq + i, cm);
			}
		}
		break;
	case 0x200: /* Set-pending0 */
	case 0x204: /* Set-pending1 */
	case 0x208: /* Set-pending2 */
		irq = (offset & 0xF) *8;
		if (GIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		if (irq < 16) {
			src = 0x00;
		}
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				mask = GIC_TARGET(s, irq + i);
				GIC_SET_PENDING(s, irq + i, mask);
			}
		}
		break;
	case 0x280: /* Clear-pending0 */
	case 0x284: /* Clear-pending1 */
	case 0x288: /* Clear-pending2 */
		irq = (offset & 0xF) *8;
		if (GIC_NUM_IRQ(s) <= irq) {
			done = 0;
			break;
		}
		/* ??? This currently clears the pending bit for all CPUs, even
 		 * for per-CPU interrupts.  It's unclear whether this is the
		 * corect behavior.  */
		mask = GIC_ALL_CPU_MASK(s);
		for (i = 0; i < 8; i++) {
			if (src & (1 << i)) {
				GIC_CLEAR_PENDING(s, irq + i, mask);
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
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (irq < 32) {
				s->cpu_state[cpu].priority[irq] = src >> 4;
			} else {
				s->irq_state[irq].priority = src >> 4;
			}
			break;
		case 0x8: /* CPU targets */
			irq = offset - 0x800;
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (irq < 16) {
				src = 0x0;
			} else if (irq < 32) {
				src = GIC_ALL_CPU_MASK(s);
			}
			s->irq_state[irq].target = src & GIC_ALL_CPU_MASK(s);
			break;
		case 0xC: /* Configuration */
			irq = (offset - 0xC00) * 4;
			if (GIC_NUM_IRQ(s) <= irq) {
				done = 0;
				break;
			}
			if (irq < 32) {
				src |= 0xAA;
			}
			for (i = 0; i < 4; i++) {
				if (src & (1 << (i * 2))) {
					GIC_SET_MODEL(s, irq + i);
				} else {
					GIC_CLEAR_MODEL(s, irq + i);
				}
				if (src & (2 << (i * 2))) {
					GIC_SET_TRIGGER(s, irq + i);
				} else {
					GIC_CLEAR_TRIGGER(s, irq + i);
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

static int gic_dist_read(struct gic_state *s, int cpu, u32 offset, u32 *dst)
{
	int rc = VMM_OK, i;
	irq_flags_t flags;
	u8 val;

	if (!s || !dst) {
		return VMM_EFAIL;
	}

	vmm_read_lock_irqsave(&s->dist_lock, flags);

	*dst = 0;
	for (i = 0; i < 4; i++) {
		if ((rc = __gic_dist_readb(s, cpu, offset + i, &val))) {
				break;
		}
		*dst |= val << (i * 8);
	}

	vmm_read_unlock_irqrestore(&s->dist_lock, flags);

	return VMM_OK;
}

static int gic_dist_write(struct gic_state *s, int cpu, u32 offset, 
			  u32 src_mask, u32 src)
{
	int rc = VMM_OK, irq, mask, i;
	irq_flags_t flags;

	if (!s) {
		return VMM_EFAIL;
	}

	vmm_write_lock_irqsave(&s->dist_lock, flags);

	if (offset == 0xF00) {
		/* Software Interrupt */
		irq = src & 0x3ff;
		switch ((src >> 24) & 3) {
		case 0:
			mask = (src >> 16) & GIC_ALL_CPU_MASK(s);
			break;
		case 1:
			mask = GIC_ALL_CPU_MASK(s) ^ (1 << cpu);
			break;
		case 2:
			mask = 1 << cpu;
			break;
		default:
			mask = GIC_ALL_CPU_MASK(s);
			break;
		};
		GIC_SET_PENDING(s, irq, mask);
	} else {
		src_mask = ~src_mask;
		for (i = 0; i < 4; i++) {
			if (src_mask & 0xFF) {
				if ((rc = __gic_dist_writeb(s, cpu, 
						offset + i, src & 0xFF))) {
					break;
				}
			}
			src_mask = src_mask >> 8;
			src = src >> 8;
		}
	}

	vmm_write_unlock_irqrestore(&s->dist_lock, flags);

	gic_update(s);

	return rc;
}

static int gic_cpu_read(struct gic_state *s, u32 cpu, u32 offset, u32 *dst)
{
	int rc = VMM_OK;

	if (!s || !dst) {
		return VMM_EFAIL;
	}
	if (s->num_cpu <= cpu) {
		return VMM_EFAIL;
	}

	switch (offset) {
	case 0x00: /* Control */
		*dst = s->cpu_state[cpu].enabled;
		break;
	case 0x04: /* Priority mask */
		*dst = s->cpu_state[cpu].priority_mask;
		break;
	case 0x08: /* Binary Point */
		/* ??? Not implemented.  */
		break;
	case 0x0c: /* Acknowledge */
		*dst = gic_acknowledge_irq(s, cpu);
		break;
	case 0x14: /* Runing Priority */
		*dst = s->cpu_state[cpu].running_priority;
		break;
	case 0x18: /* Highest Pending Interrupt */
		*dst = s->cpu_state[cpu].current_pending;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	}

	return rc;
}

static int gic_cpu_write(struct gic_state *s, u32 cpu, u32 offset, 
		  u32 src_mask, u32 src)
{
	int rc = VMM_OK;

	if (!s) {
		return VMM_EFAIL;
	}
	if (s->num_cpu <= cpu) {
		return VMM_EFAIL;
	}

	src = src & ~src_mask;

	switch (offset) {
	case 0x00: /* Control */
		s->cpu_state[cpu].enabled = src & 0x1 ? TRUE : FALSE;
		gic_update(s);
		break;
	case 0x04: /* Priority mask */
		s->cpu_state[cpu].priority_mask = src & 0xFF;
		gic_update(s);
		break;
	case 0x08: /* Binary Point */
		/* ??? Not implemented.  */
		break;
	case 0x10: /* End Of Interrupt */
		gic_complete_irq(s, cpu, src & 0x3ff);
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	return rc;
}

int gic_reg_read(struct gic_state *s, physical_addr_t offset, u32 *dst)
{
	struct vmm_vcpu *vcpu;

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->id != vcpu->guest->id) {
		return VMM_EFAIL;
	}

	if ((offset >= s->cpu.offset) &&
	    (offset < (s->cpu.offset + s->cpu.length))) {
		/* Read CPU Interface */
		return gic_cpu_read(s, vcpu->subid, offset & 0xFC, dst);
	} else if ((offset >= s->dist.offset) &&
		   (offset < (s->dist.offset + s->dist.length)))  {
		/* Read Distribution Control */
		return gic_dist_read(s, vcpu->subid, offset & 0xFFC, dst);
	}

	return VMM_EFAIL;
}
VMM_EXPORT_SYMBOL(gic_reg_read);

int gic_reg_write(struct gic_state *s, physical_addr_t offset,
		  u32 src_mask, u32 src)
{
	struct vmm_vcpu *vcpu;

	vcpu = vmm_scheduler_current_vcpu();
	if (!vcpu || !vcpu->guest) {
		return VMM_EFAIL;
	}
	if (s->guest->id != vcpu->guest->id) {
		return VMM_EFAIL;
	}

	if ((offset >= s->cpu.offset) &&
	    (offset < (s->cpu.offset + s->cpu.length))) {
		/* Write CPU Interface */
		return gic_cpu_write(s, vcpu->subid, 
				   offset & 0xFC, src_mask, src);
	} else if ((offset >= s->dist.offset) &&
		   (offset < (s->dist.offset + s->dist.length)))  {
		/* Write Distribution Control */
		return gic_dist_write(s, vcpu->subid, 
				    offset & 0xFFC, src_mask, src);
	}

	return VMM_EFAIL;
}
VMM_EXPORT_SYMBOL(gic_reg_write);

static int gic_emulator_read8(struct vmm_emudev *edev,
			      physical_addr_t offset, 
			      u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = gic_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int gic_emulator_read16(struct vmm_emudev *edev,
			       physical_addr_t offset, 
			       u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = gic_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int gic_emulator_read32(struct vmm_emudev *edev,
			       physical_addr_t offset, 
			       u32 *dst)
{
	return gic_reg_read(edev->priv, offset, dst);
}

static int gic_emulator_write8(struct vmm_emudev *edev,
			       physical_addr_t offset, 
			       u8 src)
{
	return gic_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int gic_emulator_write16(struct vmm_emudev *edev,
				physical_addr_t offset, 
				u16 src)
{
	return gic_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int gic_emulator_write32(struct vmm_emudev *edev,
				physical_addr_t offset, 
				u32 src)
{
	return gic_reg_write(edev->priv, offset, 0x00000000, src);
}

int gic_state_reset(struct gic_state *s)
{
	u32 i;
	irq_flags_t flags;

	vmm_write_lock_irqsave(&s->dist_lock, flags);

	/*
	 * We should not reset level as for host to guest IRQ might
	 * have been raised already.
	 */
	for (i = 0; i < GIC_NUM_IRQ(s); i++) {
		GIC_CLEAR_ENABLED(s, i, GIC_ALL_CPU_MASK(s));
		GIC_CLEAR_PENDING(s, i, GIC_ALL_CPU_MASK(s));
		GIC_CLEAR_ACTIVE(s, i, GIC_ALL_CPU_MASK(s));
		GIC_CLEAR_MODEL(s, i);
		GIC_CLEAR_TRIGGER(s, i);
	}

	for (i = 0; i < GIC_NUM_CPU(s); i++) {
		struct gic_cpu_state *cpu_state = &s->cpu_state[i];

		if (s->type == GIC_TYPE_ARM11MPCORE) {
			cpu_state->priority_mask = 0xf0;
		} else {
			cpu_state->priority_mask = 0x0;
		}
		cpu_state->current_pending = 1023;
		cpu_state->running_irq = 1023;
		cpu_state->running_priority = 0x100;
		cpu_state->enabled = FALSE;
	}
	/* */
	for (i = 0; i < 16; i++) {
		GIC_SET_ENABLED(s, i, GIC_ALL_CPU_MASK(s));
		GIC_SET_TRIGGER(s, i);
	}
	s->enabled = FALSE;

	vmm_write_unlock_irqrestore(&s->dist_lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(gic_state_reset);

static int gic_emulator_reset(struct vmm_emudev *edev)
{
	struct gic_state *s = edev->priv;
	
	return gic_state_reset(s);
}

static u32 gic_configs[][14] = {
	{ 
		/* num_irq */ 96,
		/* base_irq */ 0,
		/* id0 */ 0x90, 
		/* id1 */ 0x13, 
		/* id2 */ 0x04, 
		/* id3 */ 0x00, 
		/* id4 */ 0x0d, 
		/* id5 */ 0xf0, 
		/* id6 */ 0x05, 
		/* id7 */ 0xb1,
		/* cpu_offset */ 0x100,
		/* cpu_length */ 0x100,
		/* dist_offset */ 0x1000,
		/* dist_length */ 0x1000,
	},
	{ 
		/* num_irq */ 96,
		/* base_irq */ 0,
		/* id0 */ 0x90, 
		/* id1 */ 0x13, 
		/* id2 */ 0x04, 
		/* id3 */ 0x00, 
		/* id4 */ 0x0d, 
		/* id5 */ 0xf0, 
		/* id6 */ 0x05, 
		/* id7 */ 0xb1,
		/* cpu_offset */ 0x0,
		/* cpu_length */ 0x100,
		/* dist_offset */ 0x1000,
		/* dist_length */ 0x1000,
	},
	{ 
		/* num_irq */ 96,
		/* base_irq */ 0,
		/* id0 */ 0x90, 
		/* id1 */ 0x13, 
		/* id2 */ 0x04, 
		/* id3 */ 0x00, 
		/* id4 */ 0x0d, 
		/* id5 */ 0xf0, 
		/* id6 */ 0x05, 
		/* id7 */ 0xb1,
		/* cpu_offset */ 0x100,
		/* cpu_length */ 0x100,
		/* dist_offset */ 0x1000,
		/* dist_length */ 0x1000,
	},
	{ 
		/* num_irq */ 128,
		/* base_irq */ 0,
		/* id0 */ 0x90, 
		/* id1 */ 0x13, 
		/* id2 */ 0x04, 
		/* id3 */ 0x00, 
		/* id4 */ 0x0d, 
		/* id5 */ 0xf0, 
		/* id6 */ 0x05, 
		/* id7 */ 0xb1,
		/* cpu_offset */ 0x2000,
		/* cpu_length */ 0x1000,
		/* dist_offset */ 0x1000,
		/* dist_length */ 0x1000,
	},
};

struct gic_state *gic_state_alloc(const char *name,
				  struct vmm_guest *guest, 
				  enum gic_type type,
				  u32 num_cpu, 
				  bool is_child_pic,
				  u32 base_irq,
				  u32 num_irq,
				  u32 parent_irq)
{
	u32 i;
	struct gic_state *s = NULL;

	if (guest->vcpu_count > GIC_MAX_NCPU) {
		return NULL;
	}
	if (num_irq > GIC_MAX_NIRQ) {
		return NULL;
	}

	s = vmm_zalloc(sizeof(struct gic_state));
	if (!s) {
		return NULL;
	}

	s->num_cpu = num_cpu;
	s->num_irq = num_irq;
	s->base_irq = base_irq;
	s->type = type;
	s->id[0] = gic_configs[type][2];
	s->id[1] = gic_configs[type][3];
	s->id[2] = gic_configs[type][4];
	s->id[3] = gic_configs[type][5];
	s->id[4] = gic_configs[type][6];
	s->id[5] = gic_configs[type][7];
	s->id[6] = gic_configs[type][8];
	s->id[7] = gic_configs[type][9];
	s->cpu.offset = gic_configs[type][10];
	s->cpu.length = gic_configs[type][11];
	s->dist.offset = gic_configs[type][12];
	s->dist.length = gic_configs[type][13];

	s->is_child_pic = is_child_pic;

	for (i = 0; i < s->num_cpu; i++) {
		INIT_RW_LOCK(&s->cpu_state[i].cpu_lock);
		s->cpu_state[i].parent_irq = parent_irq;
	}

	s->guest = guest;
	INIT_RW_LOCK(&s->dist_lock);

	for (i = s->base_irq; i < (s->base_irq + s->num_irq); i++) {
		vmm_devemu_register_irq_handler(guest, i, 
						name, gic_irq_handle, s);
	}

	return s;
}
VMM_EXPORT_SYMBOL(gic_state_alloc);

int gic_state_free(struct gic_state *s)
{
	u32 i;

	if (!s) {
		return VMM_EFAIL;
	}

	for (i = s->base_irq; i < (s->base_irq + s->num_irq); i++) {
		vmm_devemu_unregister_irq_handler(s->guest, i,
						  gic_irq_handle, s);
	}
	vmm_free(s);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(gic_state_free);

static int gic_emulator_probe(struct vmm_guest *guest,
			      struct vmm_emudev *edev,
			      const struct vmm_devtree_nodeid *eid)
{
	const char *attr;
	struct gic_state *s;
	bool is_child_pic;
	enum gic_type type;
	u32 parent_irq, base_irq, num_irq;

	if (guest->vcpu_count > GIC_MAX_NCPU) {
		return VMM_ENODEV;
	}

	attr = vmm_devtree_attrval(edev->node, "child_pic");
	if (attr) {
		is_child_pic = TRUE;
	} else {
		is_child_pic = FALSE;
	}

	attr = vmm_devtree_attrval(edev->node, "parent_irq");
	if (!attr) {
		return VMM_EFAIL;
	}
	parent_irq = *((u32 *)attr);
	
	type = (enum gic_type)(eid->data);

	attr = vmm_devtree_attrval(edev->node, "base_irq");
	if (!attr) {
		base_irq = gic_configs[type][1];
	} else {
		base_irq = *((u32 *)attr);
	}
	
	attr = vmm_devtree_attrval(edev->node, "num_irq");
	if (!attr) {
		num_irq = gic_configs[type][0];
	} else {
		num_irq = *((u32 *)attr);
		if (num_irq > GIC_MAX_NIRQ) {
			num_irq = GIC_MAX_NIRQ;
		}
	}
	
	s = gic_state_alloc(edev->node->name, guest, type, 
			    guest->vcpu_count, is_child_pic,
			    base_irq, num_irq, parent_irq);

	edev->priv = s;

	return VMM_OK;
}

static int gic_emulator_remove(struct vmm_emudev *edev)
{
	struct gic_state *s = edev->priv;

	gic_state_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid gic_emuid_table[] = {
	{ .type = "pic", 
	  .compatible = "arm11mpcore,gic", 
	  .data = (void *)GIC_TYPE_ARM11MPCORE,
	},
	{ .type = "pic", 
	  .compatible = "realview,gic", 
	  .data = (void *)GIC_TYPE_REALVIEW,
	},
	{ .type = "pic", 
	  .compatible = "vexpress,gic", 
	  .data = (void *)GIC_TYPE_VEXPRESS,
	},
	{ .type = "pic", 
	  .compatible = "vexpress,gicv2", 
	  .data = (void *)GIC_TYPE_VEXPRESS_V2,
	},
	{ /* end of list */ },
};

static struct vmm_emulator gic_emulator = {
	.name = "gic",
	.match_table = gic_emuid_table,
	.endian = VMM_EMULATOR_LITTLE_ENDIAN,
	.probe = gic_emulator_probe,
	.read8 = gic_emulator_read8,
	.write8 = gic_emulator_write8,
	.read16 = gic_emulator_read16,
	.write16 = gic_emulator_write16,
	.read32 = gic_emulator_read32,
	.write32 = gic_emulator_write32,
	.reset = gic_emulator_reset,
	.remove = gic_emulator_remove,
};

static int __init gic_emulator_init(void)
{
	return vmm_devemu_register_emulator(&gic_emulator);
}

static void __exit gic_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&gic_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
