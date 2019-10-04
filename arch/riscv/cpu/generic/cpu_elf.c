/**
 * Copyright (c) 2018 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief implementation of CPU specific elf functions
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <libs/elf.h>

#ifdef CONFIG_64BIT
int arch_elf_check_hdr(const struct elf64_hdr *x)
#else
int arch_elf_check_hdr(const struct elf32_hdr *x)
#endif
{
	/* TODO: */
	return 0;
}

#ifdef CONFIG_64BIT
int arch_elf_apply_relocate(struct elf64_shdr *sechdrs,
#else
int arch_elf_apply_relocate(struct elf32_shdr *sechdrs,
#endif
			    const char *strtab,
			    unsigned int symindex,
			    unsigned int relindex,
			    struct vmm_module *mod)
{
	/* TODO: */
	vmm_printf("module %s: RELOCATION unsupported\n", mod->name);
	return 0;
}

#ifdef CONFIG_64BIT
int arch_elf_apply_relocate_add(struct elf64_shdr *sechdrs,
#else
int arch_elf_apply_relocate_add(struct elf32_shdr *sechdrs,
#endif
				const char *strtab,
				unsigned int symindex,
				unsigned int relsec,
				struct vmm_module *mod)
{
	/* TODO: */
	vmm_printf("module %s: ADD RELOCATION unsupported\n", mod->name);
	return VMM_ENOEXEC;
}
