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
#define VMM_ESHUTDOWN		-27
#define VMM_EREMOTEIO		-28
#define VMM_EINPROGRESS		-29
#define VMM_EROFS		-30	/* Read-only file system */
#define VMM_EBADMSG		-31	/* Not a data message */
#define VMM_EUCLEAN		-32	/* Structure needs cleaning */
#define VMM_ENOTSUPP		-33
#define VMM_EAGAIN		-34
#define VMM_EPROTO		-35	/* Protocol error */

#define VMM_MAX_ERRNO		4095

#define VMM_IS_ERR_VALUE(x)						\
	((x) && ((unsigned long)(x) <= (unsigned long)VMM_MAX_ERRNO))

static inline void *VMM_ERR_PTR(long error)
{
	if (0 < (-error) && (-error) < VMM_MAX_ERRNO)
		return (void *) (-error);
	return (void *)0;
}

static inline long VMM_PTR_ERR(const void *ptr)
{
	return (ptr) ? -((long)ptr) : VMM_EFAIL;
}

static inline long VMM_IS_ERR(const void *ptr)
{
	return VMM_IS_ERR_VALUE((unsigned long)ptr);
}

static inline long VMM_IS_ERR_OR_NULL(const void *ptr)
{
	return !ptr || VMM_IS_ERR_VALUE((unsigned long)ptr);
}

static inline void *VMM_ERR_CAST(const void *ptr)
{
	/* cast away the const */
	return (void *) ptr;
}

static inline int VMM_PTR_RET(const void *ptr)
{
	if (VMM_IS_ERR(ptr))
		return VMM_PTR_ERR(ptr);
	else
		return 0;
}

#endif
