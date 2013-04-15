/**
 * Copyright (c) 2012 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <arch_board.h>
#include <arch_timer.h>
#include <sunxi/timer.h>

/*
 * Reset & Shutdown
 */

int arch_board_reset(void)
{
	return aw_timer_force_reset();
}

int arch_board_shutdown(void)
{
	/* FIXME: Don't know how to poweroff !!!!! */
	return VMM_EFAIL;
}

/*
 * Initialization functions
 */

int __init arch_board_early_init(void)
{
	/* Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */

	return 0;
}

int __init arch_clocksource_init(void)
{
	return aw_timer_clocksource_init();
}

int __cpuinit arch_clockchip_init(void)
{
	return aw_timer_clockchip_init();
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *hnode, *node;

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Initialize timer misc APIs */
	rc = aw_timer_misc_init();
	if (rc) {
		return rc;
	}

	/* Get host node */
	hnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_HOSTINFO_NODE_NAME);

	/* Find simple-bus node */
	node = vmm_devtree_find_compatible(hnode, NULL, "simple-bus");
	if (!node) {
		return VMM_ENODEV;
	}

	/* Do probing using device driver framework */
	rc = vmm_devdrv_probe(node);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}
