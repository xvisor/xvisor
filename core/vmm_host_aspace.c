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
#include <arch_config.h>
#include <arch_sections.h>
#include <arch_cpu_aspace.h>
#include <arch_devtree.h>
#include <libs/stringlib.h>
#include <libs/rbtree_augmented.h>

static virtual_addr_t host_mem_rw_va[CONFIG_CPU_COUNT];

struct host_mhash_entry {
	struct rb_node rb;
	physical_addr_t pa;
	virtual_addr_t va;
	virtual_size_t sz;
	u32 mem_flags;
	u32 ref_count;
};

struct host_mhash_ctrl {
	vmm_rwlock_t lock;
	virtual_addr_t start;
	virtual_size_t size;
	u32 count;
	struct rb_root root;
	struct host_mhash_entry *entry;
};

static struct host_mhash_ctrl host_mhash;

/* NOTE: Must be called with write lock held on host_mhash.lock */
static struct host_mhash_entry *__host_mhash_alloc(void)
{
	u32 i;
	struct host_mhash_entry *e = NULL;

	for (i = 0; i < host_mhash.count; i++) {
		if (!host_mhash.entry[i].ref_count) {
			e = &host_mhash.entry[i];
			e->ref_count = 1;
			break;
		}
	}

	return e;
}

/* NOTE: Must be called with read/write lock held on host_mhash.lock */
static struct host_mhash_entry *__host_mhash_find(physical_addr_t pa)
{
	struct rb_node *n;
	struct host_mhash_entry *ret = NULL;

	n = host_mhash.root.rb_node;
	while (n) {
		struct host_mhash_entry *e =
				rb_entry(n, struct host_mhash_entry, rb);

		if ((e->pa <= pa) && (pa < (e->pa + e->sz))) {
			ret = e;
			break;
		} else if (pa < e->pa) {
			n = n->rb_left;
		} else if ((e->pa + e->sz) <= pa) {
			n = n->rb_right;
		} else {
			vmm_panic("%s: can't find physical address\n", __func__);
		}
	}

	return ret;
}

static int host_mhash_add(physical_addr_t pa,
			  virtual_addr_t va,
			  virtual_size_t sz,
			  u32 mem_flags)
{
	int rc = VMM_OK;
	irq_flags_t flags;
	struct rb_node **new = NULL, *parent = NULL;
	struct host_mhash_entry *e, *parent_e;

	vmm_write_lock_irqsave(&host_mhash.lock, flags);

	e = __host_mhash_find(pa);
	if (e) {
		if ((va < e->va) ||
		    ((e->va + e->sz) <= va) ||
		    ((va + sz) < e->va) ||
		    ((e->va + e->sz) < (va + sz)) ||
		    ((e->pa + e->sz) < (pa + sz)) ||
		    (e->mem_flags != mem_flags)) {
			rc = VMM_EINVALID;
			goto done;
		}

		e->ref_count++;
	} else {
		e = __host_mhash_alloc();
		if (!e) {
			rc = VMM_ENOMEM;
			goto done;
		}
		e->pa = pa;
		e->va = va;
		e->sz = sz;
		e->mem_flags = mem_flags;

		new = &(host_mhash.root.rb_node);
		while (*new) {
			parent = *new;
			parent_e = rb_entry(parent, struct host_mhash_entry, rb);
			if ((e->pa + e->sz) <= parent_e->pa) {
				new = &parent->rb_left;
			} else if ((parent_e->pa + parent_e->sz) <= e->pa) {
				new = &parent->rb_right;
			} else {
				vmm_panic("%s: can't add entry\n", __func__);
			}
		}

		rb_link_node(&e->rb, parent, new);
		rb_insert_color(&e->rb, &host_mhash.root);
	}

done:
	vmm_write_unlock_irqrestore(&host_mhash.lock, flags);

	return rc;
}

static int host_mhash_del(physical_addr_t pa,
			  virtual_addr_t va,
			  virtual_size_t sz)
{
	int rc = VMM_OK;
	u32 rflags = 0;
	physical_addr_t rpa[2] = { 0, 0 };
	virtual_addr_t rva[2] = { 0, 0 };
	virtual_size_t rsz[2] = { 0, 0 };
	irq_flags_t flags;
	struct host_mhash_entry *e;

	vmm_write_lock_irqsave(&host_mhash.lock, flags);

	e = __host_mhash_find(pa);
	if (!e) {
		rc = VMM_ENOTAVAIL;
		goto done;
	}

	if ((va < e->va) ||
	    ((e->va + e->sz) <= va) ||
	    ((va + sz) < e->va) ||
	    ((e->va + e->sz) < (va + sz)) ||
	    ((e->pa + e->sz) < (pa + sz))) {
		rc = VMM_EINVALID;
		goto done;
	}

	e->ref_count--;
	if (e->ref_count) {
		rc = VMM_EBUSY;
		goto done;
	}

	rb_erase(&e->rb, &host_mhash.root);

	rpa[0] = e->pa;
	rva[0] = e->va;
	rsz[0] = va - e->va;
	rpa[1] = pa + sz;
	rva[1] = va + sz;
	rsz[1] = (e->va + e->sz) - (va + sz);
	rflags = e->mem_flags;

	memset(e, 0, sizeof(*e));
	RB_CLEAR_NODE(&e->rb);

done:
	vmm_write_unlock_irqrestore(&host_mhash.lock, flags);

	if (rsz[0]) {
		if ((rc = host_mhash_add(rpa[0], rva[0], rsz[0], rflags))) {
			vmm_panic("%s: can't add left residue error=%d\n",
				  __func__, rc);
		}
	}

	if (rsz[1]) {
		if ((rc = host_mhash_add(rpa[1], rva[1], rsz[1], rflags))) {
			vmm_panic("%s: can't add right residue error=%d\n",
				  __func__, rc);
		}
	}

	return rc;
}

static int host_mhash_pa2va(physical_addr_t pa,
			    virtual_addr_t *va,
			    virtual_size_t *sz,
			    u32 *mem_flags)
{
	int rc = VMM_ENOTAVAIL;
	irq_flags_t flags;
	struct host_mhash_entry *e;

	vmm_read_lock_irqsave(&host_mhash.lock, flags);

	e = __host_mhash_find(pa);
	if (e) {
		if (va) {
			*va = e->va + (e->pa - pa);
		}
		if (sz) {
			*sz = e->sz - (e->pa - pa);
		}
		if (mem_flags) {
			*mem_flags = e->mem_flags;
		}
		rc = VMM_OK;
	}

	vmm_read_unlock_irqrestore(&host_mhash.lock, flags);

	return rc;
}

static virtual_size_t host_mhash_estimate_hksize(void)
{
	return sizeof(struct host_mhash_entry) * CONFIG_MEMMAP_HASH_SIZE;
}

static int host_mhash_init(virtual_addr_t mhash_start,
			   virtual_size_t mhash_size)
{
	u32 i;
	struct host_mhash_entry *e;

	INIT_RW_LOCK(&host_mhash.lock);
	host_mhash.start = mhash_start;
	host_mhash.size = mhash_size;
	host_mhash.count = mhash_size / sizeof(struct host_mhash_entry);
	host_mhash.root = RB_ROOT;
	host_mhash.entry = (struct host_mhash_entry *)host_mhash.start;

	if (!host_mhash.count) {
		return VMM_EINVALID;
	}

	for (i = 0; i < host_mhash.count; i++) {
		e = &host_mhash.entry[i];
		memset(e, 0, sizeof(*e));
		RB_CLEAR_NODE(&e->rb);
	}

	return VMM_OK;
}

static virtual_addr_t host_memmap(physical_addr_t pa,
				  virtual_size_t sz,
				  u32 mem_flags)
{
	int rc, ite;
	virtual_addr_t va = 0;
	virtual_addr_t tsz = 0;
	physical_addr_t tpa = 0;
	u32 tmem_flags = 0;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);
	tpa = pa & ~VMM_PAGE_MASK;

	rc = host_mhash_pa2va(tpa, &va, &tsz, &tmem_flags);
	if (rc == VMM_OK) {
		if (mem_flags != tmem_flags) {
			/* Trying to map same physical address with
			 * different memory attributes.
			 */
			vmm_panic("%s: mem_flags mismatch\n", __func__);
		}
		if (tsz < sz) {
			/* Trying to map same physical address with
			 * greater size than already mapped.
			 */
			vmm_panic("%s: size mismatch\n", __func__);
		}

		va = va & ~VMM_PAGE_MASK;
	} else if (rc != VMM_ENOTAVAIL) {
		/* Something went wrong. */
		vmm_panic("%s: unhandled error=%d\n", __func__, rc);
	} else {
		if ((rc = vmm_host_vapool_alloc(&va, sz))) {
			/* Don't have space */
			vmm_panic("%s: vapool alloc failed error=%d\n",
				  __func__, rc);
		}

		for (ite = 0; ite < (sz >> VMM_PAGE_SHIFT); ite++) {
			rc = arch_cpu_aspace_map(va + ite * VMM_PAGE_SIZE,
						tpa + ite * VMM_PAGE_SIZE,
						mem_flags);
			if (rc) {
				/* We were not able to map physical address */
				vmm_panic("%s: failed to create VA->PA "
					  "mapping error=%d\n",
					  __func__, rc);
			}
		}
	}

	if ((rc = host_mhash_add(tpa, va, sz, mem_flags))) {
		/* Failed to update MEMMAP HASH */
		vmm_panic("%s: failed to add memmap hash entry error=%d\n",
			  __func__, rc);
	}

	return va + (pa & VMM_PAGE_MASK);
}

static int host_memunmap(virtual_addr_t va, virtual_size_t sz)
{
	int rc, ite;
	physical_addr_t pa = 0x0;

	sz = VMM_ROUNDUP2_PAGE_SIZE(sz);
	va &= ~VMM_PAGE_MASK;

	if ((rc = arch_cpu_aspace_va2pa(va, &pa))) {
		return rc;
	}

	rc = host_mhash_del(pa, va, sz);
	if (rc == VMM_EBUSY) {
		return VMM_OK;
	} else if (rc != VMM_OK) {
		vmm_panic("%s: unhandled error=%d\n", __func__, rc);
	}

	for (ite = 0; ite < (sz >> VMM_PAGE_SHIFT); ite++) {
		rc = arch_cpu_aspace_unmap(va + ite * VMM_PAGE_SIZE);
		if (rc) {
			return rc;
		}
	}

	if ((rc = vmm_host_vapool_free(va, sz))) {
		vmm_panic("%s: failed to free virtual address error=%d\n",
			  __func__, rc);
	}

	return VMM_OK;
}

virtual_addr_t vmm_host_memmap(physical_addr_t pa,
			       virtual_size_t sz,
			       u32 mem_flags)
{
	return host_memmap(pa, sz, mem_flags);
}

int vmm_host_memunmap(virtual_addr_t va)
{
	int rc;
	virtual_addr_t alloc_va;
	virtual_size_t alloc_sz;

	rc = vmm_host_vapool_find(va, &alloc_va, &alloc_sz);
	if (rc) {
		return rc;
	}

	return host_memunmap(alloc_va, alloc_sz);
}

virtual_addr_t vmm_host_alloc_pages(u32 page_count, u32 mem_flags)
{
	physical_addr_t pa = 0x0;

	if (!vmm_host_ram_alloc(&pa,
				page_count * VMM_PAGE_SIZE,
				VMM_PAGE_SHIFT)) {
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

	if ((rc = host_memunmap(page_va, page_count * VMM_PAGE_SIZE))) {
		return rc;
	}

	return vmm_host_ram_free(pa, page_count * VMM_PAGE_SIZE);
}

int vmm_host_va2pa(virtual_addr_t va, physical_addr_t *pa)
{
	int rc = VMM_OK;
	physical_addr_t _pa = 0x0;

	if ((rc = arch_cpu_aspace_va2pa(va & ~VMM_PAGE_MASK, &_pa))) {
		return rc;
	}

	if (pa) {
		*pa = _pa | (va & VMM_PAGE_MASK);
	}

	return VMM_OK;
}

int vmm_host_pa2va(physical_addr_t pa, virtual_addr_t *va)
{
	int rc = VMM_OK;
	virtual_addr_t _va = 0x0;

	rc = host_mhash_pa2va(pa & ~VMM_PAGE_MASK, &_va, NULL, NULL);
	if (rc) {
		return rc;
	}

	if (va) {
		*va = _va | (pa & VMM_PAGE_MASK);
	}

	return VMM_OK;
}

u32 vmm_host_memory_read(physical_addr_t hpa,
			 void *dst, u32 len, bool cacheable)
{
	int rc;
	irq_flags_t flags;
	u32 bytes_read = 0, page_offset, page_read;
	virtual_addr_t tmp_va = host_mem_rw_va[vmm_smp_processor_id()];

	/* Read one page at time with irqs disabled since, we use
	 * one virtual address per-host CPU to do read/write.
	 */
	while (bytes_read < len) {
		page_offset = hpa & VMM_PAGE_MASK;

		page_read = VMM_PAGE_SIZE - page_offset;
		page_read = (page_read < (len - bytes_read)) ?
			     page_read : (len - bytes_read);

		arch_cpu_irq_save(flags);

#if !defined(ARCH_HAS_MEMORY_READWRITE)
		rc = arch_cpu_aspace_map(tmp_va, hpa & ~VMM_PAGE_MASK,
					 (cacheable) ?
					 VMM_MEMORY_FLAGS_NORMAL :
					 VMM_MEMORY_FLAGS_NORMAL_NOCACHE);
		if (rc) {
			break;
		}

		memcpy(dst, (void *)(tmp_va + page_offset), page_read);

		rc = arch_cpu_aspace_unmap(tmp_va);
		if (rc) {
			break;
		}
#else
		rc = arch_cpu_aspace_memory_read(tmp_va, hpa,
						 dst, page_read, cacheable);
		if (rc) {
			break;
		}
#endif

		arch_cpu_irq_restore(flags);

		hpa += page_read;
		bytes_read += page_read;
		dst += page_read;
	}

	return bytes_read;
}

u32 vmm_host_memory_write(physical_addr_t hpa,
			  void *src, u32 len, bool cacheable)
{
	int rc;
	irq_flags_t flags;
	u32 bytes_written = 0, page_offset, page_write;
	virtual_addr_t tmp_va = host_mem_rw_va[vmm_smp_processor_id()];

	/* Write one page at time with irqs disabled since, we use
	 * one virtual address per-host CPU to do read/write.
	 */
	while (bytes_written < len) {
		page_offset = hpa & VMM_PAGE_MASK;

		page_write = VMM_PAGE_SIZE - page_offset;
		page_write = (page_write < (len - bytes_written)) ?
			      page_write : (len - bytes_written);

		arch_cpu_irq_save(flags);

#if !defined(ARCH_HAS_MEMORY_READWRITE)
		rc = arch_cpu_aspace_map(tmp_va, hpa & ~VMM_PAGE_MASK,
					 (cacheable) ?
					 VMM_MEMORY_FLAGS_NORMAL :
					 VMM_MEMORY_FLAGS_NORMAL_NOCACHE);
		if (rc) {
			break;
		}

		memcpy((void *)(tmp_va + page_offset), src, page_write);

		rc = arch_cpu_aspace_unmap(tmp_va);
		if (rc) {
			break;
		}
#else
		rc = arch_cpu_aspace_memory_write(tmp_va, hpa,
						  src, page_write, cacheable);
		if (rc) {
			break;
		}
#endif

		arch_cpu_irq_restore(flags);

		hpa += page_write;
		bytes_written += page_write;
		src += page_write;
	}

	return bytes_written;
}

u32 vmm_host_memory_set(physical_addr_t hpa,
			  u8 byte, u32 len, bool cacheable)
{
	u8 buf[256];
	u32 to_wr, wr, total_written = 0;
	physical_addr_t pos, end;

	memset(buf, byte, sizeof(buf));

	pos = hpa;
	end = hpa + len;
	while (pos < end) {
		to_wr = (sizeof(buf) < (end - pos)) ?
					sizeof(buf) : (end - pos);

		wr = vmm_host_memory_write(pos, buf, to_wr, cacheable);

		pos += to_wr;
		total_written += to_wr;

		if (wr < to_wr) {
			break;
		}
	}

	return total_written;
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
		vmm_panic("%s: failed to free pages error=%d\n",
			  __func__, rc);
	}

	return (init_size >> VMM_PAGE_SHIFT) * VMM_PAGE_SIZE / 1024;
}

int __cpuinit vmm_host_aspace_init(void)
{
	int rc, cpu, bank_found = 0;
	u32 resv, resv_count, bank, bank_count = 0x0;
	physical_addr_t ram_start, core_resv_pa = 0x0, arch_resv_pa = 0x0;
	physical_size_t ram_size;
	virtual_addr_t vapool_start, vapool_hkstart, ram_hkstart, mhash_hkstart;
	virtual_size_t vapool_size, vapool_hksize, ram_hksize, mhash_hksize;
	virtual_size_t hk_total_size = 0x0;
	virtual_addr_t core_resv_va = 0x0, arch_resv_va = 0x0;
	virtual_size_t core_resv_sz = 0x0, arch_resv_sz = 0x0;

	/* For Non-Boot CPU just call arch code and return */
	if (!vmm_smp_is_bootcpu()) {
		rc = arch_cpu_aspace_secondary_init();
		if (rc) {
			return rc;
		}

#if defined(ARCH_HAS_MEMORY_READWRITE)
		/* Initialize memory read/write for Non-Boot CPU */
		rc = arch_cpu_aspace_memory_rwinit(
				host_mem_rw_va[vmm_smp_processor_id()]);
		if (rc) {
			return rc;
		}
#endif

		return VMM_OK;
	}

	/* Determine VAPOOL start and size */
	vapool_start = arch_code_vaddr_start();
	vapool_size = (CONFIG_VAPOOL_SIZE_MB << 20);

	/* Determine VAPOOL house-keeping size based on VAPOOL size */
	vapool_hksize = vmm_host_vapool_estimate_hksize(vapool_size);

	/* Determine RAM bank count, start and size */
	if ((rc = arch_devtree_ram_bank_setup())) {
		return rc;
	}
	if ((rc = arch_devtree_ram_bank_count(&bank_count))) {
		return rc;
	}
	if (bank_count == 0) {
		return VMM_ENOMEM;
	}
	if (bank_count > CONFIG_MAX_RAM_BANK_COUNT) {
		return VMM_EINVALID;
	}
	bank_found = 0;
	for (bank = 0; bank < bank_count; bank++) {
		if ((rc = arch_devtree_ram_bank_start(bank, &ram_start))) {
			return rc;
		}
		if (ram_start & VMM_PAGE_MASK) {
			return VMM_EINVALID;
		}
		if ((rc = arch_devtree_ram_bank_size(bank, &ram_size))) {
			return rc;
		}
		if (ram_size & VMM_PAGE_MASK) {
			return VMM_EINVALID;
		}
		if ((ram_start <= arch_code_paddr_start()) &&
		    (arch_code_paddr_start() < (ram_start + ram_size))) {
			bank_found = 1;
			break;
		}
	}
	if (!bank_found) {
		return VMM_ENODEV;
	}

	/* Determine RAM house-keeping size */
	ram_hksize = vmm_host_ram_estimate_hksize();

	/* Determine memmap hash house-keeping size */
	mhash_hksize = host_mhash_estimate_hksize();

	/* Calculate physical address, virtual address, and size of
	 * core reserved space for VAPOOL, RAM, and MEMMAP HASH house-keeping
	 */
	hk_total_size = vapool_hksize + ram_hksize + mhash_hksize;
	hk_total_size = VMM_ROUNDUP2_PAGE_SIZE(hk_total_size);
	core_resv_pa = ram_start;
	core_resv_va = vapool_start + arch_code_size();
	core_resv_sz = hk_total_size;

	/* We cannot estimate the physical address, virtual address,
	 * and size of arch reserved space so we set all of them to
	 * zero and expect that arch_primary_cpu_aspace_init() will
	 * update them if arch code requires arch reserved space.
	 */
	arch_resv_pa = 0x0;
	arch_resv_va = 0x0;
	arch_resv_sz = 0x0;

	/* Call arch_primary_cpu_aspace_init() with the estimated
	 * parameters for core reserved space and arch reserved space.
	 * The arch_primary_cpu_aspace_init() can change these parameter
	 * as needed.
	 */
	if ((rc = arch_cpu_aspace_primary_init(&core_resv_pa,
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
	vapool_hkstart = core_resv_va;
	ram_hkstart = vapool_hkstart + vapool_hksize;
	mhash_hkstart = ram_hkstart + ram_hksize;

	/* Initialize VAPOOL managment */
	if ((rc = vmm_host_vapool_init(vapool_start,
					vapool_size,
					vapool_hkstart))) {
		return rc;
	}

	/* Initialize RAM managment */
	if ((rc = vmm_host_ram_init(ram_hkstart))) {
		return rc;
	}

	/* Initialize MEMMAP HASH */
	if ((rc = host_mhash_init(mhash_hkstart, mhash_hksize))) {
		return rc;
	}

	/* Reserve all pages covering code space, core reserved space,
	 * and arch reserved space in VAPOOL & RAM.
	 */
	if (arch_code_vaddr_start() < core_resv_va) {
		core_resv_va = arch_code_vaddr_start();
	}
	if ((arch_resv_sz > 0) && (arch_resv_va < core_resv_va)) {
		core_resv_va = arch_resv_va;
	}
	if (arch_code_paddr_start() < core_resv_pa) {
		core_resv_pa = arch_code_paddr_start();
	}
	if ((arch_resv_sz > 0) &&
	    (arch_resv_pa < core_resv_pa)) {
		core_resv_pa = arch_resv_pa;
	}
	if ((core_resv_va + core_resv_sz) <
			(arch_code_vaddr_start() + arch_code_size())) {
		core_resv_sz =
		(arch_code_vaddr_start() + arch_code_size()) - core_resv_va;
	}
	if ((arch_resv_sz > 0) &&
	    ((core_resv_va + core_resv_sz) < (arch_resv_va + arch_resv_sz))) {
		core_resv_sz = (arch_resv_va + arch_resv_sz) - core_resv_va;
	}
	if ((rc = vmm_host_vapool_reserve(core_resv_va,
					  core_resv_sz))) {
		return rc;
	}
	if ((rc = vmm_host_ram_reserve(core_resv_pa,
				       core_resv_sz))) {
		return rc;
	}
	if ((rc = host_mhash_add(core_resv_pa,
				 core_resv_va,
				 core_resv_sz,
				 VMM_MEMORY_FLAGS_NORMAL))) {
		return rc;
	}

	/* Reserve portion of RAM as specified by
	 * arch device tree functions.
	 */
	if ((rc = arch_devtree_reserve_count(&resv_count))) {
		return rc;
	}
	for (resv = 0; resv < resv_count; resv++) {
		if ((rc = arch_devtree_reserve_addr(resv, &ram_start))) {
			return rc;
		}
		if ((rc = arch_devtree_reserve_size(resv, &ram_size))) {
			return rc;
		}
		if (ram_start & VMM_PAGE_MASK) {
			ram_size += ram_start & VMM_PAGE_MASK;
			ram_start -= ram_start & VMM_PAGE_MASK;
		}
		ram_size &= ~VMM_PAGE_MASK;
		if ((rc = vmm_host_ram_reserve(ram_start, ram_size))) {
			return rc;
		}
	}

	/* Setup temporary virtual address for physical read/write */
	for (cpu = 0; cpu < CONFIG_CPU_COUNT; cpu++) {
		rc = vmm_host_vapool_alloc(&host_mem_rw_va[cpu],
					   VMM_PAGE_SIZE);
		if (rc) {
			return rc;
		}
	}

#if defined(ARCH_HAS_MEMORY_READWRITE)
	/* Initialize memory read/write for Boot CPU */
	rc = arch_cpu_aspace_memory_rwinit(
				host_mem_rw_va[vmm_smp_bootcpu_id()]);
	if (rc) {
		return rc;
	}
#endif

	return VMM_OK;
}
