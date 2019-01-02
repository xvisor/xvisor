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
 * @file riscv_plat.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Platform Configuration for virt board
 */
#ifndef __RISCV_PLAT_H__
#define __RISCV_PLAT_H__

#define VIRT_NOR_FLASH			(0x00000000)
#define VIRT_NOR_FLASH_SIZE		(0x02000000)
#define VIRT_PLIC			(0x0c000000)
#define VIRT_PLIC_SIZE			(0x04000000)
#define VIRT_UART0			(0x10000000)
#define VIRT_VMINFO			(0x10001000)
#define VIRT_SIMPLEFB			(0x10002000)
#define VIRT_VIRTIO_NET			(0x20000000)
#define VIRT_VIRTIO_NET_SIZE		(0x00001000)
#define VIRT_VIRTIO_BLK			(0x20001000)
#define VIRT_VIRTIO_BLK_SIZE		(0x00001000)
#define VIRT_VIRTIO_CON			(0x20002000)
#define VIRT_VIRTIO_CON_SIZE		(0x00001000)
#define VIRT_PCI			(0x30000000)
#define VIRT_PCI_SIZE			(0x20000000)
#define VIRT_RAM0			(0x80000000)
#define VIRT_RAM0_SIZE			(0x06000000)

/*
 * Interrupts.
 */
#define IRQ_VIRT_UART0			10
#define IRQ_VIRT_VIRTIO_NET		1
#define IRQ_VIRT_VIRTIO_BLK		2
#define IRQ_VIRT_VIRTIO_CON		3

#define VIRT_PLIC_NUM_SOURCES		127
#define VIRT_PLIC_NUM_PRIORITIES	7

#endif
