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

struct cpu_apic apic;

static u32 is_apic_present(void)
{
	u32 a, b, c, d;

	cpuid(CPUID_GETFEATURES,
	      &a, &b, &c, &d);

	return (d & CPUID_FEAT_EDX_APIC);
}

int apic_init(void)
{
	u32 apic_version;

	/* Configuration says that  support APIC but its not present! */
	BUG_ON(!is_apic_present(), "No Local APIC Detected in System!\n");

	apic.msr = cpu_read_msr(MSR_APIC);

	if (!APIC_ENABLED(apic.msr)) {
		apic.msr |= (0x1UL << 11);
		cpu_write_msr(MSR_APIC, apic.msr);
	}

	apic.pbase = (APIC_BASE(apic.msr) << 12);

	/* remap base */
	apic.vbase = vmm_host_iomap(apic.pbase, PAGE_SIZE);

	BUG_ON(unlikely(apic.vbase == 0), "APIC Base mapping failed!\n");

	apic_version = apic_read(apic.vbase, APIC_VERSION);

	apic.integrated = IS_INTEGRATED_APIC(apic_version);
	apic.nr_lvt = NR_LVT_ENTRIES(apic_version);

	return VMM_OK;
}
