/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @file omap3_prcm.h
 * @version 1.0
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief OMAP3 Power, Reset, and Clock Managment APIs
 */
#ifndef __OMAP3_PRCM_H__
#define __OMAP3_PRCM_H__

#include <vmm_types.h>

#define OMAP3_IVA2_CM_BASE			0x48004000
#define OMAP3_OCP_System_Reg_CM			0x48004800
#define OMAP3_MPU_CM				0x48004900
#define OMAP3_CORE_CM				0x48004A00
#define OMAP3_SGX_CM				0x48004B00
#define OMAP3_WKUP_CM				0x48004C00
#define OMAP3_CLOCK_CONTROL_REG_CM		0x48004D00
#define OMAP3_DSS_CM				0x48004E00
#define OMAP3_CAM_CM				0x48004F00
#define OMAP3_PER_CM				0x48005000
#define OMAP3_EMU_CM				0x48005100
#define OMAP3_GLOBAL_REG_CM			0x48005200
#define OMAP3_NEON_CM				0x48005300
#define OMAP3_USBHOST_CM			0x48005400

#endif
