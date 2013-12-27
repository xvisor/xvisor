/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file mct.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Exynos MCT timer code
 *
 * Adapted from linux/arch/arm/mach-exynos4/mct.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4 MCT(Multi-Core Timer) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <vmm_types.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_smp.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <vmm_wallclock.h>
#include <vmm_heap.h>
#include <vmm_error.h>
#include <vmm_delay.h>

#include <exynos/irqs.h>
#include <exynos/plat/cpu.h>
#include <exynos/mct_timer.h>

#define HZ		(1000 / CONFIG_TSLICE_MS)

static void *exynos4_sys_timer;

static inline u32 exynos4_mct_read(u32 offset)
{
	return vmm_readl(exynos4_sys_timer + offset);
}

static void exynos4_mct_write(u32 value, u32 offset)
{
	u32 stat_addr;
	u32 mask;
	u32 i;

	vmm_writel(value, exynos4_sys_timer + offset);

	if (likely(offset >= EXYNOS4_MCT_L_BASE(0))) {
		u32 base = offset & EXYNOS4_MCT_L_MASK;
		switch (offset & ~EXYNOS4_MCT_L_MASK) {
		case (u32) MCT_L_TCON_OFFSET:
			stat_addr = base + MCT_L_WSTAT_OFFSET;
			mask = 1 << 3;	/* L_TCON write status */
			break;
		case (u32) MCT_L_ICNTB_OFFSET:
			stat_addr = base + MCT_L_WSTAT_OFFSET;
			mask = 1 << 1;	/* L_ICNTB write status */
			break;
		case (u32) MCT_L_TCNTB_OFFSET:
			stat_addr = base + MCT_L_WSTAT_OFFSET;
			mask = 1 << 0;	/* L_TCNTB write status */
			break;
		default:
			return;
		}
	} else {
		switch (offset) {
		case (u32) EXYNOS4_MCT_G_TCON:
			stat_addr = EXYNOS4_MCT_G_WSTAT;
			mask = 1 << 16;	/* G_TCON write status */
			break;
		case (u32) EXYNOS4_MCT_G_COMP0_L:
			stat_addr = EXYNOS4_MCT_G_WSTAT;
			mask = 1 << 0;	/* G_COMP0_L write status */
			break;
		case (u32) EXYNOS4_MCT_G_COMP0_U:
			stat_addr = EXYNOS4_MCT_G_WSTAT;
			mask = 1 << 1;	/* G_COMP0_U write status */
			break;
		case (u32) EXYNOS4_MCT_G_COMP0_ADD_INCR:
			stat_addr = EXYNOS4_MCT_G_WSTAT;
			mask = 1 << 2;	/* G_COMP0_ADD_INCR w status */
			break;
		case (u32) EXYNOS4_MCT_G_CNT_L:
			stat_addr = EXYNOS4_MCT_G_CNT_WSTAT;
			mask = 1 << 0;	/* G_CNT_L write status */
			break;
		case (u32) EXYNOS4_MCT_G_CNT_U:
			stat_addr = EXYNOS4_MCT_G_CNT_WSTAT;
			mask = 1 << 1;	/* G_CNT_U write status */
			break;
		default:
			return;
		}
	}

	/* Wait maximum 1 ms until written values are applied */
	for (i = 0; i < 1000; i++) {
		/* So we do this loop up to 1000 times */
		if (exynos4_mct_read(stat_addr) & mask) {
			vmm_writel(mask, exynos4_sys_timer + stat_addr);
			return;
		}
		/* We sleep 1 µs each time */
		vmm_udelay(1);
	}

	vmm_panic("MCT hangs after writing %d (offset:0x%03x)\n", value,
		  offset);
}

static u64 exynos4_frc_read(struct vmm_clocksource *cs)
{
	u32 lo, hi;

	u32 hi2 = exynos4_mct_read(EXYNOS4_MCT_G_CNT_U);

	do {
		hi = hi2;
		lo = exynos4_mct_read(EXYNOS4_MCT_G_CNT_L);
		hi2 = exynos4_mct_read(EXYNOS4_MCT_G_CNT_U);
	} while (hi != hi2);

	return ((u64) hi << 32) | lo;
}

static struct vmm_clocksource mct_frc;

int __init exynos4_clocksource_init(virtual_addr_t base,
				    const char *name,
				    int rating, u32 freq_hz)
{
	u32 reg;

	exynos4_sys_timer = (void *)base;

	/* Fill the clocksource structure */
	mct_frc.name = name;
	mct_frc.rating = rating;
	mct_frc.read = exynos4_frc_read;
	mct_frc.mask = VMM_CLOCKSOURCE_MASK(64);
	vmm_clocks_calc_mult_shift(&mct_frc.mult, 
				   &mct_frc.shift, 
				   freq_hz, NSEC_PER_SEC, 5);
	mct_frc.priv = NULL;

	/* Start the clocksource timer */
	exynos4_mct_write(0, EXYNOS4_MCT_G_CNT_L);
	exynos4_mct_write(0, EXYNOS4_MCT_G_CNT_U);
	reg = exynos4_mct_read(EXYNOS4_MCT_G_TCON);
	reg |= MCT_G_TCON_START;
	exynos4_mct_write(reg, EXYNOS4_MCT_G_TCON);

	/* register the clocksource to Xvisor */
	return vmm_clocksource_register(&mct_frc);
}

static void exynos4_mct_comp0_stop(void)
{
	u32 tcon;

	tcon = exynos4_mct_read(EXYNOS4_MCT_G_TCON);
	tcon &= ~(MCT_G_TCON_COMP0_ENABLE | MCT_G_TCON_COMP0_AUTO_INC);

	exynos4_mct_write(tcon, EXYNOS4_MCT_G_TCON);
	exynos4_mct_write(0, EXYNOS4_MCT_G_INT_ENB);
}

static void exynos4_mct_comp0_start(enum vmm_clockchip_mode mode, u32 cycles)
{
	u32 tcon;
	u64 comp_cycle;

	tcon = exynos4_mct_read(EXYNOS4_MCT_G_TCON);

	if (mode == VMM_CLOCKCHIP_MODE_PERIODIC) {
		tcon |= MCT_G_TCON_COMP0_AUTO_INC;
		exynos4_mct_write(cycles, EXYNOS4_MCT_G_COMP0_ADD_INCR);
	}

	comp_cycle = exynos4_frc_read(&mct_frc) + cycles;

	exynos4_mct_write((u32) comp_cycle, EXYNOS4_MCT_G_COMP0_L);
	exynos4_mct_write((u32) (comp_cycle >> 32), EXYNOS4_MCT_G_COMP0_U);

	exynos4_mct_write(0x1, EXYNOS4_MCT_G_INT_ENB);

	tcon |= MCT_G_TCON_COMP0_ENABLE;
	exynos4_mct_write(tcon, EXYNOS4_MCT_G_TCON);
}

static int exynos4_comp_set_next_event(unsigned long cycles,
				       struct vmm_clockchip *evt)
{
	exynos4_mct_comp0_start(evt->mode, cycles);

	return VMM_OK;
}

static void exynos4_comp_set_mode(enum vmm_clockchip_mode mode,
				  struct vmm_clockchip *evt)
{
	u32 cycles;

	exynos4_mct_comp0_stop();

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_PERIODIC:
		/* We need to start the timer for one tick */
		cycles = (((u64) NSEC_PER_SEC / HZ * evt->mult) >> evt->shift);
		exynos4_mct_comp0_start(mode, cycles);
		break;

	case VMM_CLOCKCHIP_MODE_ONESHOT:
	case VMM_CLOCKCHIP_MODE_UNUSED:
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
	case VMM_CLOCKCHIP_MODE_RESUME:
		break;
	}
}

static vmm_irq_return_t exynos4_mct_comp_isr(int irq_no, void *dev)
{
	struct vmm_clockchip *evt = dev;

	exynos4_mct_write(0x1, EXYNOS4_MCT_G_INT_CSTAT);

	evt->event_handler(evt);

	return VMM_IRQ_HANDLED;
}

static struct vmm_clockchip mct_comp_device;

int __init exynos4_clockchip_init(virtual_addr_t base, u32 hirq,
				  const char *name, int rating,
				  u32 freq_hz, u32 target_cpu)
{
	int rc;

	mct_comp_device.name = name;
	mct_comp_device.hirq = hirq;
	mct_comp_device.rating = rating;
#ifdef CONFIG_SMP
	mct_comp_device.cpumask = vmm_cpumask_of(target_cpu);
#else
	mct_comp_device.cpumask = cpu_all_mask;
#endif
	mct_comp_device.features =
	    VMM_CLOCKCHIP_FEAT_PERIODIC | VMM_CLOCKCHIP_FEAT_ONESHOT;
	vmm_clocks_calc_mult_shift(&mct_comp_device.mult, 
				   &mct_comp_device.shift, 
				   NSEC_PER_SEC, freq_hz, 5);
	mct_comp_device.min_delta_ns =
	    vmm_clockchip_delta2ns(0xF, &mct_comp_device);
	mct_comp_device.max_delta_ns =
	    vmm_clockchip_delta2ns(0xFFFFFFFF, &mct_comp_device);
	mct_comp_device.set_mode = exynos4_comp_set_mode;
	mct_comp_device.set_next_event = exynos4_comp_set_next_event;
	mct_comp_device.priv = NULL;

	/* Register interrupt handler */
	if ((rc =
	     vmm_host_irq_register(hirq, name, exynos4_mct_comp_isr,
				   &mct_comp_device))) {
		return rc;
	}
#ifdef CONFIG_SMP
	/* Set host irq affinity to target cpu */
	if ((rc =
	     vmm_host_irq_set_affinity(hirq, vmm_cpumask_of(target_cpu),
				       TRUE))) {
		return rc;
	}
#endif

	return vmm_clockchip_register(&mct_comp_device);
}

#ifdef CONFIG_SAMSUNG_MCT_LOCAL_TIMERS

#include <vmm_percpu.h>
#include <gic.h>

enum {
	MCT_INT_SPI = 0,
	MCT_INT_PPI,
	MCT_INT_UNKNOWN
};

#define MCT_L_BASE_CNT	1
#define MCT_L_MAX_COUNT	0x7FFFFFFF
#define MCT_L_MIN_COUNT	0xF

static u32 mct_int_type = MCT_INT_UNKNOWN;

void exynos4_timer_init(void);

struct mct_clock_event_clockchip {
	char name[32];
	u32 timer_base;
	struct vmm_clockchip clkchip;
};

static DEFINE_PER_CPU(struct mct_clock_event_clockchip, percpu_mct_tick);

/* Clock event handling */
static void exynos4_mct_tick_stop(struct mct_clock_event_clockchip *mevt)
{
	u32 tmp;
	u32 mask = MCT_L_TCON_INT_START | MCT_L_TCON_TIMER_START;

	tmp = exynos4_mct_read(mevt->timer_base + MCT_L_TCON_OFFSET);
	if (tmp & mask) {
		tmp &= ~mask;
		exynos4_mct_write(tmp, mevt->timer_base + MCT_L_TCON_OFFSET);
	}
}

static void exynos4_mct_tick_start(u32 cycles,
				   struct mct_clock_event_clockchip *mevt)
{
	u32 tmp;

	exynos4_mct_tick_stop(mevt);

	tmp = MCT_L_ICNTB_MANUAL_UPDATE | cycles;

	/* update interrupt count buffer */
	exynos4_mct_write(tmp, mevt->timer_base + MCT_L_ICNTB_OFFSET);

	/* enable MCT tick interrupt */
	exynos4_mct_write(0x1, mevt->timer_base + MCT_L_INT_ENB_OFFSET);

	tmp = exynos4_mct_read(mevt->timer_base + MCT_L_TCON_OFFSET);
	tmp |= MCT_L_TCON_INT_START | 
		MCT_L_TCON_TIMER_START |
		MCT_L_TCON_INTERVAL_MODE;
	exynos4_mct_write(tmp, mevt->timer_base + MCT_L_TCON_OFFSET);
}

static int exynos4_tick_set_next_event(unsigned long cycles,
				       struct vmm_clockchip *evt)
{
	struct mct_clock_event_clockchip *mevt = &this_cpu(percpu_mct_tick);

	exynos4_mct_tick_start(cycles, mevt);

	return VMM_OK;
}

static inline void exynos4_tick_set_mode(enum vmm_clockchip_mode mode,
					 struct vmm_clockchip *evt)
{
	struct mct_clock_event_clockchip *mevt = &this_cpu(percpu_mct_tick);
	u32 cycles;

	exynos4_mct_tick_stop(mevt);

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_PERIODIC:
		cycles = (((u64) NSEC_PER_SEC / HZ * evt->mult) >> evt->shift);
		exynos4_mct_tick_start(cycles, mevt);
		break;

	case VMM_CLOCKCHIP_MODE_ONESHOT:
	case VMM_CLOCKCHIP_MODE_UNUSED:
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
	case VMM_CLOCKCHIP_MODE_RESUME:
		break;
	}
}

static int exynos4_mct_tick_clear(struct mct_clock_event_clockchip *mevt)
{
	struct vmm_clockchip *evt = &mevt->clkchip;

	/*
	 * This is for supporting oneshot mode.
	 * MCT would generate interrupt periodically
	 * without explicit stopping.
	 */
	if (evt->mode != VMM_CLOCKCHIP_MODE_PERIODIC) {
		exynos4_mct_tick_stop(mevt);
	}

	/* Clear the MCT tick interrupt */
	if (exynos4_mct_read(mevt->timer_base + MCT_L_INT_CSTAT_OFFSET) & 1) {
		exynos4_mct_write(0x1, 
				  mevt->timer_base + MCT_L_INT_CSTAT_OFFSET);
		return 1;
	} else {
		return 0;
	}
}

static vmm_irq_return_t exynos4_mct_tick_isr(int irq_no, void *dev_id)
{
	struct mct_clock_event_clockchip *mevt = dev_id;

	exynos4_mct_tick_clear(mevt);

	mevt->clkchip.event_handler(&mevt->clkchip);

	return VMM_IRQ_HANDLED;
}

int __cpuinit exynos4_local_timer_init(virtual_addr_t timer_base, u32 hirq,
				       const char *name, int rating,
				       u32 freq_hz)
{
	int rc;
	struct mct_clock_event_clockchip *mevt;
	struct vmm_clockchip *evt;
	u32 cpu = vmm_smp_processor_id();

	if (mct_int_type == MCT_INT_UNKNOWN) {
		exynos4_timer_init();
	}

	mevt = &this_cpu(percpu_mct_tick);
	evt = &(mevt->clkchip);

	mevt->timer_base = EXYNOS4_MCT_L_BASE(cpu);
	vmm_sprintf(mevt->name, "mct_tick%d", cpu);

	evt->name = mevt->name;
	evt->cpumask = vmm_cpumask_of(cpu);
	evt->set_next_event = exynos4_tick_set_next_event;
	evt->set_mode = exynos4_tick_set_mode;
	evt->features =
	    VMM_CLOCKCHIP_FEAT_PERIODIC | VMM_CLOCKCHIP_FEAT_ONESHOT;
	evt->rating = 450;
	vmm_clocks_calc_mult_shift(&evt->mult, 
				   &evt->shift, 
				   NSEC_PER_SEC, 
				   freq_hz / (MCT_L_BASE_CNT + 1), 10);
	evt->max_delta_ns = vmm_clockchip_delta2ns(MCT_L_MAX_COUNT, evt);
	evt->min_delta_ns = vmm_clockchip_delta2ns(MCT_L_MIN_COUNT, evt);
	evt->priv = mevt;

	exynos4_mct_write(MCT_L_BASE_CNT, 
			  mevt->timer_base + MCT_L_TCNTB_OFFSET);

	if (mct_int_type == MCT_INT_SPI) {
		if (vmm_smp_is_bootcpu()) {
			rc = vmm_host_irq_register(EXYNOS4_IRQ_MCT_L0,
						   "mct_tick0_irq",
						    exynos4_mct_tick_isr, mevt);
			if (rc) {
				return rc;
			}
			rc = vmm_host_irq_set_affinity(EXYNOS4_IRQ_MCT_L0,
						       vmm_cpumask_of(cpu),
						       TRUE);
			if (rc) {
				vmm_host_irq_unregister(EXYNOS4_IRQ_MCT_L0,
							mevt);
				return rc;
			}
		} else {
			rc = vmm_host_irq_register(EXYNOS4_IRQ_MCT_L1,
						   "mct_tick1_irq",
						   exynos4_mct_tick_isr, mevt);
			if (rc) {
				return rc;
			}
			rc = vmm_host_irq_set_affinity(EXYNOS4_IRQ_MCT_L1,
						       vmm_cpumask_of(cpu),
						       TRUE);
			if (rc) {
				vmm_host_irq_unregister(EXYNOS4_IRQ_MCT_L1,
							mevt);
				return rc;
			}
		}
	} else {
		rc = vmm_host_irq_register(EXYNOS_IRQ_MCT_LOCALTIMER,
					   "mct_tick_irq",
					   exynos4_mct_tick_isr, mevt);
		if (rc) {
			return rc;
		}

		rc = vmm_host_irq_mark_per_cpu(EXYNOS_IRQ_MCT_LOCALTIMER);
		if (rc) {
			vmm_host_irq_unregister(EXYNOS_IRQ_MCT_LOCALTIMER,
						mevt);
			return rc;
		}

		gic_enable_ppi(EXYNOS_IRQ_MCT_LOCALTIMER);
	}

	return vmm_clockchip_register(evt);
}

void __init exynos4_timer_init(void)
{
	if ((soc_is_exynos4210()) || (soc_is_exynos5250())) {
		mct_int_type = MCT_INT_SPI;
	} else {
		mct_int_type = MCT_INT_PPI;
	}
}

#endif				/* CONFIG_SAMSUNG_MCT_LOCAL_TIMERS */
