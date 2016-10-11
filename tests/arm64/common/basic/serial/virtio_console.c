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
 * @file virtio_console.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Source file for Virtio Console serial port driver.
 */

#include <arm_io.h>
#include <serial/virtio_console.h>

void virtio_console_printch(physical_addr_t base, char ch)
{
	u32 tmp;
	struct virtio_console_config *p = (void *)base + VIRTIO_MMIO_CONFIG;

	tmp = arm_readl((void *)(base + VIRTIO_MMIO_DEVICE_ID));
	if (tmp != VIRTIO_ID_CONSOLE) {
		return;
	}

	tmp = arm_readl((void *)(base + VIRTIO_MMIO_HOST_FEATURES));
	if (!(tmp & (1 << VIRTIO_CONSOLE_F_EMERG_WRITE))) {
		return;
	}

	arm_writel(ch, &p->emerg_wr);
}

bool virtio_console_can_getch(physical_addr_t base)
{
	struct virtio_console_config *p = (void *)base + VIRTIO_MMIO_CONFIG;

	return ((tmp = arm_readl(&p->emerg_wr)) & (1 << 31)) ? TRUE : FALSE;
}

char virtio_console_getch(physical_addr_t base)
{
	u32 tmp;
	struct virtio_console_config *p = (void *)base + VIRTIO_MMIO_CONFIG;

	tmp = arm_readl((void *)(base + VIRTIO_MMIO_DEVICE_ID));
	if (tmp != VIRTIO_ID_CONSOLE) {
		return 0;
	}

	tmp = arm_readl((void *)(base + VIRTIO_MMIO_HOST_FEATURES));
	if (!(tmp & (1 << VIRTIO_CONSOLE_F_EMERG_WRITE))) {
		return 0;
	}

	while (!((tmp = arm_readl(&p->emerg_wr)) & (1 << 31))) ;

	return (char)(tmp & 0xFFU);
}

int virtio_console_init(physical_addr_t base)
{
	/* Nothing to do here. */
	return 0;
}

