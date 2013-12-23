/*
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @author: Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief: Architecture specific guest handling.
 */

#ifndef __ARCH_GUEST_HELPER_H_
#define __ARCH_GUEST_HELPER_H_

#include <cpu_vm.h>

/*! \brief x86 Guest private information
 *
 * This contains the private information for x86
 * specific guest.
 */
struct x86_guest_priv {
	struct page_table *g_npt; /**< Guest's nested page table */
};

/*!def x86_guest_priv(guest) is to access guest private information */
#define x86_guest_priv(guest) ((struct x86_guest_priv *)(guest->arch_priv))

#endif /* __ARCH_GUEST_HELPER_H_ */
