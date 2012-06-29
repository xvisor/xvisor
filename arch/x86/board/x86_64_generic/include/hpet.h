/**
 * Copyright (c) 2012 Himanshu Chauhan.
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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief HPET access and configuration.
 */

#ifndef __HPET_H
#define __HPET_H

#include <vmm_types.h>

#define DEFAULT_HPET_SYS_TIMER		0 /* Timer 0 is system timer */

#define HPET_CAP_LEGACY_SUPPORT		(0x01UL << 0)
#define HPET_CAP_FSB_DELIVERY		(0x01UL << 1)

enum {
	HPET_GEN_CAP_REG = 0,
	HPET_GEN_CONF_REG,
	HPET_GEN_INT_STATUS_REG,
	HPET_GEN_MAIN_CNTR_REG,
	HPET_TIMER_N_CONF_REG,
	NR_HPET_REGS
};

#define HPET_GEN_CAP_ID_BASE		(0x00)
#define HPET_GEN_CONF_BASE		(0x10)
#define HPET_GEN_INT_STATUS_BASE	(0x20)
#define HPET_GEN_MAIN_CNTR_BASE		(0xF0)
#define HPET_TIMER_N_CONF_BASE(__n)	(0x100 + 0x20 * __n)
#define HPET_TIMER_N_COMP_BASE(__n)	(0x108 + 0x20 * __n)

#define HPET_TIMER_PERIODIC		(0x01UL << 0)
#define HPET_TIMER_INT_TO_FSB		(0x01UL << 1)
#define HPET_TIMER_FORCE_32BIT		(0x01UL << 2)
#define HPET_TIMER_INT_EDGE		(0x01UL << 3)

struct hpet {
	physical_addr_t pbase;
	virtual_addr_t vbase;
	u32 capabilities;
} __packed;

#endif /* __HPET_H */
