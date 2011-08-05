/**
 * Copyright (c) 2011 Himanshu Chauhan
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
 * @file vmm_cmd_elf.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of guest command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_elf.h>

#if 0
/* ======================================================================
 * A very simple elf loader, assumes the image is valid, returns the
 * entry point address.
 * ====================================================================== */
static unsigned long load_elf_image_phdr(virtual_addr_t addr)
{
	Elf32_Ehdr *ehdr;		/* Elf header structure pointer     */
	Elf32_Phdr *phdr;		/* Program header structure pointer */
	int i;

	ehdr = (Elf32_Ehdr *) addr;
	phdr = (Elf32_Phdr *) (addr + ehdr->e_phoff);

	/* Load each program header */
	for (i = 0; i < ehdr->e_phnum; ++i) {
		void *dst = (void *) phdr->p_paddr;
		void *src = (void *) addr + phdr->p_offset;
		vmm_printf("Loading phdr %i to 0x%p (%i bytes)\n",
			   i, dst, phdr->p_filesz);
		if (phdr->p_filesz)
			memcpy(dst, src, phdr->p_filesz);
		if (phdr->p_filesz != phdr->p_memsz)
			memset(dst + phdr->p_filesz, 0x00, phdr->p_memsz - phdr->p_filesz);
		++phdr;
		/* TODO: Flush caches here. On Qemu, we don't need it right now. */
	}

	vmm_printf("ELF Entry: 0x%X\n", ehdr->e_entry);

	return ehdr->e_entry;
}
#endif

static unsigned long load_elf_image_shdr(virtual_addr_t addr, virtual_addr_t gvaddr)
{
	Elf32_Ehdr *ehdr;		/* Elf header structure pointer     */
	Elf32_Shdr *shdr;		/* Section header structure pointer */
	unsigned char *strtab = 0;	/* String table pointer             */
	unsigned char *image;		/* Binary image pointer             */
	int i;				/* Loop counter                     */

	/* -------------------------------------------------- */

	ehdr = (Elf32_Ehdr *) addr;

	/* Find the section header string table for output info */
	shdr = (Elf32_Shdr *) (addr + ehdr->e_shoff +
			       (ehdr->e_shstrndx * sizeof (Elf32_Shdr)));

	if (shdr->sh_type == SHT_STRTAB)
		strtab = (unsigned char *) (addr + shdr->sh_offset);

	/* Load each appropriate section */
	for (i = 0; i < ehdr->e_shnum; ++i) {
		shdr = (Elf32_Shdr *) (addr + ehdr->e_shoff +
				       (i * sizeof (Elf32_Shdr)));

		if (!(shdr->sh_flags & SHF_ALLOC)
		    || shdr->sh_size == 0) {
			continue;
		}

		if (strtab) {
			vmm_printf("%sing %s @ 0x%08x (%d bytes)\n",
				   (shdr->sh_type == SHT_NOBITS) ?
				   "Clear" : "Load",
				   &strtab[shdr->sh_name],
				   (unsigned int) shdr->sh_addr,
				   (int) shdr->sh_size);
		}

		if (shdr->sh_type == SHT_NOBITS) {
			vmm_memset ((void *)shdr->sh_addr, 0, shdr->sh_size);
		} else {
			image = (unsigned char *) addr + shdr->sh_offset;
			vmm_memcpy ((void *) (gvaddr + shdr->sh_addr),
				    (const void *) image,
				    shdr->sh_size);
		}

		/* TODO: Flush caches here. On Qemu we don't need to do it right now. */
	}

	return ehdr->e_entry;
}

static int valid_elf_image (virtual_addr_t addr)
{
	Elf32_Ehdr *ehdr;		/* Elf header structure pointer */

	ehdr = (Elf32_Ehdr *) addr;

	if (!IS_ELF (*ehdr)) {
		vmm_printf ("Error: No elf image at address 0x%08lx\n", addr);
		return VMM_EFAIL;
	}

	if (ehdr->e_type != ET_EXEC) {
		vmm_printf ("Error: Not a 32-bit elf image at address 0x%08lx\n", addr);
		return 0;
	}

	return VMM_OK;
}

s32 vmm_elf_load(virtual_addr_t src_hvaddr, virtual_addr_t dest_gvaddr)
{
	if (valid_elf_image(src_hvaddr) == VMM_OK) {
		return load_elf_image_shdr(src_hvaddr, dest_gvaddr);
	}

	return -VMM_EFAIL;
}
