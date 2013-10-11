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
 * @file bcm2835_intc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2835 intc interface
 */

#ifndef __BCM2835_INTC_H
#define __BCM2835_INTC_H

#include <vmm_types.h>

/** Maximum number of IRQs in bcm2835 intc */
#define BCM2835_INTC_MAX_IRQ		96

#define ARM_IRQ0_BASE			0
#define INTERRUPT_ARM_TIMER		(ARM_IRQ0_BASE + 0)
#define INTERRUPT_ARM_MAILBOX		(ARM_IRQ0_BASE + 1)
#define INTERRUPT_ARM_DOORBELL_0 	(ARM_IRQ0_BASE + 2)
#define INTERRUPT_ARM_DOORBELL_1 	(ARM_IRQ0_BASE + 3)
#define INTERRUPT_VPU0_HALTED		(ARM_IRQ0_BASE + 4)
#define INTERRUPT_VPU1_HALTED		(ARM_IRQ0_BASE + 5)
#define INTERRUPT_ILLEGAL_TYPE0		(ARM_IRQ0_BASE + 6)
#define INTERRUPT_ILLEGAL_TYPE1		(ARM_IRQ0_BASE + 7)
#define INTERRUPT_PENDING1		(ARM_IRQ0_BASE + 8)
#define INTERRUPT_PENDING2		(ARM_IRQ0_BASE + 9)
#define INTERRUPT_JPEG			(ARM_IRQ0_BASE + 10)
#define INTERRUPT_USB			(ARM_IRQ0_BASE + 11)
#define INTERRUPT_3D			(ARM_IRQ0_BASE + 12)
#define INTERRUPT_DMA2			(ARM_IRQ0_BASE + 13)
#define INTERRUPT_DMA3			(ARM_IRQ0_BASE + 14)
#define INTERRUPT_I2C 			(ARM_IRQ0_BASE + 15)
#define INTERRUPT_SPI 			(ARM_IRQ0_BASE + 16)
#define INTERRUPT_I2SPCM		(ARM_IRQ0_BASE + 17)
#define INTERRUPT_SDIO			(ARM_IRQ0_BASE + 18)
#define INTERRUPT_UART			(ARM_IRQ0_BASE + 19)
#define INTERRUPT_ARASANSDIO		(ARM_IRQ0_BASE + 20)

#define ARM_IRQ1_BASE			32
#define INTERRUPT_TIMER0		(ARM_IRQ1_BASE + 0)
#define INTERRUPT_TIMER1		(ARM_IRQ1_BASE + 1)
#define INTERRUPT_TIMER2		(ARM_IRQ1_BASE + 2)
#define INTERRUPT_TIMER3		(ARM_IRQ1_BASE + 3)
#define INTERRUPT_CODEC0		(ARM_IRQ1_BASE + 4)
#define INTERRUPT_CODEC1		(ARM_IRQ1_BASE + 5)
#define INTERRUPT_CODEC2		(ARM_IRQ1_BASE + 6)
#define INTERRUPT_VC_JPEG		(ARM_IRQ1_BASE + 7)
#define INTERRUPT_ISP			(ARM_IRQ1_BASE + 8)
#define INTERRUPT_VC_USB		(ARM_IRQ1_BASE + 9)
#define INTERRUPT_VC_3D			(ARM_IRQ1_BASE + 10)
#define INTERRUPT_TRANSPOSER		(ARM_IRQ1_BASE + 11)
#define INTERRUPT_MULTICORESYNC0 	(ARM_IRQ1_BASE + 12)
#define INTERRUPT_MULTICORESYNC1 	(ARM_IRQ1_BASE + 13)
#define INTERRUPT_MULTICORESYNC2 	(ARM_IRQ1_BASE + 14)
#define INTERRUPT_MULTICORESYNC3 	(ARM_IRQ1_BASE + 15)
#define INTERRUPT_DMA0			(ARM_IRQ1_BASE + 16)
#define INTERRUPT_DMA1			(ARM_IRQ1_BASE + 17)
#define INTERRUPT_VC_DMA2		(ARM_IRQ1_BASE + 18)
#define INTERRUPT_VC_DMA3		(ARM_IRQ1_BASE + 19)
#define INTERRUPT_DMA4			(ARM_IRQ1_BASE + 20)
#define INTERRUPT_DMA5			(ARM_IRQ1_BASE + 21)
#define INTERRUPT_DMA6			(ARM_IRQ1_BASE + 22)
#define INTERRUPT_DMA7			(ARM_IRQ1_BASE + 23)
#define INTERRUPT_DMA8			(ARM_IRQ1_BASE + 24)
#define INTERRUPT_DMA9			(ARM_IRQ1_BASE + 25)
#define INTERRUPT_DMA10			(ARM_IRQ1_BASE + 26)
#define INTERRUPT_DMA11			(ARM_IRQ1_BASE + 27)
#define INTERRUPT_DMA12			(ARM_IRQ1_BASE + 28)
#define INTERRUPT_AUX			(ARM_IRQ1_BASE + 29)
#define INTERRUPT_ARM			(ARM_IRQ1_BASE + 30)
#define INTERRUPT_VPUDMA		(ARM_IRQ1_BASE + 31)

#define ARM_IRQ2_BASE			64
#define INTERRUPT_HOSTPORT		(ARM_IRQ2_BASE + 0)
#define INTERRUPT_VIDEOSCALER		(ARM_IRQ2_BASE + 1)
#define INTERRUPT_CCP2TX		(ARM_IRQ2_BASE + 2)
#define INTERRUPT_SDC			(ARM_IRQ2_BASE + 3)
#define INTERRUPT_DSI0			(ARM_IRQ2_BASE + 4)
#define INTERRUPT_AVE			(ARM_IRQ2_BASE + 5)
#define INTERRUPT_CAM0			(ARM_IRQ2_BASE + 6)
#define INTERRUPT_CAM1			(ARM_IRQ2_BASE + 7)
#define INTERRUPT_HDMI0			(ARM_IRQ2_BASE + 8)
#define INTERRUPT_HDMI1			(ARM_IRQ2_BASE + 9)
#define INTERRUPT_PIXELVALVE1		(ARM_IRQ2_BASE + 10)
#define INTERRUPT_I2CSPISLV		(ARM_IRQ2_BASE + 11)
#define INTERRUPT_DSI1			(ARM_IRQ2_BASE + 12)
#define INTERRUPT_PWA0			(ARM_IRQ2_BASE + 13)
#define INTERRUPT_PWA1			(ARM_IRQ2_BASE + 14)
#define INTERRUPT_CPR			(ARM_IRQ2_BASE + 15)
#define INTERRUPT_SMI			(ARM_IRQ2_BASE + 16)
#define INTERRUPT_GPIO0			(ARM_IRQ2_BASE + 17)
#define INTERRUPT_GPIO1			(ARM_IRQ2_BASE + 18)
#define INTERRUPT_GPIO2			(ARM_IRQ2_BASE + 19)
#define INTERRUPT_GPIO3			(ARM_IRQ2_BASE + 20)
#define INTERRUPT_VC_I2C		(ARM_IRQ2_BASE + 21)
#define INTERRUPT_VC_SPI		(ARM_IRQ2_BASE + 22)
#define INTERRUPT_VC_I2SPCM		(ARM_IRQ2_BASE + 23)
#define INTERRUPT_VC_SDIO		(ARM_IRQ2_BASE + 24)
#define INTERRUPT_VC_UART		(ARM_IRQ2_BASE + 25)
#define INTERRUPT_SLIMBUS		(ARM_IRQ2_BASE + 26)
#define INTERRUPT_VEC			(ARM_IRQ2_BASE + 27)
#define INTERRUPT_CPG			(ARM_IRQ2_BASE + 28)
#define INTERRUPT_RNG			(ARM_IRQ2_BASE + 29)
#define INTERRUPT_VC_ARASANSDIO		(ARM_IRQ2_BASE + 30)
#define INTERRUPT_AVSPMON		(ARM_IRQ2_BASE + 31)

/** Initialize bcm2835 intc */
int bcm2835_intc_init(void);

#endif
