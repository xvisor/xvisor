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
 * @file acpi.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief ACPI parser structure definition.
 */

#ifndef __ACPI_H__
#define __ACPI_H__

#include <vmm_types.h>
#include <vmm_devtree.h>

#define VMM_DEVTREE_MOTHERBOARD_NODE_NAME	"motherboard"
#define VMM_DEVTREE_LAPIC_NODE_PARENT_NAME	"apic"
#define VMM_DEVTREE_LAPIC_PCPU_NODE_FMT		"lapic@%d"
#define VMM_DEVTREE_LAPIC_CPU_ID_ATTR_NAME	"acpi_cpu_id"
#define VMM_DEVTREE_LAPIC_LAPIC_ID_ATTR_NAME	"lapic_id"
#define VMM_DEVTREE_IOAPIC_NODE_FMT		"ioapic@%d"
#define VMM_DEVTREE_IOAPIC_PADDR_ATTR_NAME	"phys_addr"
#define VMM_DEVTREE_IOAPIC_GINT_BASE_ATTR_NAME	"gint_base"
#define VMM_DEVTREE_NR_IOAPIC_ATTR_NAME		"nr_ioapic"
#define VMM_DEVTREE_NR_LAPIC_ATTR_NAME		"nr_lapic"
#define VMM_DEVTREE_NR_HPET_ATTR_NAME		"nr_hpet"
#define VMM_DEVTREE_HPET_NODE_FMT		"hpet@%d"
#define VMM_DEVTREE_HPET_PADDR_ATTR_NAME	"phys_addr"
#define VMM_DEVTREE_HPET_ID_ATTR_NAME		"id"

struct acpi_search_area {
	char *area_name;
	physical_addr_t phys_start;
	physical_addr_t phys_end;
};

extern struct acpi_search_area acpi_areas[];

#define RSDP_SIGN_LEN		8
#define OEM_ID_LEN		6
#define SDT_SIGN_LEN		4

#define RSDP_SIGNATURE		"RSD PTR "
#define RSDT_SIGNATURE		"RSDT"
#define HPET_SIGNATURE		"HPET"
#define APIC_SIGNATURE		"APIC"

#define NR_HPET_TIMER_BLOCKS	8

/* Root system description pointer */
struct acpi_rsdp {
	u8 signature[RSDP_SIGN_LEN];
	u8 checksum;
	u8 oem_id[OEM_ID_LEN];
	u8 rev;
	u32 rsdt_addr;
	u32 rsdt_len;
	u64 xsdt_addr;
	u8 xchecksum;
	u8 reserved[3];
} __packed;

struct acpi_sdt_hdr {
	u8 signature[SDT_SIGN_LEN];
	u32 len;
	u8 rev;
	u8 checksum;
	u8 oem_id[OEM_ID_LEN];
	u64 oem_table_id;
	u32 oem_rev;
	u32 creator_id;
	u32 creator_rev;
} __packed;


#define MAX_RSDT	35 /* ACPI defines 35 signatures */

struct acpi_rsdt {
	struct acpi_sdt_hdr hdr;
	u32 data[MAX_RSDT];
};

struct acpi_madt_hdr {
	struct acpi_sdt_hdr	hdr;
	u32			local_apic_address;
	u32		flags;
};

#define ACPI_MADT_TYPE_LAPIC		0
#define ACPI_MADT_TYPE_IOAPIC		1
#define ACPI_MADT_TYPE_INT_SRC		2
#define ACPI_MADT_TYPE_NMI_SRC		3
#define ACPI_MADT_TYPE_LAPIC_NMI	4
#define ACPI_MADT_TYPE_LAPIC_ADRESS	5
#define ACPI_MADT_TYPE_IOSAPIC		6
#define ACPI_MADT_TYPE_LSAPIC		7
#define ACPI_MADT_TYPE_PLATFORM_INT_SRC	8
#define ACPI_MADT_TYPE_Lx2APIC		9
#define ACPI_MADT_TYPE_Lx2APIC_NMI	10

struct acpi_madt_item_hdr {
	u8	type;
	u8	length;
} __packed;

struct acpi_madt_lapic {
	struct acpi_madt_item_hdr hdr;
	u8	acpi_cpu_id;
	u8	apic_id;
	u32	flags;
} __packed;

struct acpi_madt_ioapic {
	struct acpi_madt_item_hdr hdr;
	u8	id;
	u8	__reserved;
	u32	address;
	u32	global_int_base;
} __packed;

struct acpi_madt_int_src {
	struct acpi_madt_item_hdr hdr;
	u8	bus;
	u8	bus_int;
	u32	global_int;
	u16	mps_flags;
} __packed;

struct acpi_madt_nmi {
	struct acpi_madt_item_hdr hdr;
	u16	flags;
	u32	global_int;
} __packed;

struct acpi_timer_blocks {
	u32 blkid;
	u8 asid;
	u8 rbw;
	u8 rbo;
	u8 resvd;
	u64 base;
	u8 id;
	u16 min_clk_tick;
	u8 pg_prot;
} __packed;

struct acpi_hpet {
	struct acpi_sdt_hdr hdr;
	struct acpi_timer_blocks tmr_blks[NR_HPET_TIMER_BLOCKS];
} __packed;

int acpi_init(void);

#endif /* __ACPI_H__ */
