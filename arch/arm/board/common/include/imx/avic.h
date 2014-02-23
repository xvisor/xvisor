/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
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
 * @file avic.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief header file for AVIC interrupt controller.
 *
 * Based on linux/arch/arm/mach-imx/avic.c
 *
 * Copyright 2004-2007 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2008 Juergen Beisert, kernel@pengutronix.de
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef __AVIC_H__
#define __AVIC_H__

#include <vmm_types.h>

#define AVIC_INTCNTL		0x00	/* int control reg */
#define AVIC_NIMASK		0x04	/* int mask reg */
#define AVIC_INTENNUM		0x08	/* int enable number reg */
#define AVIC_INTDISNUM		0x0C	/* int disable number reg */
#define AVIC_INTENABLEH		0x10	/* int enable reg high */
#define AVIC_INTENABLEL		0x14	/* int enable reg low */
#define AVIC_INTTYPEH		0x18	/* int type reg high */
#define AVIC_INTTYPEL		0x1C	/* int type reg low */
#define AVIC_NIPRIORITY(x)	(0x20 + 4 * (7 - (x))) /* int priority */
#define AVIC_NIVECSR		0x40	/* norm int vector/status */
#define AVIC_FIVECSR		0x44	/* fast int vector/status */
#define AVIC_INTSRCH		0x48	/* int source reg high */
#define AVIC_INTSRCL		0x4C	/* int source reg low */
#define AVIC_INTFRCH		0x50	/* int force reg high */
#define AVIC_INTFRCL		0x54	/* int force reg low */
#define AVIC_NIPNDH		0x58	/* norm int pending high */
#define AVIC_NIPNDL		0x5C	/* norm int pending low */
#define AVIC_FIPNDH		0x60	/* fast int pending high */
#define AVIC_FIPNDL		0x64	/* fast int pending low */

#define AVIC_NUM_IRQS 64

int avic_init(virtual_addr_t base);

#endif /* __AVIC_H__ */
