/*
 * Copyright (c) Himanshu Chauhan
 *
 *  APIC support - Common Interfaces
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>
 */
#ifndef _APIC_COMMON_H_
#define _APIC_COMMON_H_

#include <vmm_types.h>

#define SLAVE_IRQ_ENCODE(__dest, __dest_mode, __del_mode, __vector,	\
			 __trigger_mode)				\
	({								\
		u32 val;						\
		val = (((__del_mode & 0x7) << 8) |			\
		       ((__dest_mode & 0x1) << 9) |			\
		       ((__trigger_mode & 1) << 10) |			\
		       ((__vector & 0xff) << 11) |			\
		       ((__dest & 0xff) << 19));			\
			val;						\
	})

#define SLAVE_IRQ_DECODE(__level, __dest, __dest_mode, __del_mode, __vector, __tmode) \
	do {								\
		__dest = (((u32)__level >> 19) & 0xff);			\
		__vector = (((u32)__level >> 11) & 0xff);		\
		__tmode = (((u32)__level >> 10) & 0x1);			\
		__dest_mode = (((u32)__level >> 9) & 0x1);		\
		__del_mode = (((u32)__level >> 8) & 0x7);		\
	}while (0);

#endif /* _APIC_COMMON_H_ */
