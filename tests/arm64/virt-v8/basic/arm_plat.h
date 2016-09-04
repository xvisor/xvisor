/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file arm_plat.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Platform Configuration for virt-v8 board
 */
#ifndef __ARM_PLAT_H__
#define __ARM_PLAT_H__

#define VIRT_V8_NOR_FLASH		(0x00000000)
#define VIRT_V8_NOR_FLASH_SIZE		(0x01000000)
#define VIRT_V8_GIC			(0x08000000)
#define VIRT_V8_GIC_SIZE		(0x00020000)
#define VIRT_V8_UART0			(0x09000000)
#define VIRT_V8_VMINFO			(0x09001000)
#define VIRT_V8_SIMPLEFB		(0x09002000)
#define VIRT_V8_VIRTIO_NET		(0x0A000000)
#define VIRT_V8_VIRTIO_NET_SIZE		(0x00001000)
#define VIRT_V8_VIRTIO_BLK		(0x0A001000)
#define VIRT_V8_VIRTIO_BLK_SIZE		(0x00001000)
#define VIRT_V8_VIRTIO_CON		(0x0A002000)
#define VIRT_V8_VIRTIO_CON_SIZE		(0x00001000)
#define VIRT_V8_PCI			(0x10000000)
#define VIRT_V8_PCI_SIZE		(0x30000000)
#define VIRT_V8_RAM0			(0x40000000)
#define VIRT_V8_RAM0_SIZE		(0x06000000)

#define VIRT_V8_GIC_DIST		(VIRT_V8_GIC + 0x00000)
#define	VIRT_V8_GIC_CPU			(VIRT_V8_GIC + 0x10000)

/*
 * Interrupts.
 */
#define IRQ_VIRT_V8_VIRT_TIMER		27
#define IRQ_VIRT_V8_PHYS_TIMER		30

#define IRQ_VIRT_V8_UART0		33
#define IRQ_VIRT_V8_VIRTIO_NET		48
#define IRQ_VIRT_V8_VIRTIO_BLK		49
#define IRQ_VIRT_V8_VIRTIO_CON		50

#define IRQ_VIRT_V8_GIC_START		16
#define NR_IRQS_VIRT_V8			128
#define NR_GIC_VIRT_V8			1

#define IRQ_VIRT_TIMER			IRQ_VIRT_V8_VIRT_TIMER

#endif
