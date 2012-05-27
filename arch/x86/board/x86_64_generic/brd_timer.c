/**
 * Copyright (c) 2012 Himanshu Chauhan.
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
 * @file brd_timer.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Board timer handling. HPET code.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <cpu_interrupts.h>
#include <cpu_timer.h>
#include <cpu_asm_macros.h>

static void hpet_clockchip_set_mode(enum vmm_clockchip_mode mode,
				    struct vmm_clockchip *cc)
{
	switch (mode) {
	case VMM_CLOCKCHIP_MODE_ONESHOT:
		/* No need to do anything special for oneshot mode */
		break;
	case VMM_CLOCKCHIP_MODE_SHUTDOWN:
		break;
	case VMM_CLOCKCHIP_MODE_PERIODIC:
	case VMM_CLOCKCHIP_MODE_UNUSED:
	default:
		break;
	}
}

static int hpet_clockchip_set_next_event(unsigned long next,
					 struct vmm_clockchip *cc)
{
	return 0;
}

static int hpet_clockchip_expire(struct vmm_clockchip *cc)
{
	return 0;
}

static struct vmm_clockchip hpet_cc =
{
	.name = "hpet_clkchip",
	.rating = 300,
	.features = VMM_CLOCKCHIP_FEAT_ONESHOT,
	.shift = 32,
	.set_mode = &hpet_clockchip_set_mode,
	.set_next_event = &hpet_clockchip_set_next_event,
	.expire = &hpet_clockchip_expire,
};

s32 handle_timer_irq(arch_regs_t *uregs)
{
	hpet_cc.event_handler(&hpet_cc, uregs);
	return 0;
}

int arch_clockchip_init(void)
{
	/*
	hpet_cc.mult = vmm_clockchip_hz2mult(MHZ2HZ(CPU_FREQ_MHZ), 32);
	hpet_cc.min_delta_ns = vmm_clockchip_delta2ns(0xF, &mips_cc);
	hpet_cc.max_delta_ns = vmm_clockchip_delta2ns(0xFFFFFFFF, &mips_cc);
	hpet_cc.priv = NULL;
	*/
	return vmm_clockchip_register(&hpet_cc);
}

static u64 hpet_clocksource_read(struct vmm_clocksource *cs)
{
	/* FIXME: Read the counter */
	return 0;
}

static struct vmm_clocksource hpet_cs =
{
	.name = "hpet_clksrc",
	.rating = 300,
	.mask = 0xFFFFFFFF,
	.shift = 20,
	.read = &hpet_clocksource_read
};

int arch_clocksource_init(void)
{
	int rc;

	/* Register clocksource */
	hpet_cs.mult = vmm_clocksource_khz2mult(1000, 20);
	if ((rc = vmm_clocksource_register(&hpet_cs))) {
		return rc;
	}

	return VMM_OK;
}
