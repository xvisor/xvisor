/*
 *  APIC support - internal interfaces
 *
 *  Copyright (c) 2004-2005 Fabrice Bellard
 *  Copyright (c) 2011      Jan Kiszka, Siemens AG
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>
 */
#ifndef _APIC_H
#define _APIC_H

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_manager.h>
#include <vmm_timer.h>

/* APIC Local Vector Table */
#define APIC_LVT_TIMER                  0
#define APIC_LVT_THERMAL                1
#define APIC_LVT_PERFORM                2
#define APIC_LVT_LINT0                  3
#define APIC_LVT_LINT1                  4
#define APIC_LVT_ERROR                  5
#define APIC_LVT_NB                     6

/* APIC delivery modes */
#define APIC_DM_FIXED                   0
#define APIC_DM_LOWPRI                  1
#define APIC_DM_SMI                     2
#define APIC_DM_NMI                     4
#define APIC_DM_INIT                    5
#define APIC_DM_SIPI                    6
#define APIC_DM_EXTINT                  7

/* APIC destination mode */
#define APIC_DESTMODE_FLAT              0xf
#define APIC_DESTMODE_CLUSTER           1

#define APIC_TRIGGER_EDGE               0
#define APIC_TRIGGER_LEVEL              1

#define APIC_LVT_TIMER_PERIODIC         (1<<17)
#define APIC_LVT_MASKED                 (1<<16)
#define APIC_LVT_LEVEL_TRIGGER          (1<<15)
#define APIC_LVT_REMOTE_IRR             (1<<14)
#define APIC_INPUT_POLARITY             (1<<13)
#define APIC_SEND_PENDING               (1<<12)

#define ESR_ILLEGAL_ADDRESS		(1 << 7)

#define APIC_SV_DIRECTED_IO             (1<<12)
#define APIC_SV_ENABLE                  (1<<8)

#define VAPIC_ENABLE_BIT                0
#define VAPIC_ENABLE_MASK               (1 << VAPIC_ENABLE_BIT)

#define MAX_APICS			255

#define APIC_DEFAULT_ADDRESS		0xfee00000
#define APIC_SPACE_SIZE			0x100000

typedef struct apic_state apic_state_t;

struct apic_state {
	struct vmm_vcpu *vcpu; /* associated VCPU */
	struct vmm_guest *guest; /* associated guest */
	u32 apicbase;
	u8 id;
	u8 arb_id;
	u8 tpr;
	u32 spurious_vec;
	u8 log_dest;
	u8 dest_mode;
	u32 isr[8];  /* in service register */
	u32 tmr[8];  /* trigger mode register */
	u32 irr[8]; /* interrupt request register */
	u32 lvt[APIC_LVT_NB];
	u32 esr; /* error register */
	u32 icr[2];

	u32 divide_conf;
	int count_shift;
	u32 initial_count;
	s64 initial_count_load_time;
	s64 next_time;
	int idx;
	struct vmm_timer_event timer;
	s64 timer_expiry;
	int sipi_vector;
	int wait_for_sipi;
	u32 num_irq;
	u32 base_irq;

	physical_addr_t vapic_paddr;
	struct vmm_spinlock state_lock;
};

#endif /* !_APIC_H */
