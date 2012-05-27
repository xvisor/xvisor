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
 * @file arch_guest.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific Guest operations
 */
#ifndef _ARCH_GUEST_H__
#define _ARCH_GUEST_H__

#include <vmm_types.h>
#include <vmm_manager.h>

/** Guest functions required by VMM core */
int arch_guest_init(struct vmm_guest * guest);
int arch_guest_deinit(struct vmm_guest * guest);

#endif
