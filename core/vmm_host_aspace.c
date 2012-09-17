/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_host_aspace.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup patel (anup@brainfault.org)
 * @brief Source file for host virtual address space management.
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_host_ram.h>
#include <vmm_host_vapool.h>
#include <vmm_host_aspace.h>
#include <stringlib.h>
#include <arch_config.h>
#include <arch_sections.h>
#include <arch_cpu_aspace.h>
#include <arch_board.h>

#if !defined(ARCH_HAS_PHYSICAL_READ) || \
    !defined(ARCH_HAS_PHYSICAL_WRITE)
static virtual_addr_t phys_rw_va[CONFIG_CPU_COUNT];
#endif

virtual_addr_t vmm_host_memmap(physical_addr_t pa, 
			       virtual_size_t sz, 
			       u32 mem_flags)
{
	int rc, ite;
	virtual_addr_t va;
	physical_addr_t tpa;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);

	if ((rc = vmm_host_vapool_alloc(&va, sz, FALSE))) {
		/* Don't have space */
		BUG();
	}

	tpa = pa & ~VMM_PAGE_MASK;

	for (ite = 0; ite < (sz >> VMM_PAGE_SHIFT); ite++) {
		rc = arch_cpu_aspace_map(va + ite * VMM_PAGE_SIZE, 
					tpa + ite * VMM_PAGE_SIZE, 
					mem_flags);
		if (rc) {
			/* We were not able to map physical address */
			BUG();
		}
	}

	return va + (pa & VMM_PAGE_MASK);
}

int vmm_host_memunmap(virtual_addr_t va, virtual_size_t sz)
{
	int rc, ite;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);
	va &= ~VMM_PAGE_MASK;

	for (ite = 0; ite < (sz >> VMM_PAGE_SHIFT); ite++) {
		rc = arch_cpu_aspace_unmap(va + ite * VMM_PAGE_SIZE);
		if (rc) {
			return rc;
		}
	}

	if ((rc = vmm_host_vapool_free(va, sz))) {
		return rc;
	}

	return VMM_OK;
}

virtual_addr_t vmm_host_alloc_pages(u32 page_count, u32 mem_flags)
{
	int rc = VMM_OK;
	physical_addr_t pa = 0x0;

	rc = vmm_host_ram_alloc(&pa, page_count * VMM_PAGE_SIZE, FALSE);
	if (rc) {
		return 0x0;
	}

	return vmm_host_memmap(pa, page_count * VMM_PAGE_SIZE, mem_flags);
}

int vmm_host_free_pages(virtual_addr_t page_va, u32 page_count)
{
	int rc = VMM_OK;
	physical_addr_t pa = 0x0;

	page_va &= ~VMM_PAGE_MASK;

	if ((rc = arch_cpu_aspace_va2pa(page_va, &pa))) {
		return rc;
	}

	if ((rc = vmm_host_memunmap(page_va, page_count * VMM_PAGE_SIZE))) {
		return rc;
	}

	return vmm_host_ram_free(pa, page_count * VMM_PAGE_SIZE);
}

int vmm_host_page_va2pa(virtual_addr_t page_va, physical_addr_t * page_pa)
{
	int rc = VMM_OK;
	physical_addr_t pa = 0x0;

	page_va &= ~VMM_PAGE_MASK;

	if ((rc = arch_cpu_aspace_va2pa(page_va, &pa))) {
		return rc;
	}

	if (page_pa) {
		*page_pa = pa;
	}

	return VMM_OK;
}

u32 vmm_host_physical_read(physical_addr_t hphys_addr, 
			   void *dst, u32 len)
{
#if !defined(ARCH_HAS_PHYSICAL_READ)
	int rc;
	irq_flags_t flags;
	u32 bytes_read = 0, page_offset, page_read;
	virtual_addr_t src = phys_rw_va[vmm_smp_processor_id()];

	/* Read one page at time with irqs disabled since, we use
	 * one virtual address per-host CPU to do read/write.
	 */
	while (bytes_read < len) {
		page_offset = hphys_addr & VMM_PAGE_MASK;

		page_read = VMM_PAGE_SIZE - page_offset;
		page_read = (page_read < (len - bytes_read)) ? 
			     page_read : (len - bytes_read);

		flags = arch_cpu_irq_save();

		rc = arch_cpu_aspace_map(src, hphys_addr & ~VMM_PAGE_MASK, 
					 VMM_MEMORY_READABLE);
		if (rc) {
			break;
		}

		memcpy(dst, (void *)(src + page_offset), page_read);

		rc = arch_cpu_aspace_unmap(src);
		if (rc) {
			break;
		}

		arch_cpu_irq_restore(flags);

		hphys_addr += page_read;
		bytes_read += page_read;
		dst += page_read;
	}

	return bytes_read;
#else
	return arch_cpu_aspace_phys_read(hphys_addr, dst, len);
#endif
}

u32 vmm_host_physical_write(physical_addr_t hphys_addr, 
			    void *src, u32 len)
{
#if !defined(ARCH_HAS_PHYSICAL_WRITE)
	int rc;
	irq_flags_t flags;
	u32 bytes_written = 0, page_offset, page_write;
	virtual_addr_t dst = phys_rw_va[vmm_smp_processor_id()];

	/* Write one page at time with irqs disabled since, we use
	 * one virtual address per-host CPU to do read/write.
	 */
	while (bytes_written < len) {
		page_offset = hphys_addr & VMM_PAGE_MASK;

		page_write = VMM_PAGE_SIZE - page_offset;
		page_write = (page_write < (len - bytes_written)) ? 
			      page_write : (len - bytes_written);

		flags = arch_cpu_irq_save();

		rc = arch_cpu_aspace_map(dst, hphys_addr & ~VMM_PAGE_MASK, 
					 VMM_MEMORY_WRITEABLE);
		if (rc) {
			break;
		}

		memcpy((void *)(dst + page_offset), src, page_write);

		rc = arch_cpu_aspace_unmap(dst);
		if (rc) {
			break;
		}

		arch_cpu_irq_restore(flags);

		hphys_addr += page_write;
		bytes_written += page_write;
		src += page_write;
	}

	return bytes_written;
#else
	return arch_cpu_aspace_phys_write(hphys_addr, src, len);
#endif
}

u32 vmm_host_free_initmem(void)
{
	int rc;
	virtual_addr_t init_start;
	virtual_size_t init_size;

	init_start = arch_init_vaddr();
	init_size = arch_init_size();
	init_size = VMM_ROUNDUP2_PAGE_SIZE(init_size);

	if ((rc = vmm_host_free_pages(init_start, init_size >> VMM_PAGE_SHIFT))) {
		BUG();
	}

	return (init_size >> VMM_PAGE_SHIFT) * VMM_PAGE_SIZE / 1024;
}

int __init vmm_host_aspace_init(void)
{
#if !defined(ARCH_HAS_PHYSICAL_READ) || \
    !defined(ARCH_HAS_PHYSICAL_WRITE)
	int cpu;
#endif
	int rc;
	physical_addr_t ram_start, core_resv_pa = 0x0, arch_resv_pa = 0x0;
	physical_size_t ram_size;
	virtual_addr_t vapool_start, core_resv_va = 0x0, arch_resv_va = 0x0;
	virtual_size_t vapool_size, vapool_hksize, ram_hksize;
	virtual_size_t hk_total_size = 0x0;
	virtual_size_t core_resv_sz = 0x0, arch_resv_sz = 0x0;

	/* Determine VAPOOL start, size, and hksize */
	vapool_start = arch_code_vaddr_start();
	vapool_size = (CONFIG_VAPOOL_SIZE << 20);
	vapool_hksize = vmm_host_vapool_estimate_hksize(vapool_size);

	/* Determine RAM start, size an hksize */
	if ((rc = arch_board_ram_start(&ram_start))) {
		return rc;
	}
	if ((rc = arch_board_ram_size(&ram_size))) {
		return rc;
	}
	if (ram_start & VMM_PAGE_MASK) {
		ram_size -= VMM_PAGE_SIZE;
		ram_size += ram_start & VMM_PAGE_MASK;
		ram_start += VMM_PAGE_SIZE;
		ram_start -= ram_start & VMM_PAGE_MASK;
	}
	if (ram_size & VMM_PAGE_MASK) {
		ram_size -= ram_size & VMM_PAGE_MASK;
	}
	ram_hksize = vmm_host_ram_estimate_hksize(ram_size);

	/* Calculate physical address, virtual address, and size of 
	 * core reserved space for VAPOOL and RAM house-keeping
	 */
	hk_total_size = vapool_hksize + ram_hksize;
	hk_total_size = VMM_ROUNDUP2_PAGE_SIZE(hk_total_size);
	core_resv_pa = ram_start;
	core_resv_va = vapool_start + arch_code_size();
	core_resv_sz = hk_total_size;

	/* We cannot estimate the physical address, virtual address, and size 
	 * of arch reserved space so we set all of them to zero and expect that
	 * arch_cpu_aspace_init() will update them if arch code is going to use
	 * the arch reserved space
	 */
	arch_resv_pa = 0x0;
	arch_resv_va = 0x0;
	arch_resv_sz = 0x0;

	/* Call arch_cpu_aspace_init() will estimated parameters for core 
	 * reserved space and arch reserved space. The arch_cpu_aspace_init()
	 * can change these parameter as per needed.
	 */
	if ((rc = arch_cpu_aspace_init(&core_resv_pa, 
					&core_resv_va, 
					&core_resv_sz,
					&arch_resv_pa,
					&arch_resv_va,
					&arch_resv_sz))) {
		return rc;
	}
	if (core_resv_sz < hk_total_size) {
		return VMM_EFAIL;
	}
	if ((vapool_size <= core_resv_sz) || 
	    (ram_size <= core_resv_sz)) {
		return VMM_EFAIL;
	}

	/* Initialize VAPOOL managment */
	if ((rc = vmm_host_vapool_init(vapool_start, 
					vapool_size, 
					core_resv_va, 
					core_resv_va, 
					core_resv_sz))) {
		return rc;
	}

	/* Reserve pages used for code in VAPOOL */
	if ((rc = vmm_host_vapool_reserve(arch_code_vaddr_start(), 
					  arch_code_size()))) {
		return rc;
	}

	/* Reserve pages used for arch reserved space in VAPOOL */
	if ((0x0 < arch_resv_sz) &&
	    (rc = vmm_host_vapool_reserve(arch_resv_va, 
					   arch_resv_sz))) {
		return rc;
	}

	/* Initialize RAM managment */
	if ((rc = vmm_host_ram_init(ram_start, 
				    ram_size, 
				    core_resv_va + vapool_hksize, 
				    core_resv_pa, 
				    core_resv_sz))) {
		return rc;
	}

	/* Reserve pages used for code in RAM */
	if ((rc = vmm_host_ram_reserve(arch_code_paddr_start(), 
				       arch_code_size()))) {
		return rc;
	}

	/* Reserve pages used for arch reserved space in RAM */
	if ((0x0 < arch_resv_sz) &&
	    (rc = vmm_host_ram_reserve(arch_resv_pa, 
				       arch_resv_sz))) {
		return rc;
	}

#if !defined(ARCH_HAS_PHYSICAL_READ) || \
    !defined(ARCH_HAS_PHYSICAL_WRITE)
	for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
		rc = vmm_host_vapool_alloc(&phys_rw_va[cpu], 
					   VMM_PAGE_SIZE, FALSE);
		if (rc) {
			return rc;
		}
	}
#endif

	return VMM_OK;
}
