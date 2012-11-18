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
 * @file gpt.h
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief OMAP general purpose timer APIs
 */
#ifndef __OMAP_GPT_H__
#define __OMAP_GPT_H__

#include <vmm_types.h>
#include <vmm_host_irq.h>
#include <omap/s32k-timer.h>

/* NOTE: GPT APIs use s32k timer APIs for calibration
 * hence make sure s32k is initialized before calling
 * any GPT APIs
 */

#define GPT_TIDR				0x000
#define GPT_TIDR_TID_REV_S			0
#define GPT_TIDR_TID_REV_M			0x000000FF

#define GPT_TIOCP_CFG				0x010
#define GPT_TIOCP_CFG_CLOCKACTIVITY_S		8
#define GPT_TIOCP_CFG_CLOCKACTIVITY_M		0x00000300
#define GPT_TIOCP_CFG_EMUFREE_S			5
#define GPT_TIOCP_CFG_EMUFREE_M			0x00000020
#define GPT_TIOCP_CFG_IDLEMODE_S		3
#define GPT_TIOCP_CFG_IDLEMODE_M		0x00000018
#define GPT_TIOCP_CFG_ENAWAKEUP_S		2
#define GPT_TIOCP_CFG_ENAWAKEUP_M		0x00000004
#define GPT_TIOCP_CFG_SOFTRESET_S		1
#define GPT_TIOCP_CFG_SOFTRESET_M		0x00000002
#define GPT_TIOCP_CFG_AUTOIDLE_S		0
#define GPT_TIOCP_CFG_AUTOIDLE_M		0x00000001

#define GPT_TISTAT				0x014
#define GPT_TISTAT_RESETDONE_S			0
#define GPT_TISTAT_RESETDONE_M			0x00000001

#define GPT_TISR				0x018
#define GPT_TISR_TCAR_IT_FLAG_S			2
#define GPT_TISR_TCAR_IT_FLAG_M			0x00000004
#define GPT_TISR_OVF_IT_FLAG_S			1
#define GPT_TISR_OVF_IT_FLAG_M			0x00000002
#define GPT_TISR_MAT_IT_FLAG_S			0
#define GPT_TISR_MAT_IT_FLAG_M			0x00000001

#define GPT_TIER				0x01C
#define GPT_TIER_TCAR_IT_ENA_S			2
#define GPT_TIER_TCAR_IT_ENA_M			0x00000004
#define GPT_TIER_OVF_IT_ENA_S			1
#define GPT_TIER_OVF_IT_ENA_M			0x00000002
#define GPT_TIER_MAT_IT_ENA_S			0
#define GPT_TIER_MAT_IT_ENA_M			0x00000001

#define GPT_TWER				0x020
#define GPT_TWER_TCAR_WUP_ENA_S			2
#define GPT_TWER_TCAR_WUP_ENA_M			0x00000004
#define GPT_TWER_OVF_WUP_ENA_S			1
#define GPT_TWER_OVF_WUP_ENA_M			0x00000002
#define GPT_TWER_MAT_WUP_ENA_S			0
#define GPT_TWER_MAT_WUP_ENA_M			0x00000001

#define GPT_TCLR				0x024
#define GPT_TCLR_GPO_CFG_S			14
#define GPT_TCLR_GPO_CFG_M			0x00004000
#define GPT_TCLR_CAPT_MODE_S			13
#define GPT_TCLR_CAPT_MODE_M			0x00002000
#define GPT_TCLR_PT_S				12
#define GPT_TCLR_PT_M				0x00001000
#define GPT_TCLR_TRG_S				10
#define GPT_TCLR_TRG_M				0x00000C00
#define GPT_TCLR_TCM_S				8
#define GPT_TCLR_TCM_M				0x00000300
#define GPT_TCLR_SCPWM_S			7
#define GPT_TCLR_SCPWM_M			0x00000080
#define GPT_TCLR_CE_S				6
#define GPT_TCLR_CE_M				0x00000040
#define GPT_TCLR_PRE_S				5
#define GPT_TCLR_PRE_M				0x00000020
#define GPT_TCLR_PTV_S				2
#define GPT_TCLR_PTV_M				0x0000001C
#define GPT_TCLR_AR_S				1
#define GPT_TCLR_AR_M				0x00000002
#define GPT_TCLR_ST_S				0
#define GPT_TCLR_ST_M				0x00000001

#define GPT_TCRR				0x028
#define GPT_TCRR_TIMER_COUNTER_S		0
#define GPT_TCRR_TIMER_COUNTER_M		0xFFFFFFFF

#define GPT_TLDR				0x02C
#define GPT_TLDR_LOAD_VALUE_S			0
#define GPT_TLDR_LOAD_VALUE_M			0xFFFFFFFF

#define GPT_TTGR				0x030
#define GPT_TTGR_TRIGGER_VALUE_S		0
#define GPT_TTGR_TRIGGER_VALUE_M		0xFFFFFFFF

#define GPT_TWPS				0x034
#define GPT_TWPS_W_PEND_TOWR_S			9
#define GPT_TWPS_W_PEND_TOWR_M			0x00000200
#define GPT_TWPS_W_PEND_TOCR_S			8
#define GPT_TWPS_W_PEND_TOCR_M			0x00000100
#define GPT_TWPS_W_PEND_TCVR_S			7
#define GPT_TWPS_W_PEND_TCVR_M			0x00000080
#define GPT_TWPS_W_PEND_TNIR_S			6
#define GPT_TWPS_W_PEND_TNIR_M			0x00000040
#define GPT_TWPS_W_PEND_TPIR_S			5
#define GPT_TWPS_W_PEND_TPIR_M			0x00000020
#define GPT_TWPS_W_PEND_TMAR_S			4
#define GPT_TWPS_W_PEND_TMAR_M			0x00000010
#define GPT_TWPS_W_PEND_TTGR_S			3
#define GPT_TWPS_W_PEND_TTGR_M			0x00000008
#define GPT_TWPS_W_PEND_TLDR_S			2
#define GPT_TWPS_W_PEND_TLDR_M			0x00000004
#define GPT_TWPS_W_PEND_TCRR_S			1
#define GPT_TWPS_W_PEND_TCRR_M			0x00000002
#define GPT_TWPS_W_PEND_TCLR_S			0
#define GPT_TWPS_W_PEND_TCLR_M			0x00000001

#define GPT_TMAR				0x038
#define GPT_TMAR_COMPARE_VALUE_S		0
#define GPT_TMAR_COMPARE_VALUE_M		0xFFFFFFFF

#define GPT_TCAR1				0x03C
#define GPT_TCAR1_CAPTURE_VALUE1_S		0
#define GPT_TCAR1_CAPTURE_VALUE1_M		0xFFFFFFFF

#define GPT_TSICR				0x040
#define GPT_TSICR_POSTED_S			2
#define GPT_TSICR_POSTED_M			0x00000004
#define GPT_TSICR_SFT_S				1
#define GPT_TSICR_SFT_M				0x00000002

#define GPT_TCAR2				0x044
#define GPT_TCAR2_CAPTURE_VALUE2_S		0
#define GPT_TCAR2_CAPTURE_VALUE2_M		0xFFFFFFFF

#define GPT_TPIR				0x048
#define GPT_TPIR_POSITIVE_INC_VALUE_S		0
#define GPT_TPIR_POSITIVE_INC_VALUE_M		0xFFFFFFFF

#define GPT_TNIR				0x04C
#define GPT_TNIR_NEGATIVE_INC_VALUE_S		0
#define GPT_TNIR_NEGATIVE_INC_VALUE_M		0xFFFFFFFF

#define GPT_TCVR				0x050
#define GPT_TCVR_COUNTER_VALUE_S		0
#define GPT_TCVR_COUNTER_VALUE_M		0xFFFFFFFF

#define GPT_TOCR				0x054
#define GPT_TOCR_OVF_COUNTER_VALUE_S		0
#define GPT_TOCR_OVF_COUNTER_VALUE_M		0x00FFFFFF

#define GPT_TOWR				0x058
#define GPT_TOWR_OVF_WRAPPING_VALUE_S		0
#define GPT_TOWR_OVF_WRAPPING_VALUE_M		0x00FFFFFF

struct gpt_cfg {
	const char *name;
	physical_addr_t base_pa;
	virtual_addr_t base_va;
	u32 cm_domain;
	u32 clksel_mask;
	u32 iclken_mask;
	u32 fclken_mask;
	bool src_sys_clk;
	u32 clk_hz;
	u32 irq_no;
};

int gpt_clocksource_init(u32 gpt_num, physical_addr_t prm_pa);
int gpt_clockchip_init(u32 gpt_num, physical_addr_t prm_pa);
int gpt_global_init(u32 gpt_count, struct gpt_cfg *cfg);

#endif
