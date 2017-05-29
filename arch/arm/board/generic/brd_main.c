/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * for board information implementation.
 *
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_devtree.h>
#include <vmm_platform.h>
#include <vmm_stdio.h>
#include <arch_board.h>
#include <libs/vtemu.h>

#include <generic_board.h>

#include <linux/clk-provider.h>

/*
 * Global board context
 */

#if defined(CONFIG_VTEMU)
struct vtemu *generic_vt;
#endif

static const struct vmm_devtree_nodeid *generic_board_matches;

/*
 * Print board information
 */

static void generic_board_print_info(struct vmm_devtree_node *node,
				    const struct vmm_devtree_nodeid *match,
				    void *data)
{
	const struct generic_board *brd = match->data;
	struct vmm_chardev *cdev = data;

	if (!brd || !brd->print_info) {
		return;
	}

	brd->print_info(cdev);
}


void arch_board_print_info(struct vmm_chardev *cdev)
{
	if (generic_board_matches) {
		vmm_devtree_iterate_matching(NULL,
					     generic_board_matches,
					     generic_board_print_info,
					     cdev);
	}
}

/*
 * Initialization functions
 */

static void __init generic_board_early(struct vmm_devtree_node *node,
				       const struct vmm_devtree_nodeid *match,
				       void *data)
{
	int err;
	const struct generic_board *brd = match->data;

	if (!brd || !brd->early_init) {
		return;
	}

	err = brd->early_init(node);
	if (err) {
		vmm_printf("%s: Early init %s node failed (error %d)\n", 
			   __func__, node->name, err);
	}
}

int __init arch_board_early_init(void)
{
	/* Host aspace, Heap, Device tree, and Host IRQ available.
	 *
	 * Do necessary early stuff like:
	 * iomapping devices, 
	 * SOC clocking init, 
	 * Setting-up system data in device tree nodes,
	 * ....
	 */

	/* Determine generic board matches from nodeid table */
	generic_board_matches = 
		vmm_devtree_nidtbl_create_matches("generic_board");

	/* Early init of generic boards with 
	 * matching nodeid table enteries.
	 */
	if (generic_board_matches) {
		vmm_devtree_iterate_matching(NULL,
					     generic_board_matches,
					     generic_board_early,
					     NULL);
	}

	/* Initialize clocking framework */
	of_clk_init(NULL);

	return VMM_OK;
}

static void __init generic_board_final(struct vmm_devtree_node *node,
				       const struct vmm_devtree_nodeid *match,
				       void *data)
{
	int err;
	const struct generic_board *brd = match->data;

	if (!brd || !brd->final_init) {
		return;
	}

	err = brd->final_init(node);
	if (err) {
		vmm_printf("%s: Final init %s node failed (error %d)\n",
			   __func__, node->name, err);
	}
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;
	struct vmm_devtree_node *root;
	struct vmm_devtree_node *tmp_node;
#if defined(CONFIG_VTEMU)
	struct fb_info *info;
#endif

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	root = vmm_devtree_getnode("/");

	vmm_devtree_for_each_child(node, root) {
		/* check if node is compatible with "simple-bus" */
		tmp_node = vmm_devtree_find_compatible(node, NULL, "simple-bus");
		if (!tmp_node) {
			continue;
		}

		/* Do platform device probing using device driver framework */
		rc = vmm_platform_probe(tmp_node);
		vmm_devtree_dref_node(tmp_node);
		if (rc) {
			vmm_devtree_dref_node(node);
			return rc;
		}
	}

	vmm_devtree_dref_node(root);

	/* Create VTEMU instace if available */
#if defined(CONFIG_VTEMU)
	info = fb_find("fb0");
	if (info) {
		generic_vt = vtemu_create(info->name, info, NULL);
	}
#endif

	/* Final init of generic boards with 
	 * matching nodeid table enteries.
	 */
	if (generic_board_matches) {
		vmm_devtree_iterate_matching(NULL,
					     generic_board_matches,
					     generic_board_final,
					     NULL);
	}

	return VMM_OK;
}
