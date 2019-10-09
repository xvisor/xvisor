/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file vmm_virtio_input.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO Input Device Interface.
 *
 * This header has been derived from linux kernel source:
 * <linux_source>/include/uapi/linux/virtio_input.h
 *
 * The original header is BSD licensed.
 */

/* This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __VMM_VIRTIO_INPUT_H__
#define __VMM_VIRTIO_INPUT_H__

#include <vmm_types.h>

enum vmm_virtio_input_config_select {
	VMM_VIRTIO_INPUT_CFG_UNSET      = 0x00,
	VMM_VIRTIO_INPUT_CFG_ID_NAME    = 0x01,
	VMM_VIRTIO_INPUT_CFG_ID_SERIAL  = 0x02,
	VMM_VIRTIO_INPUT_CFG_ID_DEVIDS  = 0x03,
	VMM_VIRTIO_INPUT_CFG_PROP_BITS  = 0x10,
	VMM_VIRTIO_INPUT_CFG_EV_BITS    = 0x11,
	VMM_VIRTIO_INPUT_CFG_ABS_INFO   = 0x12,
};

struct vmm_virtio_input_absinfo {
	u32 min;
	u32 max;
	u32 fuzz;
	u32 flat;
	u32 res;
};

struct vmm_virtio_input_devids {
	u16 bustype;
	u16 vendor;
	u16 product;
	u16 version;
};

struct vmm_virtio_input_config {
	u8    select;
	u8    subsel;
	u8    size;
	u8    reserved[5];
	union {
		char string[128];
		u8 bitmap[128];
		struct vmm_virtio_input_absinfo abs;
		struct vmm_virtio_input_devids ids;
	} u;
}__attribute__((packed));

struct vmm_virtio_input_event {
	u16 type;
	u16 code;
	u32 value;
}__attribute__((packed));

#endif /* __VMM_VIRTIO_INPUT_H__ */
