/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file arch_elf.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief arch specific ELF routines
 */
#ifndef _ARCH_ELF_H__
#define _ARCH_ELF_H__

#include <vmm_types.h>

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS64
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_X86_64

struct elf64_hdr;
struct elf64_shdr;

int arch_elf_check_hdr(const struct elf64_hdr *x);

int arch_elf_apply_relocate(struct elf64_shdr *sechdrs, 
			    const char *strtab, 
			    unsigned int symindex,
			    unsigned int relindex, 
			    struct vmm_module *mod);

int arch_elf_apply_relocate_add(struct elf64_shdr *sechdrs, 
				const char *strtab,
				unsigned int symindex, 
				unsigned int relsec, 
				struct vmm_module *mod);

#endif
