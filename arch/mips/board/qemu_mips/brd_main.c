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
 * @file brd_main.c
 * @version 0.1
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief main source file for board specific code
 */

#include <vmm_main.h>
#include <vmm_devtree.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <vmm_devdrv.h>
#include <libfdt.h>

extern u32 dt_blob_start;
virtual_addr_t isa_vbase;

int vmm_devtree_populate(vmm_devtree_node_t **root,
			char **string_buffer,
			size_t *string_buffer_size)
{
	virtual_addr_t fdt_addr = (virtual_addr_t)&dt_blob_start;
	return libfdt_parse(fdt_addr,root,string_buffer,string_buffer_size);
}

int vmm_board_getclock(vmm_devtree_node_t *node, u32 *clock)
{
	if(!node || !clock) {
		return VMM_EFAIL;
	}

	if(vmm_strcmp(node->name, "uart0")==0) {
		*clock = 7372800;
	} else {
		*clock = 100000000;
	}

	return VMM_OK;
}

int __init vmm_board_early_init(void)
{
	isa_vbase = vmm_host_iomap(0x14000000UL, 0x1000);
	return (isa_vbase ? 0 : 1);
}

int __init vmm_board_final_init(void)
{
	int rc;
	vmm_devtree_node_t *node;

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Do Probing using device driver framework */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
					VMM_DEVTREE_HOSTINFO_NODE_NAME
					VMM_DEVTREE_PATH_SEPRATOR_STRING
					"plb");

	if(!node) {
		return VMM_ENOTAVAIL;
	}

	rc = vmm_devdrv_probe(node);
	if(rc) {
		return rc;
	}

	return VMM_OK;
}

int vmm_board_reset(void)
{
	return VMM_EFAIL;
}

int vmm_board_shutdown(void)
{
	return VMM_EFAIL;
}
