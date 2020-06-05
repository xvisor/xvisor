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
 * @file s2_page_rdwr.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief s2_page_rdwr test implementation
 *
 * This tests the handling of read-write pages in stage2 page table.
 */

#include <vmm_error.h>
#include <vmm_modules.h>

#undef DEBUG

#include "nested_mmu_test.h"

#define MODULE_DESC			"s2_page_rdwr test"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(WBOXTEST_IPRIORITY+1)
#define MODULE_INIT			s2_page_rdwr_init
#define MODULE_EXIT			s2_page_rdwr_exit

static int s2_page_rdwr_run(struct wboxtest *test,
				struct vmm_chardev *cdev,
				u32 test_hcpu)
{
	int rc = VMM_OK;
	struct mmu_pgtbl *s2_pgtbl;
	virtual_addr_t map_host_va;
	physical_addr_t map_host_pa;
	physical_addr_t map_guest_pa;
	physical_addr_t nomap_guest_pa;

	nested_mmu_test_alloc_pages(cdev, test, rc, fail,
		1, NESTED_MMU_TEST_RDWR_MEM_FLAGS, &map_host_va, &map_host_pa);

	nested_mmu_test_alloc_pgtbl(cdev, test, rc, fail_free_host_page,
		MMU_STAGE2, &s2_pgtbl);

	nested_mmu_test_find_free_addr(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, nested_mmu_test_best_min_addr(s2_pgtbl),
		VMM_PAGE_SHIFT, &map_guest_pa);

	nested_mmu_test_map_pgtbl(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, map_guest_pa, map_host_pa,
		VMM_PAGE_SIZE, NESTED_MMU_TEST_RDWR_REG_FLAGS);

	nested_mmu_test_find_free_addr(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, map_guest_pa + VMM_PAGE_SIZE,
		VMM_PAGE_SHIFT, &nomap_guest_pa);

#define chunk_start	0
#define chunk_end	(chunk_start + (VMM_PAGE_SIZE / 4))

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		map_guest_pa + chunk_start + sizeof(u8),
		MMU_TEST_WIDTH_8BIT,
		map_host_pa + chunk_start + sizeof(u8),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		nomap_guest_pa + chunk_start + sizeof(u8),
		MMU_TEST_WIDTH_8BIT,
		nomap_guest_pa + chunk_start + sizeof(u8),
		MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		map_guest_pa + chunk_end - sizeof(u8),
		MMU_TEST_WIDTH_8BIT | MMU_TEST_WRITE,
		map_host_pa + chunk_end - sizeof(u8),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		nomap_guest_pa + chunk_end - sizeof(u8),
		MMU_TEST_WIDTH_8BIT | MMU_TEST_WRITE,
		nomap_guest_pa + chunk_end - sizeof(u8),
		MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end

	nested_mmu_test_find_free_addr(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, nomap_guest_pa + VMM_PAGE_SIZE,
		VMM_PAGE_SHIFT, &nomap_guest_pa);

#define chunk_start	(1 * (VMM_PAGE_SIZE / 4))
#define chunk_end	(chunk_start + (VMM_PAGE_SIZE / 4))

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		map_guest_pa + chunk_start + sizeof(u16),
		MMU_TEST_WIDTH_16BIT,
		map_host_pa + chunk_start + sizeof(u16),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		nomap_guest_pa + chunk_start + sizeof(u16),
		MMU_TEST_WIDTH_16BIT,
		nomap_guest_pa + chunk_start + sizeof(u16),
		MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		map_guest_pa + chunk_end - sizeof(u16),
		MMU_TEST_WIDTH_16BIT | MMU_TEST_WRITE,
		map_host_pa + chunk_end - sizeof(u16),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		nomap_guest_pa + chunk_end - sizeof(u16),
		MMU_TEST_WIDTH_16BIT | MMU_TEST_WRITE,
		nomap_guest_pa + chunk_end - sizeof(u16),
		MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end

	nested_mmu_test_find_free_addr(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, nomap_guest_pa + VMM_PAGE_SIZE,
		VMM_PAGE_SHIFT, &nomap_guest_pa);

#define chunk_start	(2 * (VMM_PAGE_SIZE / 4))
#define chunk_end	(chunk_start + (VMM_PAGE_SIZE / 4))

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		map_guest_pa + chunk_start + sizeof(u32),
		MMU_TEST_WIDTH_32BIT,
		map_host_pa + chunk_start + sizeof(u32),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		nomap_guest_pa + chunk_start + sizeof(u32),
		MMU_TEST_WIDTH_32BIT,
		nomap_guest_pa + chunk_start + sizeof(u32),
		MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		map_guest_pa + chunk_end - sizeof(u32),
		MMU_TEST_WIDTH_32BIT | MMU_TEST_WRITE,
		map_host_pa + chunk_end - sizeof(u32),
		0);

	nested_mmu_test_execute(cdev, test, rc, fail_free_s2_pgtbl,
		s2_pgtbl, NULL,
		nomap_guest_pa + chunk_end - sizeof(u32),
		MMU_TEST_WIDTH_32BIT | MMU_TEST_WRITE,
		nomap_guest_pa + chunk_end - sizeof(u32),
		MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end

fail_free_s2_pgtbl:
	nested_mmu_test_free_pgtbl(cdev, test, s2_pgtbl);
fail_free_host_page:
	nested_mmu_test_free_pages(cdev, test, &map_host_va, &map_host_pa, 1);
fail:
	return rc;
}

static struct wboxtest s2_page_rdwr = {
	.name = "s2_page_rdwr",
	.run = s2_page_rdwr_run,
};

static int __init s2_page_rdwr_init(void)
{
	return wboxtest_register("nested_mmu", &s2_page_rdwr);
}

static void __exit s2_page_rdwr_exit(void)
{
	wboxtest_unregister(&s2_page_rdwr);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
