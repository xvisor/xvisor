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
 * @file arch_sections.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief interface for accessing VMM sections
 */
#ifndef _ARCH_SECTIONS_H__
#define _ARCH_SECTIONS_H__

#include <vmm_types.h>

/** Overall code */
virtual_addr_t arch_code_vaddr_start(void);
physical_addr_t arch_code_paddr_start(void);
virtual_size_t arch_code_size(void);

/** Exception table */
extern u8 __start___ex_table;
extern u8 __stop___ex_table;
static inline virtual_addr_t arch_extable_start(void)
{
	return (virtual_addr_t)&__start___ex_table;
}
static inline virtual_addr_t arch_extable_end(void)
{
	return (virtual_size_t)&__stop___ex_table;
}

/** Module table */
extern u8 _modtbl_start;
extern u8 _modtbl_end;
static inline virtual_addr_t arch_modtbl_vaddr(void)
{
	return (virtual_addr_t) &_modtbl_start;
}
static inline virtual_size_t arch_modtbl_size(void)
{
	return (virtual_size_t) (&_modtbl_end - &_modtbl_start);
}

/** PerCPU section */
extern u8 _percpu_start;
extern u8 _percpu_end;
static inline virtual_addr_t arch_percpu_vaddr(void)
{
	return (virtual_addr_t) &_percpu_start;
}
static inline virtual_size_t arch_percpu_size(void)
{
	return (virtual_size_t) (&_percpu_end - &_percpu_start);
}

/** Init section */
extern u8 _init_start;
extern u8 _init_end;
static inline virtual_addr_t arch_init_vaddr(void)
{
	return (virtual_addr_t) (&_init_start);
}
static inline virtual_size_t arch_init_size(void)
{
	return (virtual_size_t) (&_init_end - &_init_start);
}

/** Device tree nodeid table */
extern u8 _nidtbl_start;
extern u8 _nidtbl_end;
static inline virtual_addr_t arch_nidtbl_vaddr(void)
{
	return (virtual_addr_t) &_nidtbl_start;
}
static inline virtual_size_t arch_nidtbl_size(void)
{
	return (virtual_size_t) (&_nidtbl_end - &_nidtbl_start);
}

#endif
