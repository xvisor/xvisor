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
 * @file cpu_init.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief intialization functions for CPU
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_params.h>
#include <vmm_devtree.h>
#include <arch_cpu.h>
#include <cpu_inline_asm.h>

extern u8 _code_start;
extern u8 _code_end;
extern u32 _load_start;
extern u32 _load_end;

virtual_addr_t arch_code_vaddr_start(void)
{
	return (virtual_addr_t) &_code_start;
}

physical_addr_t arch_code_paddr_start(void)
{
	return (physical_addr_t) _load_start;
}

virtual_size_t arch_code_size(void)
{
	return (virtual_size_t) (&_code_end - &_code_start);
}

void arch_cpu_print_info(struct vmm_chardev *cdev)
{
	/* FIXME: To be implemented. */
}

int __init arch_cpu_early_init(void)
{
	char *attr;
	struct vmm_devtree_node *node;

	/*
	 * Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOSEN_NODE_NAME);
	if (!node) {
		return VMM_ENODEV;
	}

	attr = vmm_devtree_attrval(node, VMM_DEVTREE_BOOTARGS_ATTR_NAME);
	if (attr) {
		vmm_parse_early_options(attr);
	}

	return VMM_OK;
}

int __init arch_cpu_final_init(void)
{
	/* All VMM API's are available here */
	/* We can register a CPU specific resources here */

	return VMM_OK;
}

void __init cpu_init(void)
{
#ifndef CONFIG_ARMV5
	if (cpu_supports_fpu()) {
		/* Allow full-access to cp10 & cp11 if CPU supports FPU */
		write_cpacr(CPACR_CP_MASK(11) | CPACR_CP_MASK(10));
	}
#endif

	/* Initialize VMM (APIs only available after this) */
	vmm_init();

	/* We will never come back here. */
	vmm_hang();
}
