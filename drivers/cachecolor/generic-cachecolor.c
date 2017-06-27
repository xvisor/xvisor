/**
 * Copyright (c) 2017 Paolo Modica.
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
 * @file generic-cachecolor.c
 * @author Paolo Modica <p.modica90@gmail.com>
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic cache color driver.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_initfn.h>
#include <vmm_host_ram.h>
#include <libs/bitops.h>

struct generic_cachecolor {
	u32 first_color_bit;
	u32 num_color_bits;
	u32 color_order;
};

static u32 generic_num_colors(void *priv)
{
	struct generic_cachecolor *cc = priv;

	return 1 << cc->num_color_bits;
}

static u32 generic_color_order(void *priv)
{
	struct generic_cachecolor *cc = priv;

	return cc->color_order;
}

static bool generic_color_match(physical_addr_t pa, physical_size_t sz,
				u32 color, void *priv)
{
	struct generic_cachecolor *cc = priv;
	u32 color_mask = (1 << cc->num_color_bits) - 1;
	u32 color_num = (pa >> cc->first_color_bit) & color_mask;
	physical_size_t color_sz = (physical_size_t)1 << cc->color_order;

	if (sz != color_sz)
		return FALSE;

	if (color != color_num)
		return FALSE;

	return TRUE;
}

static struct vmm_host_ram_color_ops generic_cachecolor_ops = {
	.name = "generic-cachecolor",
	.num_colors = generic_num_colors,
	.color_order = generic_color_order,
	.color_match = generic_color_match,
};

static int __init generic_cachecolor_init(struct vmm_devtree_node *node)
{
	struct generic_cachecolor *cc;

	cc = vmm_zalloc(sizeof(*cc));
	if (!cc) {
		return VMM_ENOMEM;
	}

	if (vmm_devtree_read_u32(node, "first_color_bit",
				 &cc->first_color_bit)) {
		vmm_free(cc);
		return VMM_EINVALID;
	}

	if (vmm_devtree_read_u32(node, "num_color_bits",
				 &cc->num_color_bits)) {
		vmm_free(cc);
		return VMM_EINVALID;
	}

	if (vmm_devtree_read_u32(node, "color_order",
				 &cc->color_order)) {
		vmm_free(cc);
		return VMM_EINVALID;
	}

	if (BITS_PER_LONG <= cc->color_order) {
		vmm_free(cc);
		return VMM_ENODEV;
	}

	if (BITS_PER_LONG <= cc->first_color_bit) {
		vmm_free(cc);
		return VMM_ENODEV;
	}

	if (BITS_PER_LONG <= (cc->first_color_bit + cc->num_color_bits)) {
		vmm_free(cc);
		return VMM_ENODEV;
	}

	vmm_host_ram_set_color_ops(&generic_cachecolor_ops, cc);

	return VMM_OK;
}
VMM_INITFN_DECLARE_EARLY(gcachecolor,
			 "generic,cachecolor",
			 generic_cachecolor_init);

