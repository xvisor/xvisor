/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file vmm_sections.h
 * @version 0.01
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief Architecture specific implementation of synchronization mechanisms.
 */

#ifndef __VMM_SECTIONS_H__
#define __VMM_SECTIONS_H__

#include <vmm_types.h>

extern char _heap_start;
extern char _heap_end;
extern char _cmdtbl_start;
extern char _cmdtbl_end;
extern char _modtbl_start;
extern char _modtbl_end;
extern char _mm_hk_start;
extern char _mm_hk_end;

#define __lock_section		__attribute__((section(".spinlock.text")))
#define __cmdtbl_section	__attribute__((section(".cmdtbl")))
#define __modtbl_section	__attribute__((section(".modtbl")))

static inline virtual_addr_t vmm_mm_hk_start(void)
{
        return (virtual_addr_t)&_mm_hk_start;
}

static inline virtual_size_t vmm_mm_hk_size(void)
{
        return (virtual_size_t)(&_mm_hk_end - &_mm_hk_start);
}

static inline virtual_addr_t vmm_modtbl_start(void)
{
	return (virtual_addr_t) & _modtbl_start;
}

static inline virtual_size_t vmm_modtbl_size(void)
{
	return (virtual_size_t) (&_modtbl_end - &_modtbl_start);
}

static inline virtual_addr_t vmm_cmdtbl_start(void)
{
	return (virtual_addr_t) & _cmdtbl_start;
}

static inline virtual_size_t vmm_cmdtbl_size(void)
{
	return (virtual_size_t) (&_cmdtbl_end - &_cmdtbl_start);
}

static inline virtual_addr_t vmm_heap_start(void)
{
	return (virtual_addr_t) & _heap_start;
}

static inline virtual_size_t vmm_heap_size(void)
{
	return (virtual_addr_t) (&_heap_end - &_heap_start);
}

#endif /* __VMM_SECTIONS_H__ */
