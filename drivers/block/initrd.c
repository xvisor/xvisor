/**
 * Copyright (c) 2015 Jean-Christophe Dubois.
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
 * @file initrd.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief initrd block device driver.
 */

#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <vmm_stdio.h>
#include <drv/rbd.h>

#define MODULE_DESC			"initrd Driver"
#define MODULE_AUTHOR			"Jean-Christophe Dubois"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(RBD_IPRIORITY + 1)
#define	MODULE_INIT			initrd_driver_init
#define	MODULE_EXIT			initrd_driver_exit

#define INITRD_START_ATTR_NAME	"linux,initrd-start"
#define INITRD_END_ATTR_NAME	"linux,initrd-end"

static struct rbd *initrd_rdb;

static int __init initrd_driver_init(void)
{
	struct vmm_devtree_node *node;
	u64 initrd_start, initrd_end;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOSEN_NODE_NAME);

	/* There should be a /chosen node */
	if (!node) {
		vmm_printf("initrd: No chosen node\n", __func__);
		return VMM_ENODEV;
	}

	/* Is there a linux,initrd-start attribute */
	if (vmm_devtree_read_u64(node,
			INITRD_START_ATTR_NAME, &initrd_start) == VMM_OK) {

		/* If so is there also a linux,initrd-end attribte */
		if (vmm_devtree_read_u64(node,
			INITRD_END_ATTR_NAME, &initrd_end) != VMM_OK) {
			vmm_printf("initrd: failed to find %s\n",
				   INITRD_END_ATTR_NAME);
			goto error;
		}

		/* Let's do a little bit os sanity check */
		if (initrd_end <= initrd_start) {
			vmm_printf("initrd: Error: %s > %s \n",
				   INITRD_START_ATTR_NAME,
				   INITRD_END_ATTR_NAME);
			goto error;
		}

		/* OK, we know where the initrd device is located */
		if ((initrd_rdb = rbd_create("initrd",
					(physical_addr_t)initrd_start,
					(physical_size_t)(initrd_end -
						  initrd_start))) == NULL) {
			vmm_printf("initrd: rbd_create() failed\n");
			goto error;
		}

		vmm_printf("initrd: RBD created at 0x%016llx - 0x%016llx\n",
			   initrd_start, initrd_end);
	} else {
		vmm_printf("initrd: no %s/%s attribute\n",
			   INITRD_START_ATTR_NAME, INITRD_END_ATTR_NAME);
	}

 error:
	vmm_devtree_dref_node(node);

	return VMM_OK;
}

static void __exit initrd_driver_exit(void)
{
	if (initrd_rdb) {
		rbd_destroy(initrd_rdb);
		initrd_rdb = NULL;
	}
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
