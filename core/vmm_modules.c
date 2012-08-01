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
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file of module managment code
 */

#include <arch_sections.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <list.h>

struct module_wrap {
	struct dlist head;
	struct vmm_module mod;
	bool built_in;
	virtual_addr_t start_va;
	u32 pg_count;
};

struct vmm_modules_ctrl {
	struct vmm_mutex lock;
	struct dlist mod_list;
        u32 mod_count;
};

static struct vmm_modules_ctrl modctrl;

bool vmm_modules_isbuiltin(struct vmm_module *mod)
{
	struct module_wrap *mwrap;

	if (!mod) {
		return FALSE;
	}

	mwrap = container_of(mod, struct module_wrap, mod);
	if (mwrap->mod.signature != VMM_MODULE_SIGNATURE) {
		return FALSE;
	}
	
	return (mwrap->built_in) ? TRUE : FALSE;
}

int vmm_modules_unload(struct vmm_module *mod)
{
	struct module_wrap *mwrap;

	if (!mod) {
		return VMM_EFAIL;
	}

	mwrap = container_of(mod, struct module_wrap, mod);
	if (mwrap->mod.signature != VMM_MODULE_SIGNATURE) {
		return VMM_EFAIL;
	}
	if (mwrap->built_in) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&modctrl.lock);

	if (mwrap->mod.exit) {
		mwrap->mod.exit();
	}
	list_del(&mwrap->head);
	vmm_host_free_pages(mwrap->start_va, mwrap->pg_count);
	vmm_free(mwrap);
	modctrl.mod_count--;

	vmm_mutex_unlock(&modctrl.lock);

	return VMM_OK;
}

struct vmm_module *vmm_modules_getmodule(u32 index)
{
	struct dlist *l;
	struct module_wrap *ret = NULL;

	vmm_mutex_lock(&modctrl.lock);
	if (0 <= index && index < modctrl.mod_count) {
		list_for_each(l, &modctrl.mod_list) {
			ret = list_entry(l, struct module_wrap, head);
			if (!index) {
				break;
			}
			index--;
		}
	}
	vmm_mutex_unlock(&modctrl.lock);

	return (ret) ? &ret->mod : NULL;
}

u32 vmm_modules_count(void)
{
	u32 ret;

	vmm_mutex_lock(&modctrl.lock);
	ret = modctrl.mod_count;
	vmm_mutex_unlock(&modctrl.lock);

	return ret;
}

int __init vmm_modules_init(void)
{
	int ret;
	u32 i, j, table_size, mod_count;
	struct module_wrap *mwrap;
	struct vmm_module *table;
	struct vmm_module tmp;

	/* Reset the control structure */
	vmm_memset(&modctrl, 0, sizeof(modctrl));
	INIT_MUTEX(&modctrl.lock);
	INIT_LIST_HEAD(&modctrl.mod_list);
	modctrl.mod_count = 0;

	/* Initialize the control structure */
	table = (struct vmm_module *) arch_modtbl_vaddr();
	table_size = arch_modtbl_size() / sizeof(struct vmm_module);

	/* Find and count valid built-in modules */
	for (i = 0; i < table_size; i++) {
		/* Check validity of module table entry */
		if (table[i].signature == VMM_MODULE_SIGNATURE) {
			/* Increment count in control structure */
			mod_count++;
		} else {
			break;
		}
	}

	/* If no built-in modules found then return */
	if (!mod_count) {
		return VMM_OK;
	}

	/* Sort built-in modules based on ipriority (Selection Sort) */
	for (i = 0; i < (mod_count - 1); i++) {
		for (j = (i + 1); j < mod_count; j++) {
			if (table[j].ipriority < table[i].ipriority) {
				vmm_memcpy(&tmp, &table[i], sizeof(tmp));
				vmm_memcpy(&table[i], &table[j], sizeof(tmp));
				vmm_memcpy(&table[j], &tmp, sizeof(tmp));
			}
		}
	}

	/* Initialize built-in modules in sorted order */
	for (i = 0; i < mod_count; i++) {
		mwrap = vmm_malloc(sizeof(struct module_wrap));
		if (!mwrap) {
			break;
		}

		vmm_memset(mwrap, 0, sizeof(struct module_wrap));
		INIT_LIST_HEAD(&mwrap->head);
		vmm_memcpy(&mwrap->mod, &table[i], sizeof(struct vmm_module));
		mwrap->built_in = TRUE;
		mwrap->start_va = 0;
		mwrap->pg_count = 0;

		/* Initialize module if required */
		if (mwrap->mod.init) {
#if defined(CONFIG_VERBOSE_MODE)
			vmm_printf("Initialize %s\n", mwrap->mod.name);
#endif
			if ((ret = mwrap->mod.init())) {
				vmm_printf("%s: %s init error %d\n", 
					   __func__, mwrap->mod.name, ret);
			}
			mwrap->mod.istatus = ret;
		}

		list_add_tail(&mwrap->head, &modctrl.mod_list);
		modctrl.mod_count++;
	}

	return VMM_OK;
}
