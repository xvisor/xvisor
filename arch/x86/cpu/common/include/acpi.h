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

struct acpi_search_area {
	char *area_name;
	physical_addr_t phys_start;
	physical_addr_t phys_end;
};

extern struct acpi_search_area acpi_areas[];

#define RSDP_SIGNATURE		"RSD PTR "
#define RSDT_SIGNATURE		"RSDT"
#define HPET_SIGNATURE		"HPET"

/* Root system description pointer */
struct rsdp {
	u8 signature[8];
	u8 checksum;
	u8 oem_id[6];
	u8 rev;
	u32 rsdt_addr;
	u32 rsdt_len;
	u64 xsdt_addr;
	u8 xchecksum;
	u8 reserved[3];
} __packed;

struct sdt_header {
	u8 signature[4];
	u32 len;
	u8 rev;
	u8 checksum;
	u8 oem_id[6];
	u64 oem_table_id;
	u32 oem_rev;
	u32 creator_id;
	u32 creator_rev;
} __packed;

int acpi_init(void);

#endif /* __ACPI_H__ */
