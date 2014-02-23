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
#include <vmm_host_irq.h>
#include <libs/list.h>

extern virtual_addr_t lapic_eoi_addr;

#define APIC_NAME_LEN			256
#define APIC_ENABLE			0x100
#define APIC_FOCUS_DISABLED		(1 << 9)
#define APIC_SIV			0xFF

#define APIC_TDCR_2			0x00
#define APIC_TDCR_4			0x01
#define APIC_TDCR_8			0x02
#define APIC_TDCR_16			0x03
#define APIC_TDCR_32			0x08
#define APIC_TDCR_64			0x09
#define APIC_TDCR_128			0x0a
#define APIC_TDCR_1			0x0b

#define APIC_LVTT_VECTOR_MASK		0x000000FF
#define APIC_LVTT_DS_PENDING		(1 << 12)
#define APIC_LVTT_MASK			(1 << 16)
#define APIC_LVTT_TM			(1 << 17)

#define APIC_LVT_IIPP_MASK		0x00002000
#define APIC_LVT_IIPP_AH		0x00002000
#define APIC_LVT_IIPP_AL		0x00000000

#define IOAPIC_REGSEL			0x00
#define IOAPIC_RW			0x10

#define APIC_ICR_DM_MASK		0x00000700
#define APIC_ICR_VECTOR			APIC_LVTT_VECTOR_MASK
#define APIC_ICR_DM_FIXED		(0 << 8)
#define APIC_ICR_DM_LOWEST_PRIORITY	(1 << 8)
#define APIC_ICR_DM_SMI			(2 << 8)
#define APIC_ICR_DM_RESERVED		(3 << 8)
#define APIC_ICR_DM_NMI			(4 << 8)
#define APIC_ICR_DM_INIT		(5 << 8)
#define APIC_ICR_DM_STARTUP		(6 << 8)
#define APIC_ICR_DM_EXTINT		(7 << 8)

#define APIC_ICR_DM_PHYSICAL		(0 << 11)
#define APIC_ICR_DM_LOGICAL		(1 << 11)

#define APIC_ICR_DELIVERY_PENDING	(1 << 12)

#define APIC_ICR_INT_POLARITY		(1 << 13)

#define APIC_ICR_LEVEL_ASSERT		(1 << 14)
#define APIC_ICR_LEVEL_DEASSERT		(0 << 14)

#define APIC_ICR_TRIGGER		(1 << 15)

#define APIC_ICR_INT_MASK		(1 << 16)

#define APIC_ICR_DEST_FIELD		(0 << 18)
#define APIC_ICR_DEST_SELF		(1 << 18)
#define APIC_ICR_DEST_ALL		(2 << 18)
#define APIC_ICR_DEST_ALL_BUT_SELF	(3 << 18)

#define LOCAL_APIC_DEF_PHYS_BASE	0xFEE00000ULL
#define IOAPIC_DEF_PHYS_BASE		0xFEC00000ULL

#define APIC_ISR_BASE			(0x100) /* In-service interrrupt base */
#define APIC_ISR_0			(APIC_ISR_BASE + 0x00)
#define APIC_ISR_32			(APIC_ISR_BASE + 0x10)
#define APIC_ISR_64			(APIC_ISR_BASE + 0x20)
#define APIC_ISR_96			(APIC_ISR_BASE + 0x30)
#define APIC_ISR_128			(APIC_ISR_BASE + 0x40)
#define APIC_ISR_160			(APIC_ISR_BASE + 0x50)
#define APIC_ISR_192			(APIC_ISR_BASE + 0x60)
#define APIC_ISR_224			(APIC_ISR_BASE + 0x70)

#define APIC_TMR_BASE			(APIC_ISR_224)
#define APIC_TMR_0			(APIC_TMR_BASE + 0x00)
#define APIC_TMR_32			(APIC_TMR_BASE + 0x10)
#define APIC_TMR_64			(APIC_TMR_BASE + 0x20)
#define APIC_TMR_96			(APIC_TMR_BASE + 0x30)
#define APIC_TMR_128			(APIC_TMR_BASE + 0x40)
#define APIC_TMR_160			(APIC_TMR_BASE + 0x50)
#define APIC_TMR_192			(APIC_TMR_BASE + 0x60)
#define APIC_TMR_224			(APIC_TMR_BASE + 0x70)

#define APIC_IRR_BASE			(APIC_TMR_224)
#define APIC_IRR_0			(APIC_IRR_BASE + 0x00)
#define APIC_IRR_32			(APIC_IRR_BASE + 0x10)
#define APIC_IRR_64			(APIC_IRR_BASE + 0x20)
#define APIC_IRR_96			(APIC_IRR_BASE + 0x30)
#define APIC_IRR_128			(APIC_IRR_BASE + 0x40)
#define APIC_IRR_160			(APIC_IRR_BASE + 0x50)
#define APIC_IRR_192			(APIC_IRR_BASE + 0x60)
#define APIC_IRR_224			(APIC_IRR_BASE + 0x70)

#define APIC_ERROR_STATUS       	(0x280)
#define APIC_LVT_CMCI			(0x2F0)
#define APIC_ICR_0			(0x300)
#define APIC_ICR_32			(0x310)
#define APIC_LVT_TIMER			(0x320)
#define APIC_LVT_THERM_SENSOR		(0x330)
#define APIC_LVT_PERF_MON		(0x340)
#define APIC_LVT_INT0			(0x350)
#define APIC_LVT_INT1			(0x360)
#define APIC_LVT_ERR			(0x370)
#define APIC_INIT_COUNT			(0x380)
#define APIC_CURR_COUNT			(0x390)
#define APIC_DIVIDE_CONF		(0x3E0)


#define NR_IOAPIC_PINS			24

#define IOAPIC_ID			0x0
#define IOAPIC_VERSION			0x1
#define IOAPIC_ARB			0x2
#define IOAPIC_REDIR_TABLE		0x10

#define APIC_TIMER_INT_VECTOR		0xf0
#define APIC_SMP_SCHED_PROC_VECTOR	0xf1
#define APIC_SMP_CPU_HALT_VECTOR	0xf2
#define APIC_ERROR_INT_VECTOR		0xfe
#define APIC_SPURIOUS_INT_VECTOR	0xff

#define LAPIC_ID(__vbase)		(__vbase + 0x020)
#define LAPIC_VERSION(__vbase)		(__vbase + 0x030)
#define LAPIC_TPR(__vbase)		(__vbase + 0x080)
#define LAPIC_EOI(__vbase)		(__vbase + 0x0b0)
#define LAPIC_LDR(__vbase)		(__vbase + 0x0d0)
#define LAPIC_DFR(__vbase)		(__vbase + 0x0e0)
#define LAPIC_SIVR(__vbase)		(__vbase + 0x0f0)
#define LAPIC_ISR(__vbase)		(__vbase + 0x100)
#define LAPIC_TMR(__vbase)		(__vbase + 0x180)
#define LAPIC_IRR(__vbase)		(__vbase + 0x200)
#define LAPIC_ESR(__vbase)		(__vbase + 0x280)
#define LAPIC_ICR1(__vbase)		(__vbase + 0x300)
#define LAPIC_ICR2(__vbase)		(__vbase + 0x310)
#define LAPIC_LVTTR(__vbase)		(__vbase + 0x320)
#define LAPIC_LVTTMR(__vbase)		(__vbase + 0x330)
#define LAPIC_LVTPCR(__vbase)		(__vbase + 0x340)
#define LAPIC_LINT0(__vbase)		(__vbase + 0x350)
#define LAPIC_LINT1(__vbase)		(__vbase + 0x360)
#define LAPIC_LVTER(__vbase)		(__vbase + 0x370)
#define LAPIC_TIMER_ICR(__vbase)	(__vbase + 0x380)
#define LAPIC_TIMER_CCR(__vbase)	(__vbase + 0x390)
#define LAPIC_TIMER_DCR(__vbase)	(__vbase + 0x3e0)

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

#define apic_eoi()				\
	do {					\
		*((volatile u32 *)		\
		  lapic_eoi_addr) = 0;		\
	} while(0)

struct cpu_ioapic {
	char name[APIC_NAME_LEN];
	u32 id;
	physical_addr_t paddr;
	virtual_addr_t vaddr;
	u32 version;
	unsigned int pins;
	unsigned int gsi_base;
	struct vmm_host_irq_chip irq_chip[CONFIG_HOST_IRQ_COUNT];
};

struct cpu_lapic {
	u32 id;
	physical_addr_t pbase;
	virtual_addr_t vbase;
	u32 msr;
	u32 integrated;
	u32 nr_lvt;
	u32 version;
};

union ioapic_irt_entry {
	u64 val;
	struct _bits {
		u32 intvec:8;
		u32 delmod:3;
		u32 destmod:1;
		u32 delivs:1;
		u32 intpol:1;
		u32 rirr:1;
		u32 trigger:1;
		u32 mask:1;
		u64 resvd1:39;
		u32 dest:8;
	} bits;
};

#define EXT_DEV_NAME_LEN	256

/**
 * @brief Software abstraction of a device like HPET connected to
 * IOAPIC.
 */
struct ioapic_ext_irq_device {
	char ext_dev_name[EXT_DEV_NAME_LEN];
	void (*irq_enable)(void *data);
	void (*irq_disable)(void *data);
	void (*irq_ack)(void *data);
	void (*irq_mask)(void *data);
	void (*irq_unmask)(void *data);
	void (*irq_eoi)(void *data);
	int  (*irq_set_type)(void *data, u32 flow_type);
	vmm_irq_return_t  (*irq_handler)(u32 irq_no, void *data);
	void *data;
	struct dlist head;
};

int apic_init(void);

/** Route an IOAPIC pin to specific IRQ vector */
int ioapic_route_pin_to_irq(u32 pin, u32 irqno);

/** Attach an extern device to given IRQ line. */
int ioapic_set_ext_irq_device(u32 irqno, struct ioapic_ext_irq_device *device,
			      void *data);

#endif /* __CPU_APIC_H__ */
