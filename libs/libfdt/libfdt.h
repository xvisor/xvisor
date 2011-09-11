/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file libfdt.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Flattend device tree library header
 */

#ifndef __LIBFDT_H_
#define __LIBFDT_H_

#include <vmm_types.h>

#define FDT_MAGIC	0xd00dfeed	/* 4: version, 4: total size */
#define FDT_TAGSIZE	sizeof(u32)

#define FDT_BEGIN_NODE	0x1	/* Start node: full name */
#define FDT_END_NODE	0x2	/* End node */
#define FDT_PROP	0x3	/* Property: name off,
				   size, content */
#define FDT_NOP		0x4	/* nop */
#define FDT_END		0x9

#define FDT_V1_SIZE	(7*sizeof(u32))
#define FDT_V2_SIZE	(FDT_V1_SIZE + sizeof(u32))
#define FDT_V3_SIZE	(FDT_V2_SIZE + sizeof(u32))
#define FDT_V16_SIZE	FDT_V3_SIZE
#define FDT_V17_SIZE	(FDT_V16_SIZE + sizeof(u32))

struct fdt_header {
	u32 magic;		/* magic word FDT_MAGIC */
	u32 totalsize;		/* total size of DT block */
	u32 off_dt_struct;	/* offset to structure */
	u32 off_dt_strings;	/* offset to strings */
	u32 off_mem_rsvmap;	/* offset to memory reserve map */
	u32 version;		/* format version */
	u32 last_comp_version;	/* last compatible version */

	/* version 2 fields below */
	u32 boot_cpuid_phys;	/* Which physical CPU id we're
				   booting on */
	/* version 3 fields below */
	u32 size_dt_strings;	/* size of the strings block */

	/* version 17 fields below */
	u32 size_dt_struct;	/* size of the structure block */
};

typedef struct fdt_header fdt_header_t;

struct fdt_reserve_entry {
	u64 address;
	u64 size;
};

typedef struct fdt_reserve_entry fdt_reserve_entry_t;

struct fdt_node_header {
	u32 tag;
	char name[0];
};

typedef struct fdt_node_header fdt_node_header_t;

struct fdt_property {
	u32 tag;
	u32 len;
	u32 nameoff;
	char data[0];
};

typedef struct fdt_property fdt_property_t;

int libfdt_parse(virtual_addr_t fdt_addr,
		     vmm_devtree_node_t ** root,
		     char **string_buffer, size_t * string_buffer_size);

#endif /* __LIBFDT_H_ */
