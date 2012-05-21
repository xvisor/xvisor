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
 * @file cpu_apic.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Local APIC programming.
 */

#include <vmm_types.h>
#include <cpu_apic.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <cpu_mmu.h>
#include <arch_cpu.h>
#include <cpu_private.h>

struct cpu_lapic lapic; /* should be per-cpu for SMP */
struct cpu_ioapic ioapic;

static u32 is_lapic_present(void)
{
	u32 a, b, c, d;

	cpuid(CPUID_GETFEATURES,
	      &a, &b, &c, &d);

	return (d & CPUID_FEAT_EDX_APIC);
}

static inline u32 lapic_read(virtual_addr_t base, u32 reg)
{
	return (*((volatile u32 *)(base + reg)));
}

static inline void lapic_write(virtual_addr_t base, u32 reg, u32 val)
{
	*((volatile u32 *)(base + reg)) = val;
}


static inline u32 ioapic_read(u32 reg)
{
	u32 *ioapic_vaddr = (u32 *)ioapic.vbase;

	ioapic_vaddr[0] = reg;
	return ioapic_vaddr[4];
}

static inline void ioapic_write(u32 reg, u32 val)
{
	u32 *ioapic_vaddr = (u32 *)ioapic.vbase;

	ioapic_vaddr[0] = reg;
	ioapic_vaddr[4] = val;
}

static int setup_ioapic(void)
{
	ioapic.pbase = IOAPIC_PHYS_BASE;

	/* remap base */
	ioapic.vbase = vmm_host_iomap(ioapic.pbase, PAGE_SIZE);
	BUG_ON(unlikely(ioapic.vbase == 0), "IOAPIC Base mapping failed!\n");

	ioapic.version = ioapic_read(1);

	return 0;
}

int apic_init(void)
{
	/* Configuration says that  support APIC but its not present! */
	BUG_ON(!is_lapic_present(), "No Local APIC Detected in System!\n");

	lapic.msr = cpu_read_msr(MSR_APIC);

	if (!APIC_ENABLED(lapic.msr)) {
		lapic.msr |= (0x1UL << 11);
		cpu_write_msr(MSR_APIC, lapic.msr);
	}

	lapic.pbase = (APIC_BASE(lapic.msr) << 12);

	/* remap base */
	lapic.vbase = vmm_host_iomap(lapic.pbase, PAGE_SIZE);

	BUG_ON(unlikely(lapic.vbase == 0), "APIC Base mapping failed!\n");

	lapic.version = lapic_read(lapic.vbase, APIC_VERSION);

	lapic.integrated = IS_INTEGRATED_APIC(lapic.version);
	lapic.nr_lvt = NR_LVT_ENTRIES(lapic.version);

	setup_ioapic(); /* in SMP only BSP should do it */

	return VMM_OK;
}
