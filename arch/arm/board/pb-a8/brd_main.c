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
#include <vmm_libfdt.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <pba8_board.h>
#include <realview/realview_timer.h>

extern u32 dt_blob_start;
virtual_addr_t pba8_sys_base;
virtual_addr_t pba8_sctl_base;

int vmm_devtree_populate(vmm_devtree_node_t ** root,
			 char **string_buffer, size_t * string_buffer_size)
{
	virtual_addr_t fdt_addr = (virtual_addr_t) & dt_blob_start;
	return vmm_libfdt_parse(fdt_addr, root, string_buffer,
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
	virtual_addr_t timer_base;

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Map control registers */
	pba8_sys_base = vmm_host_iomap(REALVIEW_SYS_BASE, 0x1000);
	pba8_sctl_base = vmm_host_iomap(REALVIEW_SCTL_BASE, 0x1000);

	/* Initialize Realview timers */
	timer_base = vmm_host_iomap(REALVIEW_PBA8_TIMER0_1_BASE, 0x1000);
	rc = realview_timer_init(pba8_sctl_base, timer_base,
				 REALVIEW_TIMER1_EnSel);
	if (rc) {
		return rc;
	}
	rc = realview_timer_init(pba8_sctl_base, timer_base + 0x20,
				 REALVIEW_TIMER2_EnSel);
	if (rc) {
		return rc;
	}
	rc = vmm_host_iounmap(timer_base, 0x1000);
	if (rc) {
		return rc;
	}
	timer_base = vmm_host_iomap(REALVIEW_PBA8_TIMER2_3_BASE, 0x1000);
	rc = realview_timer_init(pba8_sctl_base, timer_base,
				 REALVIEW_TIMER3_EnSel);
	if (rc) {
		return rc;
	}
	rc = realview_timer_init(pba8_sctl_base, timer_base + 0x20,
				 REALVIEW_TIMER4_EnSel);
	if (rc) {
		return rc;
	}
	rc = vmm_host_iounmap(timer_base, 0x1000);
	if (rc) {
		return rc;
	}

	/* Unlock Lockable reigsters */
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
