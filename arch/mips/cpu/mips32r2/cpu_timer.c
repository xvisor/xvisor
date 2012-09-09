/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file cpu_timer.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source code for handling cpu timer functions
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_percpu.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <cpu_interrupts.h>
#include <cpu_timer.h>
#include <cpu_asm_macros.h>

#define CPU_FREQ_MHZ			100
#define MHZ2HZ(_x_)			(u64)(_x_ * 1000 * 1000)

static void mips_clockchip_set_mode(enum vmm_clockchip_mode mode,
				    struct vmm_clockchip *cc)
{
	u32 sr = read_c0_status();

	switch (mode) {
	case VMM_CLOCKCHIP_MODE_ONESHOT:
		/* No need to do anything special for oneshot mode */
		break;
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
		/* Disable the timer interrupts. */
		sr &= ~((0x1UL << 7) << 8);
		write_c0_status(sr);
		break;
	case VMM_CLOCKCHIP_MODE_PERIODIC:
	case VMM_CLOCKCHIP_MODE_UNUSED:
	default:
		break;
	}
}

static int mips_clockchip_set_next_event(unsigned long next,
					 struct vmm_clockchip *cc)
{
	u32 sr = read_c0_status();

	/* Disable the timer interrupts. */
	sr &= ~((0x1UL << 7) << 8);
	write_c0_status(sr);

	/* Setup compare register */
	write_c0_compare(read_c0_count() + next);

	/* Enable the timer interrupts. */
	sr |= ((0x1UL << 7) << 8);
	write_c0_status(sr);

	return 0;
}

static int mips_clockchip_expire(struct vmm_clockchip *cc)
{
	return 0;
}

static struct vmm_clockchip mips_cc = 
{
	.name = "mips_clkchip",
	.hirq = 0,
	.rating = 300,
	.features = VMM_CLOCKCHIP_FEAT_ONESHOT,
	.shift = 32,
	.set_mode = &mips_clockchip_set_mode,
	.set_next_event = &mips_clockchip_set_next_event,
	.expire = &mips_clockchip_expire,
};

static DEFINE_PER_CPU(struct vmm_clockchip, mcc);

s32 handle_internal_timer_interrupt(arch_regs_t *uregs)
{
	mips_cc.event_handler(&mips_cc, uregs);
	return 0;
}

int arch_clockchip_init(void)
{
	struct vmm_clockchip *cc = &this_cpu(mcc);

	memcpy(cc, &mips_cc, sizeof(struct vmm_clockchip));

#if CONFIG_SMP
	cc->cpumask = vmm_cpumask_of(arch_smp_id());
#else
	cc->cpumask = cpu_all_mask;
#endif
	cc->mult = vmm_clockchip_hz2mult(MHZ2HZ(CPU_FREQ_MHZ), 32);
	cc->min_delta_ns = vmm_clockchip_delta2ns(0xF, &mips_cc);
	cc->max_delta_ns = vmm_clockchip_delta2ns(0xFFFFFFFF, &mips_cc);
	cc->priv = NULL;

	/* Disable the timer interrupts. */
	u32 sr = read_c0_status();
	sr &= ~((0x1UL << 7) << 8);
	write_c0_status(sr);

	return vmm_clockchip_register(cc);
}

static u64 mips_clocksource_read(struct vmm_clocksource *cs)
{
	return read_c0_count();
}

static struct vmm_clocksource mips_cs =  
{
	.name = "mips_clksrc",
	.rating = 300,
	.mask = 0xFFFFFFFF,
	.shift = 20,
	.read = &mips_clocksource_read
};

int arch_clocksource_init(void)
{
	int rc;

	/* Register clocksource */
	mips_cs.mult = vmm_clocksource_khz2mult(1000, 20);
	if ((rc = vmm_clocksource_register(&mips_cs))) {
		return rc;
	}

	/* Enable the monotonic count. */
	u32 cause = read_c0_cause();
	cause &= ~(0x1UL << 27);
	write_c0_cause(cause);

	/* Initialize the counter to 0. */
	write_c0_count(0);

	return VMM_OK;
}
