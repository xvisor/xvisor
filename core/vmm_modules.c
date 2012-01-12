/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_modules.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file of module managment code
 */

#include <arch_cpu.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_modules.h>

struct vmm_modules_ctrl {
        struct vmm_module *table;
        u32 table_size;
        u32 mod_count;
};

static struct vmm_modules_ctrl modules_ctrl;

struct vmm_module *vmm_modules_getmodule(u32 index)
{
	if (0 <= index && index < modules_ctrl.mod_count) {
		return &modules_ctrl.table[index];
	}
	return NULL;
}

u32 vmm_modules_count(void)
{
	return modules_ctrl.mod_count;
}

int __init vmm_modules_init(void)
{
	int mod_ret;
	u32 i, j;
	struct vmm_module tmpmod;

	/* Reset the control structure */
	vmm_memset(&modules_ctrl, 0, sizeof(modules_ctrl));

	/* Initialize the control structure */
	modules_ctrl.table = (struct vmm_module *) arch_modtbl_vaddr();
	modules_ctrl.table_size = arch_modtbl_size() / sizeof(struct vmm_module);
	modules_ctrl.mod_count = 0;

	/* Find and count valid modules */
	for (i = 0; i < modules_ctrl.table_size; i++) {
		/* Check validity of command table entry */
		if (modules_ctrl.table[i].signature == VMM_MODULE_SIGNATURE) {
			/* Increment count in control structure */
			modules_ctrl.mod_count++;
		} else {
			break;
		}
	}

	/* If no modules found then return */
	if (!modules_ctrl.mod_count) {
		return VMM_OK;
	}

	/* Sort modules based on initialization priority (Selection Sort) */
	for (i = 0; i < (modules_ctrl.mod_count - 1); i++) {
		for (j = (i + 1); j < modules_ctrl.mod_count; j++) {
			if (modules_ctrl.table[j].ipriority <
			    modules_ctrl.table[i].ipriority) {
				vmm_memcpy(&tmpmod,
					   &modules_ctrl.table[i],
					   sizeof(tmpmod));
				vmm_memcpy(&modules_ctrl.table[i],
					   &modules_ctrl.table[j],
					   sizeof(modules_ctrl.table[i]));
				vmm_memcpy(&modules_ctrl.table[j],
					   &tmpmod,
					   sizeof(modules_ctrl.table[j]));
			}
		}
	}

	/* Initialize modules in sorted order */
	for (i = 0; i < modules_ctrl.mod_count; i++) {
		/* Initialize module if required */
		if (modules_ctrl.table[i].init) {
#if defined(CONFIG_VERBOSE_MODE)
			vmm_printf("Initialize %s\n",
				   modules_ctrl.table[i].name);
#endif
			mod_ret = modules_ctrl.table[i].init();
			if (mod_ret) {
				vmm_printf("%s: %s init error %d\n", 
				__func__, modules_ctrl.table[i].name, mod_ret);
			}
			modules_ctrl.table[i].istatus = mod_ret;
		}
	}

	return VMM_OK;
}
