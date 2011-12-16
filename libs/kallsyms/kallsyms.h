/**
 * Copyright (c) 2011 Jean-Christophe Dubois
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
 * @file kallsyms.h
 * @version 1.0
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief header file for kallsyms related functions
 */

#ifndef __KALLSYMS_H_
#define __KALLSYMS_H_

/*
 * Max supported symbol length
 */
#define KSYM_NAME_LEN 128

/*
 * Tell the compiler that the count isn't in the small data section if the arch
 * has one (eg: FRV).
 */
extern const unsigned long kallsyms_num_syms
    __attribute__ ((weak, section(".rodata")));
extern const unsigned long kallsyms_addresses[] __attribute__ ((weak));
extern const unsigned char kallsyms_names[] __attribute__ ((weak));
extern const unsigned char kallsyms_token_table[] __attribute__ ((weak));
extern const unsigned short kallsyms_token_index[] __attribute__ ((weak));
extern const unsigned long kallsyms_markers[] __attribute__ ((weak));

/* Lookup the address for a symbol. Returns 0 if not found. */
unsigned long kallsyms_lookup_name(const char *name);

unsigned long kallsyms_get_symbol_pos(unsigned long addr, unsigned long *symbolsize, unsigned long *offset);

/* Call a function on each kallsyms symbol in the core kernel */
int kallsyms_on_each_symbol(int (*fn) (void *, const char *, unsigned long),
			    void *data);

int kallsyms_lookup_size_offset(unsigned long addr,
				unsigned long *symbolsize,
				unsigned long *offset);

/* Lookup an address. */
const char *kallsyms_lookup(unsigned long addr,
			    unsigned long *symbolsize,
			    unsigned long *offset, char *namebuf);

/* Look up a kernel symbol and return it in a text buffer. */
int kallsyms_sprint_symbol(char *buffer, unsigned long address);
int kallsyms_sprint_backtrace(char *buffer, unsigned long address);

int kallsyms_lookup_symbol_name(unsigned long addr, char *symname);
int kallsyms_lookup_symbol_attrs(unsigned long addr, unsigned long *size,
				 unsigned long *offset, char *name);

#endif
