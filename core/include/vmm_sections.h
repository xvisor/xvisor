/**
 * Copyright (c) 2011 Pavel Borzenkov.
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
 * @author Pavel Borzenkov <pavel.borzenkov@gmail.com>
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief Architecture specific implementation of synchronization mechanisms.
 */

#ifndef __VMM_SECTIONS_H__
#define __VMM_SECTIONS_H__

#include <vmm_types.h>

#define __lock_section		__attribute__((section(".spinlock.text")))
#define __modtbl_section	__attribute__((section(".modtbl")))
#define __init_section		__attribute__((section(".init.text")))

#endif /* __VMM_SECTIONS_H__ */
