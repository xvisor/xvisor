/**
 * Copyright (c) 2013 Himanshu Chauhan.
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
 * @file cpu_features.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief CPU specific features gathering information.
 */

#include <vmm_types.h>
#include <cpu_features.h>

struct cpuinfo_x86 cpu_info;

/* all the cache descriptor types we care about (no TLB or trace cache entries) */
static struct _cache_table cache_table[] = {
	{ 0x06, LVL_1_INST, 8 },/* 4-way set assoc, 32 byte line size */
	{ 0x08, LVL_1_INST, 16 },/* 4-way set assoc, 32 byte line size */
	{ 0x0a, LVL_1_DATA, 8 },/* 2 way set assoc, 32 byte line size */
	{ 0x0c, LVL_1_DATA, 16 },/* 4-way set assoc, 32 byte line size */
	{ 0x22, LVL_3,      512 },/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x23, LVL_3,      1024 },/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x25, LVL_3,      2048 },/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x29, LVL_3,      4096 },/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x2c, LVL_1_DATA, 32 },/* 8-way set assoc, 64 byte line size */
	{ 0x30, LVL_1_INST, 32 },/* 8-way set assoc, 64 byte line size */
	{ 0x39, LVL_2,      128 },/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x3b, LVL_2,      128 },/* 2-way set assoc, sectored cache, 64 byte line size */
	{ 0x3c, LVL_2,      256 },/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x41, LVL_2,      128 },/* 4-way set assoc, 32 byte line size */
	{ 0x42, LVL_2,      256 },/* 4-way set assoc, 32 byte line size */
	{ 0x43, LVL_2,      512 },/* 4-way set assoc, 32 byte line size */
	{ 0x44, LVL_2,      1024 },/* 4-way set assoc, 32 byte line size */
	{ 0x45, LVL_2,      2048 },/* 4-way set assoc, 32 byte line size */
	{ 0x60, LVL_1_DATA, 16 },/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x66, LVL_1_DATA, 8 },/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x67, LVL_1_DATA, 16 },/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x68, LVL_1_DATA, 32 },/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x70, LVL_TRACE,  12 },/* 8-way set assoc */
	{ 0x71, LVL_TRACE,  16 },/* 8-way set assoc */
	{ 0x72, LVL_TRACE,  32 },/* 8-way set assoc */
	{ 0x78, LVL_2,    1024 },/* 4-way set assoc, 64 byte line size */
	{ 0x79, LVL_2,     128 },/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x7a, LVL_2,     256 },/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x7b, LVL_2,     512 },/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x7c, LVL_2,    1024 },/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x7d, LVL_2,    2048 },/* 8-way set assoc, 64 byte line size */
	{ 0x7f, LVL_2,     512 },/* 2-way set assoc, 64 byte line size */
	{ 0x82, LVL_2,     256 },/* 8-way set assoc, 32 byte line size */
	{ 0x83, LVL_2,     512 },/* 8-way set assoc, 32 byte line size */
	{ 0x84, LVL_2,    1024 },/* 8-way set assoc, 32 byte line size */
	{ 0x85, LVL_2,    2048 },/* 8-way set assoc, 32 byte line size */
	{ 0x86, LVL_2,     512 },/* 4-way set assoc, 64 byte line size */
	{ 0x87, LVL_2,    1024 },/* 8-way set assoc, 64 byte line size */
	{ 0x00, 0, 0}
};

static inline void gather_cpu_brandinfo(struct cpuinfo_x86 *cpu_info)
{
	u32 a, b, c, d;

	cpuid(CPUID_EXTENDED_FEATURES, &a, &b, &c, &d);
	cpu_info->family = ((a >> CPUID_BASE_FAMILY_SHIFT) & CPUID_BASE_FAMILY_MASK);
	if (cpu_info->family == 0xf)
		cpu_info->family += ((a >> CPUID_EXTD_FAMILY_SHIFT) & CPUID_EXTD_FAMILY_MASK);

	cpu_info->model = ((a >> CPUID_BASE_MODEL_SHIFT) & CPUID_BASE_MODEL_MASK);
	cpu_info->model <<= 4;
	cpu_info->model |= ((a >> CPUID_EXTD_MODEL_SHIFT) & CPUID_EXTD_MODEL_MASK);

	cpu_info->stepping = ((a >> CPUID_STEPPING_SHIFT) & CPUID_STEPPING_MASK);

	/* processor identification name */
	cpuid(CPUID_EXTENDED_BRANDSTRING, (u32 *)&cpu_info->name_string[0],
		(u32 *)&cpu_info->name_string[4],
		(u32 *)&cpu_info->name_string[8],
		(u32 *)&cpu_info->name_string[12]);

	cpuid(CPUID_EXTENDED_BRANDSTRINGMORE, (u32 *)&cpu_info->name_string[16],
		(u32 *)&cpu_info->name_string[20],
		(u32 *)&cpu_info->name_string[24],
		(u32 *)&cpu_info->name_string[28]);

	cpuid(CPUID_EXTENDED_BRANDSTRINGEND, (u32 *)&cpu_info->name_string[32],
		(u32 *)&cpu_info->name_string[36],
		(u32 *)&cpu_info->name_string[40],
		(u32 *)&cpu_info->name_string[44]);
}

static inline void gather_amd_features(struct cpuinfo_x86 *cpu_info)
{
	u32 a, b, c, d;

	gather_cpu_brandinfo(cpu_info);

	cpuid(AMD_CPUID_EXTENDED_L1_CACHE_TLB_IDENTIFIER, &a, &b, &c, &d);
	cpu_info->l1_dcache_size = ((c >> CPUID_L1_CACHE_SIZE_SHIFT) & CPUID_L1_CACHE_SIZE_MASK);
	cpu_info->l1_dcache_line_size = ((c >> CPUID_L1_CACHE_LINE_SHIFT) & CPUID_L1_CACHE_LINE_MASK);
	cpu_info->l1_icache_size = ((d >> CPUID_L1_CACHE_SIZE_SHIFT) & CPUID_L1_CACHE_SIZE_MASK);
	cpu_info->l1_icache_line_size = ((d >> CPUID_L1_CACHE_LINE_SHIFT) & CPUID_L1_CACHE_LINE_MASK);

	cpuid(CPUID_EXTENDED_L2_CACHE_TLB_IDENTIFIER, &a, &b, &c, &d);
	cpu_info->l2_cache_size = ((c >> CPUID_L2_CACHE_SIZE_SHIFT) & CPUID_L2_CACHE_SIZE_MASK);
	cpu_info->l2_cache_line_size = ((c >> CPUID_L2_CACHE_LINE_SHIFT) & CPUID_L2_CACHE_LINE_MASK);

	cpuid(CPUID_EXTENDED_FEATURES, &a, &b, &c, &d);
	cpu_info->hw_virt_available = ((c >> 2) & 1);

	if (cpu_info->hw_virt_available) {
		/* Check if nested paging is also available. */
		cpuid(AMD_CPUID_EXTENDED_SVM_IDENTIFIER, &a, &b, &c, &d);
		cpu_info->hw_nested_paging = (d & 0x1UL);
		cpu_info->hw_nr_asids = b;
		cpu_info->decode_assist = ((d >> 7) & 0x1);
	}
}

static inline void gather_intel_cacheinfo(struct cpuinfo_x86 *cpu_info)
{
	int regs[4];
	int i, n, j;
	unsigned char *dp = (unsigned char *)regs;
	unsigned int trace = 0, l1i = 0, l1d = 0, l2 = 0, l3 = 0; /* Cache sizes */

	cpuid(CPUID_BASE_FEATURES, (u32 *)&regs[0], (u32 *)&regs[1],
	      (u32 *)&regs[2], (u32 *)&regs[3]);
	n = regs[0] & 0xff;

	for (i = 0; i < n; i++) {
		cpuid(CPUID_BASE_FEATURES, (u32 *)&regs[0], (u32 *)&regs[1],
		      (u32 *)&regs[2], (u32*)&regs[3]);
		for (j = 0; j < 3; j++) {
			if (regs[j] < 0) regs[j] = 0;
		}

		/* byte 0 is level counter */
		for (j = 1; j < 16; j++) {
			unsigned char des = dp[j];
			unsigned char k = 0;

			/* look up this descriptor in the table */
			while (cache_table[k].descriptor != 0) {
				if (cache_table[k].descriptor == des) {
					switch (cache_table[k].cache_type) {
					case LVL_1_INST:
						l1i += cache_table[k].size;
						break;
					case LVL_1_DATA:
						l1d += cache_table[k].size;
						break;
					case LVL_2:
						l2 += cache_table[k].size;
						break;
					case LVL_3:
						l3 += cache_table[k].size;
						break;
					case LVL_TRACE:
						trace += cache_table[k].size;
						break;
					}

					break;
				}

				k++;
			}
		}
	}
	cpu_info->l2_cache_size = l2;
	cpu_info->l3_cache_size = l3;
	cpu_info->l1_icache_size = l1i;
	cpu_info->l1_dcache_size = l1d;
}

static inline void gather_intel_features(struct cpuinfo_x86 *cpu_info)
{
	u32 a, b, c, d;

	gather_cpu_brandinfo(cpu_info);
	gather_intel_cacheinfo(cpu_info);

	cpuid(CPUID_BASE_FEATURES, &a, &b, &c, &d);
	cpu_info->hw_virt_available = ((c >> 5) & 1);
}

void indentify_cpu(void)
{
	u32 tmp;

	cpuid(CPUID_BASE_VENDORSTRING, (u32 *)&tmp,
		(u32 *)&cpu_info.vendor_string[0],
		(u32 *)&cpu_info.vendor_string[8],
		(u32 *)&cpu_info.vendor_string[4]);

	if (!memcmp(cpu_info.vendor_string, "AuthenticAMD", PROCESSOR_VENDOR_ID_LEN))
		cpu_info.vendor = x86_VENDOR_AMD;
	else if (!memcmp(cpu_info.vendor_string, "GenuineIntel", PROCESSOR_VENDOR_ID_LEN))
		cpu_info.vendor = x86_VENDOR_INTEL;
	else {
		vmm_panic("Unknown Vendor: %s\n", cpu_info.vendor_string);
	}


	switch(cpu_info.vendor) {
	case x86_VENDOR_AMD:
		gather_amd_features(&cpu_info);
		break;

	case x86_VENDOR_INTEL:
		gather_intel_features(&cpu_info);
		break;
	}
}
