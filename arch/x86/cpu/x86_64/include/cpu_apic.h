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
 * @file cpu_apic.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for APIC
 */

#ifndef __CPU_APIC_H__
#define __CPU_APIC_H__

#include <vmm_types.h>

#define APIC_PHYS_BASE		0xFEE00000ULL
#define IOAPIC_PHYS_BASE	0xFEC00000ULL

#define APIC_ID			(0x20)
#define APIC_VERSION		(0x30)
#define APIC_TPR		(0x80)
#define APIC_APR		(0x90)
#define APIC_PPR		(0xA0)
#define APIC_EOI		(0xB0)
#define APIC_RRD		(0xC0)
#define APIC_LDR		(0xD0) /* logical destination */
#define APIC_DFR		(0xE0) /* Destination format */
#define APIC_SPURIOUS_INT	(0xF0)

#define APIC_ISR_BASE		(0x100) /* In-service interrrupt base */
#define APIC_ISR_0		(APIC_ISR_BASE + 0x00)
#define APIC_ISR_32		(APIC_ISR_BASE + 0x10)
#define APIC_ISR_64		(APIC_ISR_BASE + 0x20)
#define APIC_ISR_96		(APIC_ISR_BASE + 0x30)
#define APIC_ISR_128		(APIC_ISR_BASE + 0x40)
#define APIC_ISR_160		(APIC_ISR_BASE + 0x50)
#define APIC_ISR_192		(APIC_ISR_BASE + 0x60)
#define APIC_ISR_224		(APIC_ISR_BASE + 0x70)

#define APIC_TMR_BASE		(APIC_ISR_224)
#define APIC_TMR_0		(APIC_TMR_BASE + 0x00)
#define APIC_TMR_32		(APIC_TMR_BASE + 0x10)
#define APIC_TMR_64		(APIC_TMR_BASE + 0x20)
#define APIC_TMR_96		(APIC_TMR_BASE + 0x30)
#define APIC_TMR_128		(APIC_TMR_BASE + 0x40)
#define APIC_TMR_160		(APIC_TMR_BASE + 0x50)
#define APIC_TMR_192		(APIC_TMR_BASE + 0x60)
#define APIC_TMR_224		(APIC_TMR_BASE + 0x70)

#define APIC_IRR_BASE		(APIC_TMR_224)
#define APIC_IRR_0		(APIC_IRR_BASE + 0x00)
#define APIC_IRR_32		(APIC_IRR_BASE + 0x10)
#define APIC_IRR_64		(APIC_IRR_BASE + 0x20)
#define APIC_IRR_96		(APIC_IRR_BASE + 0x30)
#define APIC_IRR_128		(APIC_IRR_BASE + 0x40)
#define APIC_IRR_160		(APIC_IRR_BASE + 0x50)
#define APIC_IRR_192		(APIC_IRR_BASE + 0x60)
#define APIC_IRR_224		(APIC_IRR_BASE + 0x70)

#define APIC_ERROR_STATUS       (0x280)
#define APIC_LVT_CMCI		(0x2F0)
#define APIC_ICR_0		(0x300)
#define APIC_ICR_32		(0x310)
#define APIC_LVT_TIMER		(0x320)
#define APIC_LVT_THERM_SENSOR	(0x330)
#define APIC_LVT_PERF_MON	(0x340)
#define APIC_LVT_INT0		(0x350)
#define APIC_LVT_INT1		(0x360)
#define APIC_LVT_ERR		(0x370)
#define APIC_INIT_COUNT		(0x380)
#define APIC_CURR_COUNT		(0x390)
#define APIC_DIVIDE_CONF	(0x3E0)

#define IS_INTEGRATED_APIC(_x)			\
	({					\
		int ia = 0;			\
		do {				\
			int va = (_x & 0xF);	\
			ia = (va >= 0x10	\
			      && va <= 0x15 ?	\
			      1 : 0);		\
		}while(0);			\
		ia;				\
	})

#define NR_LVT_ENTRIES(_x)			\
	({					\
		int nr_lvt = ((_x >> 16)	\
			      & 0xFF);		\
			(nr_lvt - 1);		\
	})

struct cpu_ioapic {
	physical_addr_t pbase;
	virtual_addr_t vbase;
	u32 version;
};

struct cpu_lapic {
	physical_addr_t pbase;
	virtual_addr_t vbase;
	u32 msr;
	u32 integrated;
	u32 nr_lvt;
	u32 version;
};

int apic_init(void);

#endif /* __CPU_APIC_H__ */
