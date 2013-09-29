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

#define VIRT_V8_NOR_FLASH_0		(0x000000000)
#define VIRT_V8_NOR_FLASH_0_SIZE	(0x001000000)
#define VIRT_V8_GIC			(0x02c000000)
#define VIRT_V8_GIC_SIZE		(0x000003000)
#define VIRT_V8_VIRTIO_NET		(0x040200000)
#define VIRT_V8_VIRTIO_NET_SIZE		(0x000001000)
#define VIRT_V8_VIRTIO_BLK		(0x040400000)
#define VIRT_V8_VIRTIO_BLK_SIZE		(0x000001000)
#define VIRT_V8_VIRTIO_CON		(0x040600000)
#define VIRT_V8_VIRTIO_CON_SIZE		(0x000001000)
#define VIRT_V8_RAM0			(0x080000000)
#define VIRT_V8_RAM0_SIZE		(0x006000000)

#define VIRT_V8_GIC_DIST		(VIRT_V8_GIC + 0x1000)
#define	VIRT_V8_GIC_CPU			(VIRT_V8_GIC + 0x2000)

/*
 * Interrupts.
 */
#define IRQ_VIRT_V8_VIRT_TIMER		27
#define IRQ_VIRT_V8_PHYS_TIMER		30

#define IRQ_VIRT_V8_VIRTIO_NET		40
#define IRQ_VIRT_V8_VIRTIO_BLK		41
#define IRQ_VIRT_V8_VIRTIO_CON		42

#define IRQ_VIRT_V8_GIC_START		16
#define NR_IRQS_VIRT_V8			128
#define NR_GIC_VIRT_V8			1

#define IRQ_VIRT_TIMER			IRQ_VIRT_V8_VIRT_TIMER

#endif
