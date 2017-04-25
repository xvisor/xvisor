/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file vmm_virtio_console.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO Console Device Interface.
 *
 * This header has been derived from linux kernel source:
 * <linux_source>/include/uapi/linux/virtio_console.h
 *
 * The original header is BSD licensed. 
 */

/*
 * This header, excluding the #ifdef __KERNEL__ part, is BSD licensed so
 * anyone can use the definitions to implement compatible drivers/servers:
 *
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
 *
 * Copyright (C) Red Hat, Inc., 2009, 2010, 2011
 * Copyright (C) Amit Shah <amit.shah@redhat.com>, 2009, 2010, 2011
 */

#ifndef __VMM_VIRTIO_CONSOLE_H__
#define __VMM_VIRTIO_CONSOLE_H__

#include <vmm_types.h>

/* Feature bits */
#define VMM_VIRTIO_CONSOLE_F_SIZE		0 /* Does host provide console size? */
#define VMM_VIRTIO_CONSOLE_F_MULTIPORT		1 /* Does host provide multiple ports? */
#define VMM_VIRTIO_CONSOLE_F_EMERG_WRITE	2 /* Does host support emergency write? */

#define VMM_VIRTIO_CONSOLE_BAD_ID		(~(u32)0)

struct vmm_virtio_console_config {
	/* colums of the screens */
	u16 cols;
	/* rows of the screens */
	u16 rows;
	/* max. number of ports this device can hold */
	u32 max_nr_ports;
	/* emergency write register */
	u32 emerg_wr;
} __attribute__((packed));

/*
 * A message that's passed between the Host and the Guest for a
 * particular port.
 */
struct vmm_virtio_console_control {
	u32 id;		/* Port number */
	u16 event;		/* The kind of control event (see below) */
	u16 value;		/* Extra information for the key */
};

/* Some events for control messages */
#define VMM_VIRTIO_CONSOLE_DEVICE_READY		0
#define VMM_VIRTIO_CONSOLE_PORT_ADD		1
#define VMM_VIRTIO_CONSOLE_PORT_REMOVE		2
#define VMM_VIRTIO_CONSOLE_PORT_READY		3
#define VMM_VIRTIO_CONSOLE_CONSOLE_PORT		4
#define VMM_VIRTIO_CONSOLE_RESIZE		5
#define VMM_VIRTIO_CONSOLE_PORT_OPEN		6
#define VMM_VIRTIO_CONSOLE_PORT_NAME		7

#endif /* __VMM_VIRTIO_CONSOLE_H__ */
