/*
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file lapic.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Local APIC Emulator
 *
 * The source has been largely adapted from QEMU hw/intc/apic.c
 *
 *  Copyright (c) 2004-2005 Fabrice Bellard
 */

#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_timer.h>
#include <vmm_devemu.h>
#include <vmm_spinlocks.h>
#include <vmm_vcpu_irq.h>
#include <vmm_scheduler.h>

#include <arch_cpu_irq.h>
#include <arch_barrier.h>
#include <cpu_msr.h>
#include <emu/lapic.h>
#include <emu/apic_common.h>

#define MODULE_DESC			"Local APIC Emulator"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			apic_emulator_init
#define	MODULE_EXIT			apic_emulator_exit

/* debug PIC */
enum apic_debug_log_levels {
	APIC_LOG_LVL_ERR,
	APIC_LOG_LVL_INFO,
	APIC_LOG_LVL_DEBUG,
	APIC_LOG_LVL_VERBOSE
};

static unsigned int apic_default_log_lvl = APIC_LOG_LVL_VERBOSE;

#define APIC_LOG(lvl, fmt, args...)				\
	do {							\
		if (APIC_LOG_LVL_##lvl <= apic_default_log_lvl)	\
			vmm_printf("LAPIC: " fmt , ##args);	\
	} while (0);


#define MAX_APIC_WORDS 			8

#define SYNC_FROM_VAPIC                 0x1
#define SYNC_TO_VAPIC                   0x2
#define SYNC_ISR_IRR_TO_VAPIC           0x4

static void apic_set_irq(apic_state_t *s, int vector_num, int trigger_mode);
static void apic_update_irq(apic_state_t *s);
static void apic_get_delivery_bitmask(apic_state_t *s, u32 *deliver_bitmask,
                                      u8 dest, u8 dest_mode);

static apic_state_t *cpu_get_current_apic(apic_state_t *apic_base)
{
	struct vmm_vcpu *vcpu = vmm_scheduler_current_vcpu();
	apic_state_t *apic = NULL;
	int i;

	for (i = 0; i < MAX_APICS+1; i++) {
		apic = apic_base + i;
		if (apic->vcpu == vcpu) {
			return apic;
		}
	}

	return NULL;
}

/* Find first bit starting from msb */
static int apic_fls_bit(u32 value)
{
	return fls(value);
}

/* Find first bit starting from lsb */
static int apic_ffs_bit(u32 value)
{
	return ffs(value);
}

static inline void apic_set_bit(u32 *tab, int index)
{
	int i, mask;
	i = index >> 5;
	mask = 1 << (index & 0x1f);
	tab[i] |= mask;
}

static inline void apic_reset_bit(u32 *tab, int index)
{
	int i, mask;
	i = index >> 5;
	mask = 1 << (index & 0x1f);
	tab[i] &= ~mask;
}

static inline int apic_get_bit(u32 *tab, int index)
{
	int i, mask;
	i = index >> 5;
	mask = 1 << (index & 0x1f);
	return !!(tab[i] & mask);
}

/* return -1 if no bit is set */
static int get_highest_priority_int(u32 *tab)
{
	int i;
	for (i = 7; i >= 0; i--) {
		if (tab[i] != 0) {
			return i * 32 + apic_fls_bit(tab[i]);
		}
	}
	return -1;
}

static void apic_local_deliver(apic_state_t *s, int vector)
{
	u32 lvt = s->lvt[vector];
	int trigger_mode;

	if (lvt & APIC_LVT_MASKED)
		return;

	switch ((lvt >> 8) & 7) {
	case APIC_DM_SMI:
		APIC_LOG(ERR, "SMI Interrupts not supported!\n");
		break;

	case APIC_DM_NMI:
		APIC_LOG(ERR, "NMI Interrupts not supported!\n");
		break;

	case APIC_DM_EXTINT:
		APIC_LOG(ERR, "EXTInterrupts not delivered via apic_local_deliver\n");
		break;

	case APIC_DM_FIXED:
		trigger_mode = APIC_TRIGGER_EDGE;
		if ((vector == APIC_LVT_LINT0 || vector == APIC_LVT_LINT1) &&
		    (lvt & APIC_LVT_LEVEL_TRIGGER))
			trigger_mode = APIC_TRIGGER_LEVEL;
		apic_set_irq(s, lvt & 0xff, trigger_mode);
	}
}

#if 0
static void apic_external_nmi(apic_state_t *s)
{
	apic_local_deliver(s, APIC_LVT_LINT1);
}
#endif

#define foreach_apic(apic_base, apic, deliver_bitmask, code)		\
	{								\
		int __i, __j;						\
		int nr_apics = apic_base->vcpu->guest->vcpu_count;	\
		for(__i = 0; __i < MAX_APIC_WORDS; __i++) {		\
			u32 __mask = deliver_bitmask[__i];		\
			if (__mask) {					\
				for(__j = 0; __j < 32; __j++) {		\
					if (__mask & (1U << __j)) {	\
						if ((__i * 32 + __j)	\
						    >= nr_apics)	\
							continue;	\
						apic = apic_base +	\
							(__i * 32 + __j); \
							if (apic) {	\
								code;	\
							}		\
					}				\
				}					\
			}						\
		}							\
	}

static void apic_bus_deliver(apic_state_t *apic_base, const u32 *deliver_bitmask,
                             u8 delivery_mode, u8 vector_num,
                             u8 trigger_mode)
{
	apic_state_t *apic_iter;

	switch (delivery_mode) {
        case APIC_DM_LOWPRI:
		/* XXX: search for focus processor, arbitration */
		{
			int i, d;
			d = -1;
			for(i = 0; i < MAX_APIC_WORDS; i++) {
				if (deliver_bitmask[i]) {
					d = i * 32 + apic_ffs_bit(deliver_bitmask[i]);
					break;
				}
			}
			if (d >= 0) {
				apic_iter = apic_base + d;
				if (apic_iter) {
					apic_set_irq(apic_iter, vector_num, trigger_mode);
				}
			}
		}
		return;

        case APIC_DM_FIXED:
		break;

        case APIC_DM_SMI:
		APIC_LOG(ERR, "SMI Interrupts not supported.\n");
#if 0
		foreach_apic(apic_iter, deliver_bitmask,
			     cpu_interrupt(CPU(apic_iter->cpu), CPU_INTERRUPT_SMI)
			     );
#endif
		return;

        case APIC_DM_NMI:
		APIC_LOG(ERR, "NMI Interrupts not supported.\n");
#if 0
		foreach_apic(apic_iter, deliver_bitmask,
			     cpu_interrupt(CPU(apic_iter->cpu), CPU_INTERRUPT_NMI)
			     );
#endif
            return;

        case APIC_DM_INIT:
		APIC_LOG(ERR, "INIT Interrupts not supported.\n");
#if 0
		/* normal INIT IPI sent to processors */
		foreach_apic(apic_iter, deliver_bitmask,
			     cpu_interrupt(CPU(apic_iter->cpu),
					   CPU_INTERRUPT_INIT)
			     );
#endif
		return;

        case APIC_DM_EXTINT:
		/* handled in I/O APIC code */
		break;

        default:
		return;
	}

	foreach_apic(apic_base, apic_iter, deliver_bitmask,
		     apic_set_irq(apic_iter, vector_num, trigger_mode) );
}

/* Called when slaves (IOAPIC/PIC) assert interrupt. */
void apic_deliver_irq(apic_state_t *s, u8 dest, u8 dest_mode, u8 delivery_mode,
                      u8 vector_num, u8 trigger_mode)
{
	u32 deliver_bitmask[MAX_APIC_WORDS];

	apic_get_delivery_bitmask(s, deliver_bitmask, dest, dest_mode);
	apic_bus_deliver(s, deliver_bitmask, delivery_mode, vector_num, trigger_mode);
}

/* Process IRQ asserted via device emulation framework */
void apic_irq_handle(u32 irq, int cpu, int level, void *opaque)
{
	irq_flags_t flags;
	apic_state_t *s = opaque;
	u8 dest, dest_mode, del_mode, tmode, vnum;

	vmm_spin_lock_irqsave(&s->state_lock, flags);

	SLAVE_IRQ_DECODE(level, dest, dest_mode, del_mode, vnum, tmode);

	apic_deliver_irq(s, dest, dest_mode, del_mode, vnum, tmode);

	vmm_spin_unlock_irqrestore(&s->state_lock, flags);
}

#if 0
static void apic_set_base(apic_state_t *s, u64 val)
{
	s->apicbase = (val & 0xfffff000) |
		(s->apicbase & (MSR_IA32_APICBASE_BSP | MSR_IA32_APICBASE_ENABLE));
	/* if disabled, cannot be enabled again */
	if (!(val & MSR_IA32_APICBASE_ENABLE)) {
		s->apicbase &= ~MSR_IA32_APICBASE_ENABLE;
		cpu_clear_apic_feature(&s->cpu->env);
		s->spurious_vec &= ~APIC_SV_ENABLE;
	}
}

static void apic_set_tpr(apic_state_t *s, u8 val)
{
	/* Updates from cr8 are ignored while the VAPIC is active */
	if (!s->vapic_paddr) {
		s->tpr = val << 4;
		apic_update_irq(s);
	}
}

static u8 apic_get_tpr(apic_state_t *s)
{
#if 0
	apic_sync_vapic(s, SYNC_FROM_VAPIC);
#endif
	return s->tpr >> 4;
}
#endif

static int apic_get_ppr(apic_state_t *s)
{
	int tpr, isrv, ppr;

	tpr = (s->tpr >> 4);
	isrv = get_highest_priority_int(s->isr);
	if (isrv < 0)
		isrv = 0;
	isrv >>= 4;
	if (tpr >= isrv)
		ppr = s->tpr;
	else
		ppr = isrv << 4;
	return ppr;
}

static int apic_get_arb_pri(apic_state_t *s)
{
	/* XXX: arbitration */
	return 0;
}

/*
 * <0 - low prio interrupt,
 * 0  - no interrupt,
 * >0 - interrupt number
 */
static int apic_irq_pending(apic_state_t *s)
{
	int irrv, ppr;
	irrv = get_highest_priority_int(s->irr);
	if (irrv < 0) {
		return 0;
	}
	ppr = apic_get_ppr(s);
	if (ppr && (irrv & 0xf0) <= (ppr & 0xf0)) {
		return -1;
	}

	return irrv;
}

int apic_get_interrupt(apic_state_t *s)
{
	int intno;

	/* if the APIC is installed or enabled, we let the 8259 handle the
	   IRQs */
	if (!s)
		return -1;
	if (!(s->spurious_vec & APIC_SV_ENABLE))
		return -1;

	intno = apic_irq_pending(s);

	if (intno == 0) {
		return -1;
	} else if (intno < 0) {
		return s->spurious_vec & 0xff;
	}
	apic_reset_bit(s->irr, intno);
	apic_set_bit(s->isr, intno);

	return intno;
}

/* signal the CPU if an irq is pending */
static void apic_update_irq(apic_state_t *s)
{
	int intno;

	if (!(s->spurious_vec & APIC_SV_ENABLE)) {
		return;
	}

	if (apic_irq_pending(s) > 0) {
		intno = apic_get_interrupt(s);
		vmm_vcpu_irq_assert(s->vcpu, intno, 0);
	}
}

static void apic_set_irq(apic_state_t *s, int vector_num, int trigger_mode)
{
	apic_set_bit(s->irr, vector_num);

	if (trigger_mode) {
		apic_set_bit(s->tmr, vector_num);
	} else {
		apic_reset_bit(s->tmr, vector_num);
	}

	if (s->vapic_paddr) {
		/*
		 * The vcpu thread needs to see the new IRR before we pull its current
		 * TPR value. That way, if we miss a lowering of the TRP, the guest
		 * has the chance to notice the new IRR and poll for IRQs on its own.
		 */
		arch_smp_mb();
	}

	apic_update_irq(s);
}

static void apic_eoi(apic_state_t *s)
{
	int isrv;
	isrv = get_highest_priority_int(s->isr);
	if (isrv < 0)
		return;
	apic_reset_bit(s->isr, isrv);
#if 0
	if (!(s->spurious_vec & APIC_SV_DIRECTED_IO) && apic_get_bit(s->tmr, isrv)) {
		ioapic_eoi_broadcast(isrv);
	}
#endif
	apic_update_irq(s);
}

static int apic_find_dest(apic_state_t *base, u8 dest)
{
	apic_state_t *apic = base + dest;

	if (apic && apic->id == dest)
		return dest;  /* shortcut in case apic->id == apic->idx */

	return -1;
}

static void apic_get_delivery_bitmask(apic_state_t *apic_base, u32 *deliver_bitmask,
                                      u8 dest, u8 dest_mode)
{
	if (dest_mode == 0) {
		if (dest == 0xff) {
			memset(deliver_bitmask, 0xff, MAX_APIC_WORDS * sizeof(u32));
		} else {
			int idx = apic_find_dest(apic_base, dest);
			memset(deliver_bitmask, 0x00, MAX_APIC_WORDS * sizeof(u32));
			if (idx >= 0)
				apic_set_bit(deliver_bitmask, idx);
		}
	} else
		APIC_LOG(ERR, "Logical Interrupt delivery not supported!\n");
}

#if 0
static void apic_startup(apic_state_t *s, int vector_num)
{
	s->sipi_vector = vector_num;
	cpu_interrupt(CPU(s->cpu), CPU_INTERRUPT_SIPI);
}

void apic_sipi(apic_state_t *s)
{
	cpu_reset_interrupt(CPU(s->cpu), CPU_INTERRUPT_SIPI);

	if (!s->wait_for_sipi)
		return;
	cpu_x86_load_seg_cache_sipi(s->cpu, s->sipi_vector);
	s->wait_for_sipi = 0;
}
#endif

static void apic_deliver(apic_state_t *s, u8 dest, u8 dest_mode,
                         u8 delivery_mode, u8 vector_num,
                         u8 trigger_mode)
{
	u32 deliver_bitmask[MAX_APIC_WORDS];
	int dest_shorthand = (s->icr[0] >> 18) & 3;
	apic_state_t *apic_iter;

	switch (dest_shorthand) {
	case 0:
		apic_get_delivery_bitmask(s, deliver_bitmask, dest, dest_mode);
		break;
	case 1:
		memset(deliver_bitmask, 0x00, sizeof(deliver_bitmask));
		apic_set_bit(deliver_bitmask, s->idx);
		break;
	case 2:
		memset(deliver_bitmask, 0xff, sizeof(deliver_bitmask));
		break;
	case 3:
		memset(deliver_bitmask, 0xff, sizeof(deliver_bitmask));
		apic_reset_bit(deliver_bitmask, s->idx);
		break;
	}

	switch (delivery_mode) {
        case APIC_DM_INIT:
		{
			int trig_mode = (s->icr[0] >> 15) & 1;
			int level = (s->icr[0] >> 14) & 1;
			if (level == 0 && trig_mode == 1) {
				foreach_apic(s, apic_iter, deliver_bitmask,
					     apic_iter->arb_id = apic_iter->id );
				return;
			}
		}
            break;

#if 0
        case APIC_DM_SIPI:
		foreach_apic(apic_iter, deliver_bitmask,
			     apic_startup(apic_iter, vector_num) );
		return;
#endif
	}

	apic_bus_deliver(s, deliver_bitmask, delivery_mode, vector_num, trigger_mode);
}

static u32 apic_get_current_count(apic_state_t *s)
{
	s64 d;
	u32 val;
	d = (vmm_timer_timestamp() - s->initial_count_load_time) >>
		s->count_shift;
	if (s->lvt[APIC_LVT_TIMER] & APIC_LVT_TIMER_PERIODIC) {
		/* periodic */
		val = s->initial_count - (d % ((u64)s->initial_count + 1));
	} else {
		if (d >= s->initial_count)
			val = 0;
		else
			val = s->initial_count - d;
	}
	return val;
}

bool apic_next_timer(apic_state_t *s, s64 current_time)
{
	s64 d;

	/* We need to store the timer state separately to support APIC
	 * implementations that maintain a non-QEMU timer, e.g. inside the
	 * host kernel. This open-coded state allows us to migrate between
	 * both models. */
	s->timer_expiry = -1;

	if (s->lvt[APIC_LVT_TIMER] & APIC_LVT_MASKED) {
		return false;
	}

	d = (current_time - s->initial_count_load_time) >> s->count_shift;

	if (s->lvt[APIC_LVT_TIMER] & APIC_LVT_TIMER_PERIODIC) {
		if (!s->initial_count) {
			return false;
		}
		d = ((d / ((u64)s->initial_count + 1)) + 1) *
			((u64)s->initial_count + 1);
	} else {
		if (d >= s->initial_count) {
			return false;
		}
		d = (u64)s->initial_count + 1;
	}
	s->next_time = s->initial_count_load_time + (d << s->count_shift);
	s->timer_expiry = s->next_time;

	return true;
}

static void apic_timer_update(apic_state_t *s, s64 current_time)
{
	if (apic_next_timer(s, current_time)) {
		vmm_timer_event_stop(&s->timer);
		vmm_timer_event_start(&s->timer, s->next_time);
	} else {
		vmm_timer_event_stop(&s->timer);
	}
}

static void apic_timer(struct vmm_timer_event *event)
{
	apic_state_t *s = (apic_state_t *)event->priv;

	apic_local_deliver(s, APIC_LVT_TIMER);
	apic_timer_update(s, s->next_time);
}

static u32 apic_ioport_read(apic_state_t *base, physical_addr_t addr, u32 *dst)
{
	apic_state_t *s;
	u32 val;
	int index;

	s = cpu_get_current_apic(base);
	if (!s) {
		APIC_LOG(ERR, "No LAPIC associated with current VCPU!\n");
		return 0;
	}

	index = (addr >> 4) & 0xff;
	switch(index) {
	case 0x02: /* id */
		val = s->id << 24;
		break;
	case 0x03: /* version */
		val = 0x11 | ((APIC_LVT_NB - 1) << 16); /* version 0x11 */
		break;
	case 0x08:
		val = s->tpr;
		break;
	case 0x09:
		val = apic_get_arb_pri(s);
		break;
	case 0x0a:
		/* ppr */
		val = apic_get_ppr(s);
		break;
	case 0x0b:
		val = 0;
		break;
	case 0x0d:
		val = s->log_dest << 24;
		break;
	case 0x0e:
		val = s->dest_mode << 28;
		break;
	case 0x0f:
		val = s->spurious_vec;
		break;
	case 0x10 ... 0x17:
		val = s->isr[index & 7];
		break;
	case 0x18 ... 0x1f:
		val = s->tmr[index & 7];
		break;
	case 0x20 ... 0x27:
		val = s->irr[index & 7];
		break;
	case 0x28:
		val = s->esr;
		break;
	case 0x30:
	case 0x31:
		val = s->icr[index & 1];
		break;
	case 0x32 ... 0x37:
		val = s->lvt[index - 0x32];
		break;
	case 0x38:
		val = s->initial_count;
		break;
	case 0x39:
		val = apic_get_current_count(s);
		break;
	case 0x3e:
		val = s->divide_conf;
		break;
	default:
		s->esr |= ESR_ILLEGAL_ADDRESS;
		val = 0;
		break;
	}

	*dst = val;

	return VMM_OK;
}

static int apic_ioport_write(apic_state_t *s, u32 addr, u32 src_mask, u32 val)
{
	apic_state_t *apic = cpu_get_current_apic(s);
	int index;

	if (!apic) {
		APIC_LOG(ERR, "No LAPIC attached to current VCPU.\n");
		return 0;
	}

	index = (addr >> 4) & 0xff;

	switch(index) {
	case 0x02:
		s->id = (val >> 24);
		break;
	case 0x03:
		break;
	case 0x08:
		s->tpr = val;
		apic_update_irq(s);
		break;
	case 0x09:
	case 0x0a:
		break;
	case 0x0b: /* EOI */
		apic_eoi(s);
		break;
	case 0x0d:
		s->log_dest = val >> 24;
		break;
	case 0x0e:
		s->dest_mode = val >> 28;
		break;
	case 0x0f:
		s->spurious_vec = val & 0x1ff;
		apic_update_irq(s);
		break;
	case 0x10 ... 0x17:
	case 0x18 ... 0x1f:
	case 0x20 ... 0x27:
	case 0x28:
		break;
	case 0x30:
		s->icr[0] = val;
		apic_deliver(s, (s->icr[1] >> 24) & 0xff, (s->icr[0] >> 11) & 1,
			     (s->icr[0] >> 8) & 7, (s->icr[0] & 0xff),
			     (s->icr[0] >> 15) & 1);
		break;
	case 0x31:
		s->icr[1] = val;
		break;
	case 0x32 ... 0x37:
		{
			int n = index - 0x32;
			s->lvt[n] = val;
			if (n == APIC_LVT_TIMER) {
				apic_timer_update(s, vmm_timer_timestamp());
			} else if (n == APIC_LVT_LINT0) {
				apic_update_irq(s);
			}
		}
		break;
	case 0x38:
		s->initial_count = val;
		s->initial_count_load_time = vmm_timer_timestamp();
		apic_timer_update(s, s->initial_count_load_time);
		break;
	case 0x39:
		break;
	case 0x3e:
		{
			int v;
			s->divide_conf = val & 0xb;
			v = (s->divide_conf & 3) | ((s->divide_conf >> 1) & 4);
			s->count_shift = (v + 1) & 7;
		}
		break;
	default:
		s->esr |= ESR_ILLEGAL_ADDRESS;
		break;
	}

	return VMM_OK;
}

static int apic_emulator_read8(struct vmm_emudev *edev,
			       physical_addr_t offset, 
			       u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = apic_ioport_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int apic_emulator_read16(struct vmm_emudev *edev,
				physical_addr_t offset, 
				u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = apic_ioport_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int apic_emulator_read32(struct vmm_emudev *edev,
				physical_addr_t offset, 
				u32 *dst)
{
	int ret_val = apic_ioport_read(edev->priv, offset, dst);

	return ret_val;
}

static int apic_emulator_write8(struct vmm_emudev *edev,
				physical_addr_t offset, 
				u8 src)
{
	int ret_val = apic_ioport_write(edev->priv, offset, 0xFFFFFF00, src);

	return ret_val;
}

static int apic_emulator_write16(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u16 src)
{
	int ret_val = apic_ioport_write(edev->priv, offset, 0xFFFF0000, src);

	return ret_val;
}

static int apic_emulator_write32(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u32 src)
{
	int ret_val = apic_ioport_write(edev->priv, offset, 0x00000000, src);

	return ret_val;
}

void cpu_set_apic_base(apic_state_t *s, u64 val)
{
	/* FIXME: Change the APIC base and move the region. */
}

u64 cpu_get_apic_base(apic_state_t *s)
{
	if (s)
		return s->apicbase;

	return 0;
}

bool cpu_is_bsp(apic_state_t *s)
{
	u64 base = cpu_get_apic_base(s);

	if (!base) {
		APIC_LOG(ERR, "LAPIC base not set for vcpu %s\n", s->vcpu->name);
		return 0;
	}

	return (base & MSR_IA32_APICBASE_BSP);
}

void cpu_set_apic_tpr(apic_state_t *s, u8 val)
{
	if (!s) {
		return;
	}

	/* FIXME: Set the TPR */
}

u8 cpu_get_apic_tpr(apic_state_t *s)
{
	if (!s) {
		return 0;
	}

	/* FIXME: Return TPR */
	return 0;
}

void apic_init_reset(apic_state_t *s)
{
	int i;

	if (!s) {
		return;
	}
	s->tpr = 0;
	s->spurious_vec = 0xff;
	s->log_dest = 0;
	s->dest_mode = 0xf;
	memset(s->isr, 0, sizeof(s->isr));
	memset(s->tmr, 0, sizeof(s->tmr));
	memset(s->irr, 0, sizeof(s->irr));
	for (i = 0; i < APIC_LVT_NB; i++) {
		s->lvt[i] = APIC_LVT_MASKED;
	}
	s->esr = 0;
	memset(s->icr, 0, sizeof(s->icr));
	s->divide_conf = 0;
	s->count_shift = 0;
	s->initial_count = 0;
	s->initial_count_load_time = 0;
	s->next_time = 0;
	s->wait_for_sipi = 1;

	vmm_timer_event_stop(&s->timer);

	s->timer_expiry = -1;
}

void apic_designate_bsp(apic_state_t *s)
{
	if (s) {
		return;
	}

	s->apicbase |= MSR_IA32_APICBASE_BSP;
}

static void apic_reset_common(apic_state_t *s)
{
	bool bsp;

	bsp = cpu_is_bsp(s);
	s->apicbase = APIC_DEFAULT_ADDRESS |
		(bsp ? MSR_IA32_APICBASE_BSP : 0) | MSR_IA32_APICBASE_ENABLE;

	apic_init_reset(s);

	if (bsp) {
		/*
		 * LINT0 delivery mode on CPU #0 is set to ExtInt at initialization
		 * time typically by BIOS, so PIC interrupt can be delivered to the
		 * processor when local APIC is enabled.
		 */
		s->lvt[APIC_LVT_LINT0] = 0x700;
	}
}

static int apic_emulator_remove(struct vmm_emudev *edev)
{
	apic_state_t *s = edev->priv;

	if (!s) {
		return VMM_EFAIL;
	}

	vmm_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devemu_irqchip apic_irqchip = {
	.name = "APIC",
	.handle = apic_irq_handle,
};

static int apic_emulator_probe(struct vmm_guest *guest,
			       struct vmm_emudev *edev,
			       const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	apic_state_t *s, *e;
	int i = 0;
	irq_flags_t flags;
	int nr_vcpus = guest->vcpu_count;
	struct vmm_vcpu *vcpu;

	APIC_LOG(VERBOSE, "Probe start\n");
	s = vmm_zalloc(sizeof(apic_state_t) * nr_vcpus);
	if (!s) {
		APIC_LOG(ERR, "APIC state allocation failed!\n");
		rc = VMM_ENOMEM;
		goto apic_emulator_probe_done;
	}

	APIC_LOG(VERBOSE, "%d APICs in system.\n", nr_vcpus);

	/* initialize and assign each LAPIC to its vcpu */
	vmm_read_lock_irqsave_lite(&guest->vcpu_lock, flags);

	list_for_each_entry(vcpu, &guest->vcpu_list, head) {
		e = s + i;
		e->guest = guest;
		e->vcpu = vcpu;
		e->id = i; /* APIC ID  (RO) */
		INIT_SPIN_LOCK(&s->state_lock);
		apic_reset_common(e);
		i++;
	}

	vmm_read_unlock_irqrestore_lite(&guest->vcpu_lock, flags);

	INIT_TIMER_EVENT(&s->timer, &apic_timer, s);

	if ((rc = vmm_devtree_read_u32(edev->node, "base_irq", &s->base_irq)) != VMM_OK) {
		APIC_LOG(ERR, "Base IRQ not defined!\n");
		goto apic_emulator_probe_freestate_fail;
	}

	if ((rc = vmm_devtree_read_u32(edev->node, "num_irq", &s->num_irq)) != VMM_OK) {
		APIC_LOG(ERR, "Number of IRQ not defined!\n");
		goto apic_emulator_probe_freestate_fail;
	}

	for (i = s->base_irq; i < (s->base_irq + s->num_irq); i++) {
		vmm_devemu_register_irqchip(guest, i, &apic_irqchip, s);
	}

	edev->priv = s;

	return VMM_OK;

 apic_emulator_probe_freestate_fail:
	vmm_free(s);

 apic_emulator_probe_done:
	return rc;
}

static int apic_emulator_reset(struct vmm_emudev *edev)
{
	apic_state_t *s = edev->priv;
	int i, nr_vcpu = s->vcpu->guest->vcpu_count;

	APIC_LOG(VERBOSE, "Emulator reset.\n");

	for (i = 0; i < nr_vcpu; i++) {
		apic_reset_common(s);
		s++;
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid apic_emulator_emuid_table[] = {
	{
		.type = "pic",
		.compatible = "lapic",
	},
	{ /* end of list */ },
};

static struct vmm_emulator apic_emulator = {
	.name =        "lapic",
	.match_table = apic_emulator_emuid_table,
	.endian =      VMM_DEVEMU_LITTLE_ENDIAN,
	.probe =       apic_emulator_probe,
	.read8 =       apic_emulator_read8,
	.write8 =      apic_emulator_write8,
	.read16 =      apic_emulator_read16,
	.write16 =     apic_emulator_write16,
	.read32 =      apic_emulator_read32,
	.write32 =     apic_emulator_write32,
	.reset =       apic_emulator_reset,
	.remove =      apic_emulator_remove,
};

static int __init apic_emulator_init(void)
{
	return vmm_devemu_register_emulator(&apic_emulator);
}

static void __exit apic_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&apic_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
