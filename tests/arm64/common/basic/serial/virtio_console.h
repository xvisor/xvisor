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
 * @file virtio_console.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Header file for Virtio Console serial port driver.
 */

#ifndef __VIRTIO_CONSOLE_H_
#define __VIRTIO_CONSOLE_H_

#include <arm_types.h>

/* VirtIO MMIO */
#define VIRTIO_MMIO_DEVICE_ID		0x008
#define VIRTIO_MMIO_HOST_FEATURES	0x010
#define VIRTIO_MMIO_CONFIG		0x100

/* VirtIO Console */
#define VIRTIO_ID_CONSOLE		3

/* VirtIO Console Feature bits */
#define VIRTIO_CONSOLE_F_SIZE		0
#define VIRTIO_CONSOLE_F_MULTIPORT 	1
#define VIRTIO_CONSOLE_F_EMERG_WRITE 	2

struct virtio_console_config {
	/* colums of the screens */
	u16 cols;
	/* rows of the screens */
	u16 rows;
	/* max. number of ports this device can hold */
	u32 max_nr_ports;
	/* emergency write register */
	u32 emerg_wr;
} __attribute__((packed));

void virtio_console_printch(physical_addr_t base, char ch);
bool virtio_console_can_getch(physical_addr_t base);
char virtio_console_getch(physical_addr_t base);
int virtio_console_init(physical_addr_t base);

#endif /* __VIRTIO_CONSOLE_H_ */
