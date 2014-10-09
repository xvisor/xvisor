/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file hpet.h
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief Definitions related to HPET.
 *
 * This file has been adapted for Xvisor
 * from Qemu/include/hw/timer/hpet.h
 *
 * Original Copyright:
 * Copyright IBM, Corp. 2008
 *
 * Authors:
 *  Beth Kon   <bkon@us.ibm.com>
 *
 */
#ifndef __HPET_EMUL_H
#define __HPET_EMUL_H

#include <vmm_types.h>

#define HPET_BASE               0xfed00000
#define HPET_CLK_PERIOD         10000000ULL /* 10000000 femtoseconds == 10ns*/

#define FS_PER_NS 1000000
#define HPET_MIN_TIMERS         3
#define HPET_MAX_TIMERS         32

#define HPET_NUM_IRQ_ROUTES     32

#define HPET_LEGACY_PIT_INT     0
#define HPET_LEGACY_RTC_INT     1

#define HPET_CFG_ENABLE		0x001
#define HPET_CFG_LEGACY		0x002

#define HPET_ID			0x000
#define HPET_PERIOD		0x004
#define HPET_CFG		0x010
#define HPET_STATUS		0x020
#define HPET_COUNTER		0x0f0
#define HPET_TN_CFG		0x000
#define HPET_TN_CMP		0x008
#define HPET_TN_ROUTE		0x010
#define HPET_CFG_WRITE_MASK	0x3

#define HPET_ID_NUM_TIM_SHIFT   8
#define HPET_ID_NUM_TIM_MASK    0x1f00

#define HPET_TN_TYPE_LEVEL	0x002
#define HPET_TN_ENABLE		0x004
#define HPET_TN_PERIODIC	0x008
#define HPET_TN_PERIODIC_CAP	0x010
#define HPET_TN_SIZE_CAP	0x020
#define HPET_TN_SETVAL		0x040
#define HPET_TN_32BIT		0x100
#define HPET_TN_INT_ROUTE_MASK	0x3e00
#define HPET_TN_FSB_ENABLE	0x4000
#define HPET_TN_FSB_CAP		0x8000
#define HPET_TN_CFG_WRITE_MASK	0x7f4e
#define HPET_TN_INT_ROUTE_SHIFT	9
#define HPET_TN_INT_ROUTE_CAP_SHIFT 32
#define HPET_TN_CFG_BITS_READONLY_OR_RESERVED 0xffff80b1U

struct hpet_fw_entry {
    u32 event_timer_block_id;
    u64 address;
    u16 min_tick;
    u8 page_prot;
} __packed;

struct hpet_fw_config {
    u8 count;
    struct hpet_fw_entry hpet[8];
} __packed;

extern struct hpet_fw_config hpet_cfg;

#endif
