/**
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
 * @file cpu_elf.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief implementation of CPU specific elf functions
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <libs/elf.h>

int arch_elf_check_hdr(const struct elf64_hdr *x)
{
	/* Make sure it's an AARCH64 executable */
	if (x->e_machine != EM_AARCH64)
		return 0;

	/* Make sure the entry address is reasonable */
	if (x->e_entry & 3)
		return 0;

	/* Don't allow unknown ABI */
	if ((x->e_flags & EF_ARM_EABI_MASK) == EF_ARM_EABI_UNKNOWN) {
		return 0;
	}

	return 1;
}

int arch_elf_apply_relocate(struct elf64_shdr *sechdrs, 
			    const char *strtab, 
			    unsigned int symindex,
			    unsigned int relindex, 
			    struct vmm_module *mod)
{
	vmm_printf("module %s: RELOCATION unsupported\n", mod->name);
	return 0;
}

int arch_elf_apply_relocate_add(struct elf64_shdr *sechdrs, 
				const char *strtab,
				unsigned int symindex, 
				unsigned int relsec, 
				struct vmm_module *mod)
{
	vmm_printf("module %s: ADD RELOCATION unsupported\n", mod->name);
	return VMM_ENOEXEC;
}

