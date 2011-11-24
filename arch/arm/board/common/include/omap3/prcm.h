/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
 * Copyright (c) 2011 Sukanto Ghosh.
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
 * @file prcm.h
 * @version 1.0
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief OMAP3 Power, Reset, and Clock Managment APIs
 */
#ifndef __OMAP3_PRCM_H__
#define __OMAP3_PRCM_H__

#include <vmm_types.h>

#define OMAP3_SYSCLK_S12M			12000000
#define OMAP3_SYSCLK_S13M			13000000
#define OMAP3_SYSCLK_S19_2M			19200000
#define OMAP3_SYSCLK_S24M			24000000
#define OMAP3_SYSCLK_S26M			26000000
#define OMAP3_SYSCLK_S38_4M			38400000

#define OMAP3_PRCM_BASE				0x48004000

#define OMAP3_CM_BASE				0x48004000
#define OMAP3_CM_SIZE				0x1500

#define OMAP3_IVA2_CM_BASE			0x48004000
#define OMAP3_OCP_SYS_REG_CM_BASE		0x48004800
#define OMAP3_MPU_CM_BASE			0x48004900
#define OMAP3_CORE_CM_BASE			0x48004A00
#define OMAP3_SGX_CM_BASE			0x48004B00
#define OMAP3_WKUP_CM_BASE			0x48004C00
#define OMAP3_CLOCK_CTRL_REG_CM_BASE		0x48004D00
#define OMAP3_DSS_CM_BASE			0x48004E00
#define OMAP3_CAM_CM_BASE			0x48004F00
#define OMAP3_PER_CM_BASE			0x48005000
#define OMAP3_EMU_CM_BASE			0x48005100
#define OMAP3_GLOBAL_REG_CM_BASE		0x48005200
#define OMAP3_NEON_CM_BASE			0x48005300
#define OMAP3_USBHOST_CM_BASE			0x48005400

#define OMAP3_PRM_BASE				0x48306000
#define OMAP3_PRM_SIZE				0x1500

#define OMAP3_IVA2_PRM_BASE			0x48306000
#define OMAP3_OCP_SYS_REG_PRM_BASE		0x48306800
#define OMAP3_MPU_PRM_BASE			0x48306900
#define OMAP3_CORE_PRM_BASE			0x48306A00
#define OMAP3_SGX_PRM_BASE			0x48306B00
#define OMAP3_WKUP_PRM_BASE			0x48306C00
#define OMAP3_CLOCK_CTRL_REG_PRM_BASE		0x48306D00
#define OMAP3_DSS_PRM_BASE			0x48306E00
#define OMAP3_CAM_PRM_BASE			0x48306F00
#define OMAP3_PER_PRM_BASE			0x48307000
#define OMAP3_EMU_PRM_BASE			0x48307100
#define OMAP3_GLOBAL_REG_PRM_BASE		0x48307200
#define OMAP3_NEON_PRM_BASE			0x48307300
#define OMAP3_USBHOST_PRM_BASE			0x48307400

#define OMAP3_PRM_CLKSRC_CTRL			0x70
#define OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_S	6
#define OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_M	(0x3 << 6)
#define OMAP3_PRM_CLKSRC_CTRL_AUTOEXTCLK_S	3
#define OMAP3_PRM_CLKSRC_CTRL_AUTOEXTCLK_M	(0x3 << 3)
#define OMAP3_PRM_CLKSRC_CTRL_SYSCLKSEL_S	0
#define OMAP3_PRM_CLKSRC_CTRL_SYSCLKSEL_M	(0x3 << 0)

#define OMAP3_CM_FCLKEN_WKUP			0x00
#define OMAP3_CM_FCLKEN_WKUP_EN_WDT2_S		5
#define OMAP3_CM_FCLKEN_WKUP_EN_WDT2_M		(1 << 5)
#define OMAP3_CM_FCLKEN_WKUP_EN_GPIO1_S		3
#define OMAP3_CM_FCLKEN_WKUP_EN_GPIO1_M		(1 << 3)
#define OMAP3_CM_FCLKEN_WKUP_EN_GPT1_S		0
#define OMAP3_CM_FCLKEN_WKUP_EN_GPT1_M		(1 << 0)

#define OMAP3_CM_ICLKEN_WKUP			0x10
#define OMAP3_CM_ICLKEN_WKUP_EN_WDT2_S		5
#define OMAP3_CM_ICLKEN_WKUP_EN_WDT2_M		(1 << 5)
#define OMAP3_CM_ICLKEN_WKUP_EN_GPIO1_S		3
#define OMAP3_CM_ICLKEN_WKUP_EN_GPIO1_M		(1 << 3)
#define OMAP3_CM_ICLKEN_WKUP_EN_32KSYNC_S	2
#define OMAP3_CM_ICLKEN_WKUP_EN_32KSYNC_M	(1 << 2)
#define OMAP3_CM_ICLKEN_WKUP_EN_GPT1_S		0
#define OMAP3_CM_ICLKEN_WKUP_EN_GPT1_M		(1 << 0)

#define OMAP3_CM_IDLEST_WKUP			0x20
#define OMAP3_CM_IDLEST_WKUP_ST_WDT2_S		5
#define OMAP3_CM_IDLEST_WKUP_ST_WDT2_M		(1 << 5)
#define OMAP3_CM_IDLEST_WKUP_ST_GPIO1_S		3
#define OMAP3_CM_IDLEST_WKUP_ST_GPIO1_M		(1 << 3)
#define OMAP3_CM_IDLEST_WKUP_ST_32KSYNC_S	2
#define OMAP3_CM_IDLEST_WKUP_ST_32KSYNC_M	(1 << 2)
#define OMAP3_CM_IDLEST_WKUP_ST_GPT1_S		0
#define OMAP3_CM_IDLEST_WKUP_ST_GPT1_M		(1 << 0)

#define OMAP3_CM_AUTOIDLE_WKUP			0x30
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_WDT2_S	5
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_WDT2_M	(1 << 5)
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_GPIO1_S	3
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_GPIO1_M	(1 << 3)
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_32KSYNC_S	2
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_32KSYNC_M	(1 << 2)
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_GPT1_S	0
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_GPT1_M	(1 << 0)

#define OMAP3_CM_CLKSEL_WKUP			0x40
#define OMAP3_CM_CLKSEL_WKUP_CLKSEL_RM_S	1
#define OMAP3_CM_CLKSEL_WKUP_CLKSEL_RM_M	(0x3 << 1)
#define OMAP3_CM_CLKSEL_WKUP_CLKSEL_GPT1_S	0
#define OMAP3_CM_CLKSEL_WKUP_CLKSEL_GPT1_M	(1 << 0)


#endif
