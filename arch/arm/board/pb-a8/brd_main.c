/**
 * Copyright (c) 2011 Anup Patel.
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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <libfdt.h>
#include <pba8_board.h>
#include <realview/timer.h>

extern u32 dt_blob_start;
virtual_addr_t pba8_sys_base;

int vmm_board_ram_start(physical_addr_t * addr)
{
	int rc = VMM_OK;
	fdt_fileinfo_t fdt;
	fdt_node_header_t * fdt_node;
	fdt_property_t * prop;
	
	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	fdt_node = libfdt_find_node(&fdt, 
				    VMM_DEVTREE_PATH_SEPRATOR_STRING
				    VMM_DEVTREE_HOSTINFO_NODE_NAME
				    VMM_DEVTREE_PATH_SEPRATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	prop = libfdt_get_property(&fdt, fdt_node,
				   VMM_DEVTREE_MEMORY_PHYS_ADDR_ATTR_NAME);
	if (!prop) {
		return VMM_EFAIL;
	}
	*addr = *((physical_addr_t *)prop->data);

	return VMM_OK;
}

int vmm_board_ram_size(physical_size_t * size)
{
	int rc = VMM_OK;
	fdt_fileinfo_t fdt;
	fdt_node_header_t * fdt_node;
	fdt_property_t * prop;
	
	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	fdt_node = libfdt_find_node(&fdt, 
				    VMM_DEVTREE_PATH_SEPRATOR_STRING
				    VMM_DEVTREE_HOSTINFO_NODE_NAME
				    VMM_DEVTREE_PATH_SEPRATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	prop = libfdt_get_property(&fdt, fdt_node,
				   VMM_DEVTREE_MEMORY_PHYS_SIZE_ATTR_NAME);
	if (!prop) {
		return VMM_EFAIL;
	}
	*size = *((physical_size_t *)prop->data);

	return VMM_OK;
}

int vmm_devtree_populate(vmm_devtree_node_t ** root,
			 char **string_buffer, 
			 size_t * string_buffer_size)
{
	int rc = VMM_OK;
	fdt_fileinfo_t fdt;
	
	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	return libfdt_parse_devtree(&fdt, 
				    root, 
				    string_buffer, 
				    string_buffer_size);
}

int vmm_board_getclock(vmm_devtree_node_t * node, u32 * clock)
{
	if (!node || !clock) {
		return VMM_EFAIL;
	}

	if (vmm_strcmp(node->name, "uart0") == 0) {
		*clock = 24000000;
	} else if (vmm_strcmp(node->name, "uart1") == 0) {
		*clock = 24000000;
	} else {
		*clock = 100000000;
	}

	return VMM_OK;
}

int vmm_board_reset(void)
{
	/* FIXME: Write 0x1 to RESETCTL just like linux does */
#if 0
	vmm_writel(REALVIEW_SYS_CTRL_RESET_CONFIGCLR, 
		   (void *)(pba8_sys_base + REALVIEW_SYS_RESETCTL_OFFSET));
#else
	vmm_writel(0x100, 
		   (void *)(pba8_sys_base + REALVIEW_SYS_RESETCTL_OFFSET));
#endif

	return VMM_OK;
}

int vmm_board_shutdown(void)
{
	/* FIXME: TBD */
	return VMM_OK;
}

int vmm_board_early_init(void)
{
	/*
	 * TODO:
	 * Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */
	return 0;
}

int vmm_board_final_init(void)
{
	int rc;
	vmm_devtree_node_t *node;

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Map control registers */
	pba8_sys_base = vmm_host_iomap(REALVIEW_SYS_BASE, 0x1000);

	/* Unlock Lockable registers */
	vmm_writel(REALVIEW_SYS_LOCKVAL, 
		   (void *)(pba8_sys_base + REALVIEW_SYS_LOCK_OFFSET));

	/* Do Probing using device driver framework */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPRATOR_STRING "nbridge");

	if (!node) {
		return VMM_ENOTAVAIL;
	}

	rc = vmm_devdrv_probe(node);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}
