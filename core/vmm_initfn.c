/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_initfn.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for device-tree based init functions
 */

#include <vmm_error.h>
#include <vmm_initfn.h>

static void __init initfn_nidtbl_found(struct vmm_devtree_node *node,
					const struct vmm_devtree_nodeid *match,
					void *data)
{
	int err;
	vmm_initfn_t init_fn = match->data;

	if (!init_fn) {
		return;
	}

	err = init_fn(node);
#ifdef CONFIG_VERBOSE_MODE
	if (err) {
		vmm_printf("%s: CPU%d Init %s node failed (error %d)\n",
			   __func__, vmm_smp_processor_id(), node->name, err);
	}
#else
	(void)err;
#endif
}

static int initfn_do(const char *subsys)
{
	const struct vmm_devtree_nodeid *matches;

	matches = vmm_devtree_nidtbl_create_matches(subsys);
	if (!matches) {
		return VMM_OK;
	}

	vmm_devtree_iterate_matching(NULL, matches,
				     initfn_nidtbl_found, NULL);

	if (matches) {
		vmm_devtree_nidtbl_destroy_matches(matches);
	}

	return VMM_OK;
}

int __init vmm_initfn_early(void)
{
	return initfn_do("initfn_early");
}

int __init vmm_initfn_final(void)
{
	return initfn_do("initfn_final");
}
