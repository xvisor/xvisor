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
 * @file realview.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Realview board interface header
 */

#ifndef _LINUX_REALVIEW_H
#define _LINUX_REALVIEW_H

/* ------------------------------------------------------------------------
 *  RealView Registers
 * ------------------------------------------------------------------------
 * 
 */
#define REALVIEW_SYS_ID_OFFSET               0x00
#define REALVIEW_SYS_SW_OFFSET               0x04
#define REALVIEW_SYS_LED_OFFSET              0x08
#define REALVIEW_SYS_OSC0_OFFSET             0x0C

#define REALVIEW_SYS_OSC1_OFFSET             0x10
#define REALVIEW_SYS_OSC2_OFFSET             0x14
#define REALVIEW_SYS_OSC3_OFFSET             0x18
#define REALVIEW_SYS_OSC4_OFFSET             0x1C	/* OSC1 for RealView/AB */

#define REALVIEW_SYS_LOCK_OFFSET             0x20
#define REALVIEW_SYS_100HZ_OFFSET            0x24
#define REALVIEW_SYS_CFGDATA1_OFFSET         0x28
#define REALVIEW_SYS_CFGDATA2_OFFSET         0x2C
#define REALVIEW_SYS_FLAGS_OFFSET            0x30
#define REALVIEW_SYS_FLAGSSET_OFFSET         0x30
#define REALVIEW_SYS_FLAGSCLR_OFFSET         0x34
#define REALVIEW_SYS_NVFLAGS_OFFSET          0x38
#define REALVIEW_SYS_NVFLAGSSET_OFFSET       0x38
#define REALVIEW_SYS_NVFLAGSCLR_OFFSET       0x3C
#define REALVIEW_SYS_RESETCTL_OFFSET         0x40
#define REALVIEW_SYS_PCICTL_OFFSET           0x44
#define REALVIEW_SYS_MCI_OFFSET              0x48
#define REALVIEW_SYS_FLASH_OFFSET            0x4C
#define REALVIEW_SYS_CLCD_OFFSET             0x50
#define REALVIEW_SYS_CLCDSER_OFFSET          0x54
#define REALVIEW_SYS_BOOTCS_OFFSET           0x58
#define REALVIEW_SYS_24MHz_OFFSET            0x5C
#define REALVIEW_SYS_MISC_OFFSET             0x60
#define REALVIEW_SYS_IOSEL_OFFSET            0x70
#define REALVIEW_SYS_PROCID_OFFSET           0x84
#define REALVIEW_SYS_TEST_OSC0_OFFSET        0xC0
#define REALVIEW_SYS_TEST_OSC1_OFFSET        0xC4
#define REALVIEW_SYS_TEST_OSC2_OFFSET        0xC8
#define REALVIEW_SYS_TEST_OSC3_OFFSET        0xCC
#define REALVIEW_SYS_TEST_OSC4_OFFSET        0xD0

/*
 * System identification (REALVIEW_SYS_ID)
 */
#define REALVIEW_SYS_ID_BOARD_MASK	0x0FFF0000
#define REALVIEW_SYS_ID_BOARD_SHIFT	16
#define REALVIEW_SYS_ID_EB		0x0140
#define REALVIEW_SYS_ID_PBA8		0x0178

#define REALVIEW_SYS_CTRL_LED         (1 << 0)

/* ------------------------------------------------------------------------
 *  RealView control registers
 * ------------------------------------------------------------------------
 */

/* 
 * REALVIEW_IDFIELD
 *
 * 31:24 = manufacturer (0x41 = ARM)
 * 23:16 = architecture (0x08 = AHB system bus, ASB processor bus)
 * 15:12 = FPGA (0x3 = XVC600 or XVC600E)
 * 11:4  = build value
 * 3:0   = revision number (0x1 = rev B (AHB))
 */

/*
 * REALVIEW_SYS_LOCK
 *     control access to SYS_OSCx, SYS_CFGDATAx, SYS_RESETCTL, 
 *     SYS_CLD, SYS_BOOTCS
 */
#define REALVIEW_SYS_LOCK_LOCKED    		(1 << 16)
#define REALVIEW_SYS_LOCKVAL			0xA05F
#define REALVIEW_SYS_LOCKVAL_MASK		0xFFFF	/* write 0xA05F to enable write access */

#define REALVIEW_SYS_CLCD_NLCDIOON		(1 << 2)
#define REALVIEW_SYS_CLCD_VDDPOSSWITCH		(1 << 3)
#define REALVIEW_SYS_CLCD_PWR3V5SWITCH		(1 << 4)
#define REALVIEW_SYS_CLCD_ID_MASK		(0x1f << 8)
#define REALVIEW_SYS_CLCD_ID_SANYO_3_8		(0x00 << 8)
#define REALVIEW_SYS_CLCD_ID_UNKNOWN_8_4	(0x01 << 8)
#define REALVIEW_SYS_CLCD_ID_EPSON_2_2		(0x02 << 8)
#define REALVIEW_SYS_CLCD_ID_SANYO_2_5		(0x07 << 8)
#define REALVIEW_SYS_CLCD_ID_VGA		(0x1f << 8)

/* Platform control */

u32 realview_board_id(void);
const char *realview_clcd_panel_name(void);
void realview_clcd_disable_power(void);
void realview_clcd_enable_power(void);
void realview_flags_set(u32 data);
int realview_system_reset(void);
void *realview_get_24mhz_clock_base(void);
void *realview_system_base(void);

/* Initialization functions */

void realview_sysreg_early_init(void *base);
void realview_sysreg_of_early_init(void);

#endif
