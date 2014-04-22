/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_error.h
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief header file for VMM error codes
 */
#ifndef _VMM_ERROR_H__
#define _VMM_ERROR_H__

#define VMM_OK			0
#define VMM_EFAIL		-1
#define VMM_EUNKNOWN		-2
#define VMM_ENOTAVAIL		-3
#define VMM_EALREADY		-4
#define VMM_EINVALID		-5
#define VMM_EOVERFLOW		-6
#define VMM_ENOMEM		-7
#define VMM_ENODEV		-8
#define VMM_EBUSY		-9
#define VMM_EEXIST		-10
#define VMM_ETIMEDOUT		-11
#define VMM_EACCESS		-12
#define VMM_ENOEXEC		-13
#define VMM_ENOENT		-14
#define VMM_ENOSYS		-15
#define VMM_EIO			-16
#define VMM_ETIME		-17
#define VMM_ERANGE		-18
#define VMM_EILSEQ		-19
#define VMM_EOPNOTSUPP		-20
#define VMM_ENOSPC		-21
#define VMM_ENODATA		-22
#define VMM_EFAULT		-23
#define VMM_ENXIO		-24
#define VMM_EPROTONOSUPPORT	-25
#define VMM_EPROBE_DEFER	-26

#endif
