/**
 * Copyright (c) 2010-20 Himanshu Chauhan.
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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief main source file for board specific code
 */

#include <vmm_main.h>
#include <vmm_devtree.h>
#include <vmm_error.h>
#include <vmm_host_aspace.h>
#include <vmm_devdrv.h>
#include <vmm_stdio.h>
#include <hpet.h>
#include <arch_board.h>

int __init arch_board_early_init(void)
{
	int rv;

	rv = hpet_init();
	BUG_ON(rv != VMM_OK);

        return VMM_OK;
}

int __init arch_board_final_init(void)
{
#if 0
	int rc;
	struct vmm_devtree_node *node;

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Do Probing using device driver framework */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
					VMM_DEVTREE_HOSTINFO_NODE_NAME
					VMM_DEVTREE_PATH_SEPARATOR_STRING
					"plb");

	if(!node) {
		return VMM_ENOTAVAIL;
	}

	rc = vmm_devdrv_probe(node);
	if(rc) {
		return rc;
	}
#endif
	return VMM_OK;
}

int arch_board_reset(void)
{
	return VMM_EFAIL;
}

int arch_board_shutdown(void)
{
	return VMM_EFAIL;
}
