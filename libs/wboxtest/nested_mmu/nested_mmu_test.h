/**
 * Copyright (c) 2020 Anup Patel.
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
 * @file nested_mmu_test.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Nested MMU test helper routines and macros
 */

#ifndef __NESTED_MMU_TEST__
#define __NESTED_MMU_TEST__

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_limits.h>
#include <vmm_guest_aspace.h>
#include <vmm_host_aspace.h>
#include <vmm_host_ram.h>
#include <libs/wboxtest.h>
#include <generic_mmu.h>

#ifdef DEBUG
#define DPRINTF(__cdev, __msg...)	vmm_cprintf(__cdev, __msg)
#else
#define DPRINTF(__cdev, __msg...)
#endif

#define NESTED_MMU_TEST_RDWR_MEM_FLAGS	VMM_MEMORY_FLAGS_NORMAL

#define NESTED_MMU_TEST_RDONLY_MEM_FLAGS	(VMM_MEMORY_FLAGS_NORMAL_WT & \
						 ~VMM_MEMORY_WRITEABLE)

#define NESTED_MMU_TEST_NORDWR_MEM_FLAGS	(VMM_MEMORY_FLAGS_IO & \
						 ~(VMM_MEMORY_READABLE | \
						   VMM_MEMORY_WRITEABLE))

#define NESTED_MMU_TEST_RDWR_REG_FLAGS	(VMM_REGION_REAL | \
						 VMM_REGION_MEMORY | \
						 VMM_REGION_CACHEABLE | \
						 VMM_REGION_BUFFERABLE | \
						 VMM_REGION_ISRAM)

#define NESTED_MMU_TEST_RDONLY_REG_FLAGS	(VMM_REGION_REAL | \
						 VMM_REGION_MEMORY | \
						 VMM_REGION_CACHEABLE | \
						 VMM_REGION_READONLY | \
						 VMM_REGION_ISROM)

#define NESTED_MMU_TEST_NORDWR_REG_FLAGS	(VMM_REGION_VIRTUAL | \
						 VMM_REGION_MEMORY | \
						 VMM_REGION_ISDEVICE)

#define nested_mmu_test_best_min_addr(__pgtbl) \
	((mmu_pgtbl_stage(__pgtbl) == MMU_STAGE2) ? \
	 vmm_host_ram_end() : (mmu_pgtbl_map_addr_end(__pgtbl) / 4))

#define nested_mmu_test_alloc_pages(__cdev, __test, __rc, __fail_label, \
				    __page_count, __mem_flags, \
				    __output_va_ptr, __output_pa_ptr) \
do { \
	DPRINTF((__cdev), "%s: Allocating %d Host pages ", \
		(__test)->name, (__page_count)); \
	*(__output_va_ptr) = vmm_host_alloc_pages((__page_count), \
		(__mem_flags)); \
	(__rc) = vmm_host_va2pa(*(__output_va_ptr), (__output_pa_ptr)); \
	DPRINTF((__cdev), "(error %d)%s", (__rc), (__rc) ? "\n" : " "); \
	if (__rc) { \
		goto __fail_label; \
	} \
	DPRINTF((__cdev), "(0x%"PRIPADDR")\n", *(__output_pa_ptr)); \
} while (0) \

#define nested_mmu_test_free_pages(__cdev, __test, \
				   __va_ptr, __pa_ptr, __page_count) \
do { \
	DPRINTF((__cdev), "%s: Freeing %d Host pages (0x%"PRIPADDR")\n", \
		(__test)->name, (__page_count), *(__pa_ptr)); \
	vmm_host_free_pages(*(__va_ptr), (__page_count)); \
} while (0)

#define nested_mmu_test_alloc_hugepages(__cdev, __test, __rc, __fail_label, \
					__page_count, __mem_flags, \
					__output_va_ptr, __output_pa_ptr) \
do { \
	DPRINTF((__cdev), "%s: Allocating %d Host hugepages ", \
		(__test)->name, (__page_count)); \
	*(__output_va_ptr) = vmm_host_alloc_hugepages((__page_count), \
		(__mem_flags)); \
	(__rc) = vmm_host_va2pa(*(__output_va_ptr), (__output_pa_ptr)); \
	DPRINTF((__cdev), "(error %d)%s", (__rc), (__rc) ? "\n" : " "); \
	if (__rc) { \
		goto __fail_label; \
	} \
	DPRINTF((__cdev), "(0x%"PRIPADDR")\n", *(__output_pa_ptr)); \
} while (0) \

#define nested_mmu_test_free_hugepages(__cdev, __test, \
					__va_ptr, __pa_ptr, __page_count) \
do { \
	DPRINTF((__cdev), "%s: Freeing %d Host hugepages (0x%"PRIPADDR")\n", \
		(__test)->name, (__page_count), *(__pa_ptr)); \
	vmm_host_free_hugepages(*(__va_ptr), (__page_count)); \
} while (0)

#define nested_mmu_test_alloc_pgtbl(__cdev, __test, __rc, __fail_label, \
				    __stage, __output_pgtbl_double_ptr) \
do { \
	DPRINTF((__cdev), "%s: Allocating Stage%s page table ", \
		(__test)->name, \
		((__stage) == MMU_STAGE2) ? "2" : "1"); \
	*(__output_pgtbl_double_ptr) = mmu_pgtbl_alloc((__stage), -1, \
					MMU_ATTR_REMOTE_TLB_FLUSH, 0); \
	DPRINTF((__cdev), "%s", \
		(!*(__output_pgtbl_double_ptr)) ? "(failed)\n" : ""); \
	if (!*(__output_pgtbl_double_ptr)) { \
		(__rc) = VMM_ENOMEM; \
		goto __fail_label; \
	} \
	DPRINTF((__cdev), "(0x%"PRIPADDR")\n", \
		mmu_pgtbl_physical_addr(*(__output_pgtbl_double_ptr))); \
} while (0)

#define nested_mmu_test_free_pgtbl(__cdev, __test, __pgtbl) \
do { \
	DPRINTF((__cdev), "%s: Freeing Stage%s page table (0x%"PRIPADDR")\n", \
		(__test)->name, \
		(mmu_pgtbl_stage(__pgtbl) == MMU_STAGE2) ? "2" : "1", \
		mmu_pgtbl_physical_addr(__pgtbl)); \
	mmu_pgtbl_free(__pgtbl); \
} while (0)

#define nested_mmu_test_find_free_addr(__cdev, __test, __rc, __fail_label, \
				   __pgtbl, __min_addr, __page_order, \
				   __output_addr_ptr) \
do { \
	DPRINTF((__cdev), "%s: Finding free Guest %s ", \
		(__test)->name, \
		(mmu_pgtbl_stage(__pgtbl) == MMU_STAGE2) ? "Phys" : "Virt"); \
	(__rc) = mmu_find_free_address((__pgtbl), (__min_addr), \
				   (__page_order), (__output_addr_ptr)); \
	DPRINTF((__cdev), "(error %d)%s", (__rc), (__rc) ? "\n" : " "); \
	if (__rc) { \
		goto __fail_label; \
	} \
	DPRINTF((__cdev), "(0x%"PRIPADDR")\n", *(__output_addr_ptr)); \
} while (0)

#define nested_mmu_test_map_pgtbl(__cdev, __test, __rc, __fail_label, \
				   __pgtbl, __guest_phys, __host_phys, \
				   __guest_size, __mem_or_reg_flags) \
do { \
	struct mmu_page __pg; \
	__pg.ia = (__guest_phys); \
	__pg.oa = (__host_phys); \
	__pg.sz = (__guest_size); \
	arch_mmu_pgflags_set(&__pg.flags, \
		mmu_pgtbl_stage(__pgtbl), (__mem_or_reg_flags)); \
	DPRINTF(cdev, "%s: Mapping Stage%s Guest %s 0x%"PRIPADDR" => " \
		"%s Phys 0x%"PRIPADDR" (%ld KB)\n", (__test)->name, \
		(mmu_pgtbl_stage(__pgtbl) == MMU_STAGE2) ? "2" : "1", \
		(mmu_pgtbl_stage(__pgtbl) == MMU_STAGE2) ? "Phys" : "Virt", \
		__pg.ia, \
		(mmu_pgtbl_stage(__pgtbl) == MMU_STAGE2) ? "Host" : "Guest", \
		__pg.oa, \
		__pg.sz / SZ_1K); \
	(__rc) = mmu_map_page((__pgtbl), &__pg); \
	if (__rc) { \
		goto __fail_label; \
	} \
} while (0)

#define nested_mmu_test_idmap_stage1(__cdev, __test, __rc, __fail_label, \
				     __s2_pgtbl, __s1_pgtbl, __map_size, \
				     __reg_flags) \
do { \
	DPRINTF(cdev, "%s: Identity map Stage1 page table (0x%"PRIPADDR") " \
		"in Stage2 page table (0x%"PRIPADDR") ", \
		(__test)->name, \
		mmu_pgtbl_physical_addr(__s1_pgtbl), \
		mmu_pgtbl_physical_addr(__s2_pgtbl)); \
	(__rc) = mmu_idmap_nested_pgtbl((__s2_pgtbl), (__s1_pgtbl), \
		(__map_size), (__reg_flags)); \
	DPRINTF((__cdev), "(error %d)\n", (__rc)); \
	if (__rc) { \
		goto __fail_label; \
	} \
} while (0)

#define nested_mmu_test_execute(__cdev, __test, __rc, __fail_label, \
				__s2_pgtbl, __s1_pgtbl, \
				__va, __flags, __exp_addr, __exp_fault) \
do { \
	DPRINTF((__cdev), "%s: Checking %s%s%s%s at Guest Virt 0x%lx ", \
		(__test)->name, \
		((__flags) & MMU_TEST_WRITE) ? "write" : "read", \
		((__flags) & MMU_TEST_WIDTH_8BIT) ? "8" : "", \
		((__flags) & MMU_TEST_WIDTH_16BIT) ? "16" : "", \
		((__flags) & MMU_TEST_WIDTH_32BIT) ? "32" : "", \
		__va); \
	(__rc) = mmu_test_nested_pgtbl((__s2_pgtbl), (__s1_pgtbl), (__flags), \
		(unsigned long)(__va), (__exp_addr), (__exp_fault)); \
	DPRINTF((__cdev), "(error %d)\n", (__rc)); \
	if (__rc) { \
		goto __fail_label; \
	} \
} while (0)

#endif
