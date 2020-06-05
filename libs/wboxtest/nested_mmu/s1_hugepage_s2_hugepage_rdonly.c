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
 * @file s1_hugepage_s2_hugepage_rdonly.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief s1_hugepage_s2_hugepage_rdonly test implementation
 *
 * This tests the handling of read-only hugepages in stage1 and
 * stage2 page tables.
 */

#include <vmm_error.h>
#include <vmm_modules.h>

#undef DEBUG

#include "nested_mmu_test.h"

#define MODULE_DESC			"s1_hugepage_s2_hugepage_rdonly test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			s1_hugepage_s2_hugepage_rdonly_init
#define MODULE_EXIT			s1_hugepage_s2_hugepage_rdonly_exit

static int s1_hugepage_s2_hugepage_rdonly_run(struct wboxtest *test,
				struct vmm_chardev *cdev,
				u32 test_hcpu)
{
	int rc = VMM_OK;
	struct mmu_pgtbl *s1_pgtbl;
	struct mmu_pgtbl *s2_pgtbl;
	virtual_addr_t map_host_va;
	physical_addr_t map_host_pa;
	physical_addr_t map_rdwr_s1_host_pa;
	physical_addr_t map_rdwr_s2_host_pa;
	physical_addr_t map_guest_va;
	physical_addr_t map_guest_pa;
	physical_addr_t map_rdwr_s1_guest_va;
	physical_addr_t map_rdwr_s1_guest_pa;
	physical_addr_t map_rdwr_s2_guest_va;
	physical_addr_t map_rdwr_s2_guest_pa;

	nested_mmu_test_alloc_hugepages(cdev, test, rc, fail,
		2, NESTED_MMU_TEST_RDWR_MEM_FLAGS, &map_host_va, &map_host_pa);

	map_rdwr_s1_host_pa = map_host_pa;
	map_rdwr_s2_host_pa = map_host_pa + vmm_host_hugepage_size();

	nested_mmu_test_alloc_pgtbl(cdev, test, rc, fail_free_host_hugepage,
		MMU_STAGE1, &s1_pgtbl);

	nested_mmu_test_alloc_pgtbl(cdev, test, rc, fail_free_s1_pgtbl,
		MMU_STAGE2, &s2_pgtbl);

	nested_mmu_test_find_free_addr(cdev, test, rc, fail_free_s2_pgtbl,
		s1_pgtbl, nested_mmu_test_best_min_addr(s1_pgtbl),
		vmm_host_hugepage_shift(), &map_guest_va);

	nested_mmu_test_find_free_addr(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, nested_mmu_test_best_min_addr(s2_pgtbl),
		vmm_host_hugepage_shift(), &map_guest_pa);

	map_rdwr_s1_guest_va = map_guest_va + vmm_host_hugepage_size();
	map_rdwr_s1_guest_pa = map_guest_pa + vmm_host_hugepage_size();
	map_rdwr_s2_guest_va = map_guest_va + (2 * vmm_host_hugepage_size());
	map_rdwr_s2_guest_pa = map_guest_pa + (2* vmm_host_hugepage_size());

	nested_mmu_test_map_pgtbl(cdev, test, rc, fail_free_s2_pgtbl,
		s1_pgtbl, map_guest_va, map_guest_pa,
		vmm_host_hugepage_size(), NESTED_MMU_TEST_RDONLY_MEM_FLAGS);

	nested_mmu_test_map_pgtbl(cdev, test, rc, fail_free_s2_pgtbl,
		s1_pgtbl, map_rdwr_s1_guest_va, map_rdwr_s1_guest_pa,
		vmm_host_hugepage_size(), NESTED_MMU_TEST_RDWR_MEM_FLAGS);

	nested_mmu_test_map_pgtbl(cdev, test, rc, fail_free_s2_pgtbl,
		s1_pgtbl, map_rdwr_s2_guest_va, map_rdwr_s2_guest_pa,
		vmm_host_hugepage_size(), NESTED_MMU_TEST_RDONLY_MEM_FLAGS);

	nested_mmu_test_idmap_stage1(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl, vmm_host_hugepage_size(),
		NESTED_MMU_TEST_RDWR_REG_FLAGS);

	nested_mmu_test_map_pgtbl(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, map_guest_pa, map_host_pa,
		vmm_host_hugepage_size(), NESTED_MMU_TEST_RDONLY_REG_FLAGS);

	nested_mmu_test_map_pgtbl(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, map_rdwr_s1_guest_pa, map_rdwr_s1_host_pa,
		vmm_host_hugepage_size(), NESTED_MMU_TEST_RDONLY_REG_FLAGS);

	nested_mmu_test_map_pgtbl(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, map_rdwr_s2_guest_pa, map_rdwr_s2_host_pa,
		vmm_host_hugepage_size(), NESTED_MMU_TEST_RDWR_REG_FLAGS);

#define chunk_start	0
#define chunk_end	(chunk_start + (vmm_host_hugepage_size() / 4))
#define chunk_mid	(chunk_start + ((chunk_end - chunk_start) / 2))

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_guest_va + chunk_start + sizeof(u8),
		MMU_TEST_WIDTH_8BIT,
		map_host_pa + chunk_start + sizeof(u8),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_guest_va + chunk_start + sizeof(u8),
		MMU_TEST_WIDTH_8BIT | MMU_TEST_WRITE,
		map_guest_va + chunk_start + sizeof(u8),
		MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_WRITE);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s1_guest_va + chunk_mid + sizeof(u8),
		MMU_TEST_WIDTH_8BIT,
		map_rdwr_s1_host_pa + chunk_mid + sizeof(u8),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s1_guest_va + chunk_mid + sizeof(u8),
		MMU_TEST_WIDTH_8BIT | MMU_TEST_WRITE,
		map_rdwr_s1_guest_pa + chunk_mid + sizeof(u8),
		MMU_TEST_FAULT_WRITE);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s2_guest_va + chunk_end - sizeof(u8),
		MMU_TEST_WIDTH_8BIT,
		map_rdwr_s2_host_pa + chunk_end - sizeof(u8),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s2_guest_va + chunk_end - sizeof(u8),
		MMU_TEST_WIDTH_8BIT | MMU_TEST_WRITE,
		map_rdwr_s2_guest_va + chunk_end - sizeof(u8),
		MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end
#undef chunk_mid

#define chunk_start	(1 * (vmm_host_hugepage_size() / 4))
#define chunk_end	(chunk_start + (vmm_host_hugepage_size() / 4))
#define chunk_mid	(chunk_start + ((chunk_end - chunk_start) / 2))

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_guest_va + chunk_start + sizeof(u16),
		MMU_TEST_WIDTH_16BIT,
		map_host_pa + chunk_start + sizeof(u16),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_guest_va + chunk_start + sizeof(u16),
		MMU_TEST_WIDTH_16BIT | MMU_TEST_WRITE,
		map_guest_va + chunk_start + sizeof(u16),
		MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_WRITE);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s1_guest_va + chunk_mid + sizeof(u16),
		MMU_TEST_WIDTH_16BIT,
		map_rdwr_s1_host_pa + chunk_mid + sizeof(u16),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s1_guest_va + chunk_mid + sizeof(u16),
		MMU_TEST_WIDTH_16BIT | MMU_TEST_WRITE,
		map_rdwr_s1_guest_pa + chunk_mid + sizeof(u16),
		MMU_TEST_FAULT_WRITE);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s2_guest_va + chunk_end - sizeof(u16),
		MMU_TEST_WIDTH_16BIT,
		map_rdwr_s2_host_pa + chunk_end - sizeof(u16),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s2_guest_va + chunk_end - sizeof(u16),
		MMU_TEST_WIDTH_16BIT | MMU_TEST_WRITE,
		map_rdwr_s2_guest_va + chunk_end - sizeof(u16),
		MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end
#undef chunk_mid

#define chunk_start	(2 * (vmm_host_hugepage_size() / 4))
#define chunk_end	(chunk_start + (vmm_host_hugepage_size() / 4))
#define chunk_mid	(chunk_start + ((chunk_end - chunk_start) / 2))

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_guest_va + chunk_start + sizeof(u32),
		MMU_TEST_WIDTH_32BIT,
		map_host_pa + chunk_start + sizeof(u32),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_guest_va + chunk_start + sizeof(u32),
		MMU_TEST_WIDTH_32BIT | MMU_TEST_WRITE,
		map_guest_va + chunk_start + sizeof(u32),
		MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_WRITE);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s1_guest_va + chunk_mid + sizeof(u32),
		MMU_TEST_WIDTH_32BIT,
		map_rdwr_s1_host_pa + chunk_mid + sizeof(u32),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s1_guest_va + chunk_mid + sizeof(u32),
		MMU_TEST_WIDTH_32BIT | MMU_TEST_WRITE,
		map_rdwr_s1_guest_pa + chunk_mid + sizeof(u32),
		MMU_TEST_FAULT_WRITE);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s2_guest_va + chunk_end - sizeof(u32),
		MMU_TEST_WIDTH_32BIT,
		map_rdwr_s2_host_pa + chunk_end - sizeof(u32),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, s1_pgtbl,
		map_rdwr_s2_guest_va + chunk_end - sizeof(u32),
		MMU_TEST_WIDTH_32BIT | MMU_TEST_WRITE,
		map_rdwr_s2_guest_va + chunk_end - sizeof(u32),
		MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end
#undef chunk_mid

fail_free_s2_pgtbl:
	nested_mmu_test_free_pgtbl(cdev, test, s2_pgtbl);
fail_free_s1_pgtbl:
	nested_mmu_test_free_pgtbl(cdev, test, s1_pgtbl);
fail_free_host_hugepage:
	nested_mmu_test_free_hugepages(cdev, test,
				       &map_host_va, &map_host_pa, 2);
fail:
	return rc;
}

static struct wboxtest s1_hugepage_s2_hugepage_rdonly = {
	.name = "s1_hugepage_s2_hugepage_rdonly",
	.run = s1_hugepage_s2_hugepage_rdonly_run,
};

static int __init s1_hugepage_s2_hugepage_rdonly_init(void)
{
	return wboxtest_register("nested_mmu", &s1_hugepage_s2_hugepage_rdonly);
}

static void __exit s1_hugepage_s2_hugepage_rdonly_exit(void)
{
	wboxtest_unregister(&s1_hugepage_s2_hugepage_rdonly);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
