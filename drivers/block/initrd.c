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
 * @author Anup Patel (anup@brainfault.org)
 * @brief initrd block device driver.
 */

#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <vmm_stdio.h>
#include <drv/rbd.h>
#include <drv/initrd.h>

#define MODULE_DESC			"INITRD Driver"
#define MODULE_AUTHOR			"Jean-Christophe Dubois"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		INITRD_IPRIORITY
#define	MODULE_INIT			initrd_driver_init
#define	MODULE_EXIT			initrd_driver_exit

static struct rbd *initrd_rbd;

void initrd_rbd_destroy(void)
{
	if (initrd_rbd) {
		rbd_destroy(initrd_rbd);
		initrd_rbd = NULL;
	}
}
VMM_EXPORT_SYMBOL(initrd_rbd_destroy);

struct rbd *initrd_rbd_get(void)
{
	return initrd_rbd;
}
VMM_EXPORT_SYMBOL(initrd_rbd_get);

int initrd_devtree_update(u64 start, u64 end)
{
	int rc = VMM_OK;
	struct vmm_devtree_node *node;

	/* Sanity checks */
	if (start >= end) {
		return VMM_EINVALID;
	}
	if (initrd_rbd) {
		return VMM_EBUSY;
	}

	/* There should be a /chosen node */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOSEN_NODE_NAME);
	if (!node) {
		return VMM_ENODEV;
	}

	/* Update start attribute in /chosen node */
	rc = vmm_devtree_setattr(node, INITRD_START_ATTR2_NAME,
				 &start, VMM_DEVTREE_ATTRTYPE_UINT64,
				 sizeof(start), FALSE);
	if (rc) {
		goto done;
	}

	/* Update end attribute in /chosen node */
	rc = vmm_devtree_setattr(node, INITRD_END_ATTR2_NAME,
				 &end, VMM_DEVTREE_ATTRTYPE_UINT64,
				 sizeof(end), FALSE);
	if (rc) {
		goto done;
	}

done:
	vmm_devtree_dref_node(node);

	return rc;
}
VMM_EXPORT_SYMBOL(initrd_devtree_update);

static int __init initrd_driver_init(void)
{
	struct vmm_devtree_node *node;
	u64 initrd_start, initrd_end;

	/* There should be a /chosen node */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOSEN_NODE_NAME);
	if (!node) {
		vmm_printf("initrd: No chosen node\n");
		return VMM_ENODEV;
	}

	/* Is there a start attribute */
	if (vmm_devtree_read_u64(node,
		INITRD_START_ATTR_NAME, &initrd_start) != VMM_OK) {
		if (vmm_devtree_read_u64(node,
			INITRD_START_ATTR2_NAME, &initrd_start) != VMM_OK) {
			vmm_printf("initrd: %s/%s attribute not found\n",
				   INITRD_START_ATTR_NAME,
				   INITRD_START_ATTR2_NAME);
			goto error;
		}
	}

	/* If so is there also a end attribte */
	if (vmm_devtree_read_u64(node,
		INITRD_END_ATTR_NAME, &initrd_end) != VMM_OK) {
		if (vmm_devtree_read_u64(node,
			INITRD_END_ATTR2_NAME, &initrd_end) != VMM_OK) {
			vmm_printf("initrd: %s/%s attribute not found\n",
				   INITRD_END_ATTR_NAME,
				   INITRD_END_ATTR2_NAME);
			goto error;
		}
	}

	/* Let's do a little bit os sanity check */
	if (initrd_end <= initrd_start) {
		vmm_printf("initrd: error: initrd_start > initrd_end\n");
		goto error;
	}

	/* OK, we know where the initrd device is located */
	if ((initrd_rbd = rbd_create("initrd",
				(physical_addr_t)initrd_start,
				(physical_size_t)(initrd_end -
					  initrd_start), true))
				== NULL) {
		vmm_printf("initrd: rbd_create() failed\n");
		goto error;
	}

	vmm_printf("initrd: RBD created at 0x%llx - 0x%llx\n",
		   initrd_start, initrd_end);

 error:
	vmm_devtree_dref_node(node);

	return VMM_OK;
}

static void __exit initrd_driver_exit(void)
{
	initrd_rbd_destroy();
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
