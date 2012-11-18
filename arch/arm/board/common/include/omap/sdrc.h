/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file sdrc.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for OMAP SDRC controller
 */

#ifndef __OMAP_SDRC_H_
#define __OMAP_SDRC_H_

#include <vmm_types.h>

/** SDRC register space size */
#define SDRC_REG_SIZE		0x1000

/** SDRC register offsets */
#define SDRC_SYSCONFIG		0x010
#define SDRC_CS_CFG		0x040
#define SDRC_SHARING		0x044
#define SDRC_ERR_TYPE		0x04C
#define SDRC_DLLA_CTRL		0x060
#define SDRC_DLLA_STATUS	0x064
#define SDRC_DLLB_CTRL		0x068
#define SDRC_DLLB_STATUS	0x06C
#define SDRC_POWER		0x070
#define SDRC_MCFG_0		0x080
#define SDRC_MR_0		0x084
#define SDRC_EMR2_0		0x08c
#define SDRC_ACTIM_CTRL_A_0	0x09c
#define SDRC_ACTIM_CTRL_B_0	0x0a0
#define SDRC_RFR_CTRL_0		0x0a4
#define SDRC_MANUAL_0		0x0a8
#define SDRC_MCFG_1		0x0B0
#define SDRC_MR_1		0x0B4
#define SDRC_EMR2_1		0x0BC
#define SDRC_ACTIM_CTRL_A_1	0x0C4
#define SDRC_ACTIM_CTRL_B_1	0x0C8
#define SDRC_RFR_CTRL_1		0x0D4
#define SDRC_MANUAL_1		0x0D8

#define SDRC_POWER_AUTOCOUNT_SHIFT	8
#define SDRC_POWER_AUTOCOUNT_MASK	(0xffff << SDRC_POWER_AUTOCOUNT_SHIFT)
#define SDRC_POWER_CLKCTRL_SHIFT	4
#define SDRC_POWER_CLKCTRL_MASK		(0x3 << SDRC_POWER_CLKCTRL_SHIFT)
#define SDRC_POWER_EXTCLKDIS_SHIFT	3
#define SDRC_POWER_PWDENA_SHIFT		2
#define SDRC_POWER_PAGEPOLICY_SHIFT	0
#define SDRC_SELF_REFRESH_ON_AUTOCOUNT	(0x2 << SDRC_POWER_CLKCTRL_SHIFT)

/*
 * These values represent the number of memory clock cycles between
 * autorefresh initiation.  They assume 1 refresh per 64 ms (JEDEC), 8192
 * rows per device, and include a subtraction of a 50 cycle window in the
 * event that the autorefresh command is delayed due to other SDRC activity.
 * The '| 1' sets the ARE field to send one autorefresh when the autorefresh
 * counter reaches 0.
 *
 * These represent optimal values for common parts, it won't work for all.
 * As long as you scale down, most parameters are still work, they just
 * become sub-optimal. The RFR value goes in the opposite direction. If you
 * don't adjust it down as your clock period increases the refresh interval
 * will not be met. Setting all parameters for complete worst case may work,
 * but may cut memory performance by 2x. Due to errata the DLLs need to be
 * unlocked and their value needs run time calibration.	A dynamic call is
 * need for that as no single right value exists acorss production samples.
 *
 * Only the FULL speed values are given. Current code is such that rate
 * changes must be made at DPLLoutx2. The actual value adjustment for low
 * frequency operation will be handled by omap_set_performance()
 *
 * By having the boot loader boot up in the fastest L4 speed available likely
 * will result in something which you can switch between.
 */
#define SDRC_RFR_CTRL_165MHz	(0x00044c00 | 1)
#define SDRC_RFR_CTRL_133MHz	(0x0003de00 | 1)
#define SDRC_RFR_CTRL_100MHz	(0x0002da01 | 1)
#define SDRC_RFR_CTRL_110MHz	(0x0002da01 | 1) /* Need to calc */
#define SDRC_RFR_CTRL_BYPASS	(0x00005000 | 1) /* Need to calc */

/* Minimum frequency that the SDRC DLL can lock at */
#define MIN_SDRC_DLL_LOCK_FREQ		83000000

/* Scale factor for fixed-point arith in omap3_core_dpll_m2_set_rate() */
#define SDRC_MPURATE_SCALE		8

/* 2^SDRC_MPURATE_BASE_SHIFT: MPU MHz that SDRC_MPURATE_LOOPS is defined for */
#define SDRC_MPURATE_BASE_SHIFT		9

/*
 * SDRC_MPURATE_LOOPS: Number of MPU loops to execute at
 * 2^MPURATE_BASE_SHIFT MHz for SDRC to stabilize
 */
#define SDRC_MPURATE_LOOPS		96

/** SMS register space size */
#define SMS_REG_SIZE		0x1000

/** SMS register offsets */
#define SMS_SYSCONFIG			0x010
#define SMS_ROT_CONTROL(context)	(0x180 + 0x10 * context)
#define SMS_ROT_SIZE(context)		(0x184 + 0x10 * context)
#define SMS_ROT_PHYSICAL_BA(context)	(0x188 + 0x10 * context)

/**
 * SDRC parameters for a given SDRC clock rate
 *
 * This structure holds a pre-computed set of register values for the
 * SDRC for a given SDRC clock rate and SDRAM chip.
 *
 * @rate: SDRC clock rate (in Hz)
 * @actim_ctrla: Value to program to SDRC_ACTIM_CTRLA for this rate
 * @actim_ctrlb: Value to program to SDRC_ACTIM_CTRLB for this rate
 * @rfr_ctrl: Value to program to SDRC_RFR_CTRL for this rate
 * @mr: Value to program to SDRC_MR for this rate
 */
struct sdrc_params {
	unsigned long rate;
	u32 actim_ctrla;
	u32 actim_ctrlb;
	u32 rfr_ctrl;
	u32 mr;
};

/** Initialize OMAP SDRC controller */
int sdrc_init(physical_addr_t sdrc_base_pa,
		physical_addr_t sms_base_pa,
		struct sdrc_params *sdrc_cs0,
		struct sdrc_params *sdrc_cs1);

#endif /* __OMAP_SDRC_H_ */
