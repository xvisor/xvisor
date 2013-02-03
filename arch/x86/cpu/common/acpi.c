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
 * @file acpi.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief ACPI parser.
 * Some part of the code for MADT and other SDT parsing
 * is taken, with some modifications, from MINIX3.
 *
 * My sincere thanks for MINIX3 developers.
 */

#include <vmm_types.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <libs/stringlib.h>
#include <arch_cpu.h>
#include <cpu_mmu.h>
#include <cpu_apic.h>
#include <cpu_private.h>
#include <acpi.h>

struct sdt_lookup_table {
	char	signature [SDT_SIGN_LEN + 1];
	size_t	length;
};

struct acpi_context {
	u32 nr_sys_hdr;
	struct acpi_rsdp *root_desc;
	struct acpi_rsdt *rsdt;
	struct acpi_madt_hdr *madt_hdr;
	struct acpi_hpet *hpet;
	struct sdt_lookup_table sdt_trans[MAX_RSDT];
};

static struct acpi_context *acpi_ctxt = NULL;
static u32 nr_hpet_blks = 0;

struct acpi_search_area acpi_areas[] = {
	{ "Extended BIOS Data Area (EBDA)", 0x0009FC00, 0x0009FFFF },
	{ "BIOS Read-Only Memory", 0xE0000, 0xFFFFF },
	{ NULL, 0, 0 },
};

static u64 locate_rsdp_in_area(virtual_addr_t vaddr, u32 size)
{
	virtual_addr_t addr;

	for (addr = vaddr; addr < (vaddr + size); addr += 16) {
		if (!memcmp((const void *)addr, RSDP_SIGNATURE,
				RSDP_SIGN_LEN)) {
			return addr;
		}
	}

	return 0;
}

static int acpi_check_csum(struct acpi_sdt_hdr * tb, size_t size)
{
	u8 total = 0;
	int i;
	for (i = 0; i < size; i++)
		total += ((unsigned char *)tb)[i];

	return total == 0 ? 0 : -1;
}

static int acpi_check_signature(const char *orig, const char *match)
{
	return strncmp(orig, match, SDT_SIGN_LEN);
}

static physical_addr_t acpi_get_table_base(const char * name)
{
	int i;

	for(i = 0; i < acpi_ctxt->nr_sys_hdr; i++) {
		if (strncmp(name, acpi_ctxt->sdt_trans[i].signature,
				SDT_SIGN_LEN) == 0)
			return acpi_ctxt->rsdt->data[i];
	}

	return 0;
}

static int acpi_read_sdt_at(physical_addr_t addr,
			    struct acpi_sdt_hdr * tb,
			    size_t size,
			    const char * name)
{
	struct acpi_sdt_hdr hdr;
	void *sdt_va = NULL;

	sdt_va = (void *)vmm_host_iomap(addr, PAGE_SIZE);
	if (unlikely(!sdt_va)) {
		vmm_printf("ACPI ERROR: Failed to map physical address 0x%x.\n",
			   __func__, addr);
		return VMM_EFAIL;
	}

	/* if NULL is supplied, we only return the size of the table */
	if (tb == NULL) {
		memcpy(&hdr, sdt_va, sizeof(struct acpi_sdt_hdr));
		return hdr.len;
	}

	memcpy(tb, sdt_va, sizeof(struct acpi_sdt_hdr));

	if (acpi_check_signature((const char *)tb->signature,
				 (const char *)name)) {
		vmm_printf("ACPI ERROR: acpi %s signature does not match\n", name);
		return VMM_EFAIL;
	}

	if (size < tb->len) {
		vmm_printf("ACPI ERROR: acpi buffer too small for %s\n", name);
		return VMM_EFAIL;
	}

	memcpy(tb, sdt_va, size);

	if (acpi_check_csum(tb, tb->len)) {
		vmm_printf("ACPI ERROR: acpi %s checksum does not match\n", name);
		return VMM_EFAIL;
	}

	return tb->len;
}

size_t acpi_get_table_length(const char * name)
{
	int i;

	for(i = 0; i < acpi_ctxt->nr_sys_hdr; i++) {
		if (strncmp(name, acpi_ctxt->sdt_trans[i].signature,
				SDT_SIGN_LEN) == 0)
			return acpi_ctxt->sdt_trans[i].length;
	}

	return 0;
}

static void * acpi_madt_get_typed_item(struct acpi_madt_hdr * hdr,
				       unsigned char type,
				       unsigned idx)
{
	u8 * t, * end;
	int i;

	t = (u8 *) hdr + sizeof(struct acpi_madt_hdr);
	end = (u8 *) hdr + hdr->hdr.len;

	i = 0;
	while(t < end) {
		if (type == ((struct acpi_madt_item_hdr *) t)->type) {
			if (i == idx)
				return t;
			else
				i++;
		}
		t += ((struct acpi_madt_item_hdr *) t)->length;
	}

	return NULL;
}

static virtual_addr_t find_root_system_descriptor(void)
{
	struct acpi_search_area *carea = &acpi_areas[0];
	virtual_addr_t area_map;
	virtual_addr_t rsdp_base = 0;

	while (carea->area_name) {
		vmm_printf("Search for RSDP in %s... ", carea->area_name);
		area_map = vmm_host_iomap(carea->phys_start,
					  (carea->phys_end
					   - carea->phys_start));
		BUG_ON((void *)area_map == NULL);

		if ((rsdp_base
		     = locate_rsdp_in_area(area_map,
					   (carea->phys_end
					    - carea->phys_start))) != 0) {
			vmm_printf("found.\n");
			break;
		}

		rsdp_base = 0;
		carea++;
		vmm_host_iounmap(area_map,
				 (carea->phys_end - carea->phys_start));
		vmm_printf("not found.\n");
	}

	if (likely(rsdp_base))
		vmm_printf("RSDP Base: 0x%x\n", rsdp_base);

	return rsdp_base;
}

int acpi_init(void)
{
	int i;

	if (!acpi_ctxt) {
		vmm_printf("Starting to parse ACPI tables...\n");
		acpi_ctxt = vmm_malloc(sizeof(struct acpi_context));
		if (!acpi_ctxt) {
			vmm_printf("ACPI ERROR: Failed to allocate memory for"
				   " ACPI context.\n");
			return VMM_EFAIL;
		}

		acpi_ctxt->root_desc =
			(struct acpi_rsdp *)find_root_system_descriptor();
		acpi_ctxt->rsdt = NULL;

		if (acpi_ctxt->root_desc == NULL) {
			vmm_printf("ACPI ERROR: No root system descriptor"
				   " table found!\n");
			goto rdesc_fail;
		}

		if (acpi_ctxt->root_desc->rsdt_addr == 0) {
			vmm_printf("ACPI ERROR: No root descriptor found"
				   " in RSD Pointer!\n");
			goto rsdt_fail;
		}

		acpi_ctxt->rsdt =
			(struct acpi_rsdt *)vmm_malloc(sizeof(struct acpi_rsdt));

		if (!acpi_ctxt->rsdt)
			goto rsdt_fail;

		if (acpi_read_sdt_at(acpi_ctxt->root_desc->rsdt_addr,
				     (struct acpi_sdt_hdr *)acpi_ctxt->rsdt,
				     sizeof(struct acpi_rsdt),
				     RSDT_SIGNATURE) < 0) {
			goto sdt_fail;
		}

		acpi_ctxt->nr_sys_hdr = (acpi_ctxt->rsdt->hdr.len
					 - sizeof(struct acpi_sdt_hdr))/sizeof(u32);

		vmm_printf("Number of system headers: %d: ", acpi_ctxt->nr_sys_hdr);

		for (i = 0; i < acpi_ctxt->nr_sys_hdr; i++) {
			struct acpi_sdt_hdr *hdr;

			hdr = (struct acpi_sdt_hdr *)
				vmm_host_iomap(acpi_ctxt->rsdt->data[i],
					       PAGE_SIZE);

			if (hdr == NULL) {
				vmm_printf("ACPI ERROR: Cannot read header at 0x%x\n",
					   acpi_ctxt->rsdt->data[i]);
				goto sdt_fail;
			}

			memcpy(&acpi_ctxt->sdt_trans[i].signature, &hdr->signature, SDT_SIGN_LEN);

			acpi_ctxt->sdt_trans[i].signature[SDT_SIGN_LEN] = '\0';
			vmm_printf("%s ", acpi_ctxt->sdt_trans[i].signature);
			acpi_ctxt->sdt_trans[i].length = hdr->len;

			/* FIXME: Cleanup required. All this mapping should be freed
			 * as and when required otherwise we will end up using all
			 * VMM virtual address space.
			 *
			 * host_iounmap also requires fix. Page need to marked absent
			 * and it needs to be flushed from TLB.
			 */
			//vmm_host_iounmap((virtual_addr_t)hdr, PAGE_SIZE);
		}

		vmm_printf("\n");
		acpi_ctxt->madt_hdr = (struct acpi_madt_hdr *)
			vmm_host_iomap(acpi_get_table_base("APIC"),
				       PAGE_SIZE);
		if (acpi_ctxt->madt_hdr == NULL)
			goto sdt_fail;

		acpi_ctxt->hpet = (struct acpi_hpet *)
			vmm_malloc(sizeof(struct acpi_hpet));

		if (!acpi_ctxt->hpet)
			goto sdt_fail;

		if (acpi_read_sdt_at(acpi_get_table_base(HPET_SIGNATURE),
				     (struct acpi_sdt_hdr *)acpi_ctxt->hpet,
				     sizeof(struct acpi_hpet),
				     HPET_SIGNATURE) < 0) {
			vmm_free(acpi_ctxt->hpet);
			goto sdt_fail;
		}

		nr_hpet_blks = (acpi_ctxt->hpet->hdr.len - sizeof(struct acpi_sdt_hdr))/sizeof(struct acpi_timer_blocks);

		vmm_printf("Total %d HPET Timer Blocks:\n", nr_hpet_blks);
		for (i = 0; i < nr_hpet_blks; i++) {
			vmm_printf("%d. Base: %llx\n",
				   acpi_ctxt->hpet->tmr_blks[i].asid+1,
				   acpi_ctxt->hpet->tmr_blks[i].base);
		}
	}

	return VMM_OK;

 sdt_fail:
	vmm_free(acpi_ctxt->rsdt);
 rsdt_fail:
	vmm_host_iounmap((virtual_addr_t)acpi_ctxt->root_desc,
			 PAGE_SIZE);
 rdesc_fail:
	vmm_free(acpi_ctxt);
	acpi_ctxt = NULL;

	return VMM_EFAIL;
}

struct acpi_madt_ioapic * acpi_get_ioapic_next(void)
{
	static unsigned idx = 0;
	struct acpi_madt_ioapic *ret;

	ret = (struct acpi_madt_ioapic *)
		acpi_madt_get_typed_item(acpi_ctxt->madt_hdr,
					 ACPI_MADT_TYPE_IOAPIC, idx);

	if (ret)
		idx++;

	return ret;
}

struct acpi_madt_lapic * acpi_get_lapic_next(void)
{
	static unsigned idx = 0;
	struct acpi_madt_lapic *ret;

	for (;;) {
		ret = (struct acpi_madt_lapic *)
			acpi_madt_get_typed_item(acpi_ctxt->madt_hdr,
						 ACPI_MADT_TYPE_LAPIC, idx);
		if (!ret)
			break;

		idx++;

		/* report only usable CPUs */
		if (ret->flags & 1)
			break;
	}

	return ret;
}

u32 acpi_get_nr_hpet_chips(void)
{
	/* TODO: Change ACPI parsing code to find more HPET */
	return 1;
}

u32 hpet_nr_timers_in_block(u32 block)
{
	/* FIXME: Number of timers. */
	return 32;
}

u32 acpi_get_nr_hpet_blocks(u32 chip)
{
	/*
	 * TODO: When above is fixed, return the blocks from
	 * the selected chip.
	 */
	return nr_hpet_blks;
}

u64 acpi_get_hpet_block_base(u32 idx)
{
	if (idx >= nr_hpet_blks) {
		return 0;
	}

	return acpi_ctxt->hpet->tmr_blks[idx].base;
}
