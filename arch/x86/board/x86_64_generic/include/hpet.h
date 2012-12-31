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
 * @brief Declarations related to HPET code.
 */

#ifndef __HPET_H
#define __HPET_H

#include <vmm_types.h>
#include <vmm_clockchip.h>
#include <vmm_clocksource.h>

#define HPET_CAP_LEGACY_SUPPORT		(0x01UL << 0)
#define HPET_CAP_FSB_DELIVERY		(0x01UL << 1)

#define HPET_GEN_CAP_ID_BASE		(0x00)
#define HPET_GEN_CONF_BASE		(0x10)
#define HPET_GEN_INT_STATUS_BASE	(0x20)
#define HPET_GEN_MAIN_CNTR_BASE		(0xF0)
#define HPET_TIMER_N_CONF_BASE(__n)	(0x100 + 0x20 * __n)
#define HPET_TIMER_N_COMP_BASE(__n)	(0x108 + 0x20 * __n)

#define HPET_BLOCK_REV_ID(x)		(((u64)x) & 0xFFUL)
#define HPET_BLOCK_NR_TIMERS(x)		((((u64)x) >> 8) & 0x1fUL)
#define HPET_BLOCK_CNTR_SIZE(x)		(((((u64)x) >> 13) & 0x1UL) ? 64 : 32)
#define HPET_BLOCK_HAS_LEGACY_ROUTE(x)	((((u64)x) >> 15) & 0x1UL)
#define HPET_BLOCK_CLK_PERIOD(x)	((((u64)x) >> 32) & 0xFFFFFFFFUL)

#define HPET_TIMER_PERIODIC		(0x01UL << 0)
#define HPET_TIMER_INT_TO_FSB		(0x01UL << 1)
#define HPET_TIMER_FORCE_32BIT		(0x01UL << 2)
#define HPET_TIMER_INT_EDGE		(0x01UL << 3)

typedef u32 timer_id_t;
#define HPET_TIMER_NR_BITS		(4) /* 4 bits to store timer index */
#define HPET_BLOCK_NR_BITS		(4) /* 4 bits to store timer hpet block */
#define HPET_TIMER_BLOCK_SHIFT		(HPET_TIMER_NR_BITS)
#define HPET_TIMER_CHIP_SHIFT		(HPET_TIMER_NR_BITS + HPET_TIMER_BLOCK_SHIFT)
#define HPET_TIMER_MASK			(0xFUL)
#define HPET_TIMER_BLOCK_MASK		(0xFUL << HPET_TIMER_BLOCK_SHIFT)
#define HPET_TIMER_CHIP_MASK		(0xFUL << HPET_TIMER_CHIP_SHIFT)
#define HPET_TIMER_BLOCK(_timer_id)	((_timer_id & HPET_TIMER_BLOCK_MASK) >> HPET_TIMER_BLOCK_SHIFT)
#define HPET_TIMER(_timer_id)		((_timer_id & HPET_TIMER_MASK))
#define HPET_TIMER_CHIP(_timer_id)	((_timer_id & HPET_TIMER_CHIP_MASK) >> HPET_TIMER_CHIP_SHIFT)
#define MK_TIMER_ID(_chip, _block, _timer)	((timer_id_t)((_chip << HPET_TIMER_CHIP_SHIFT) \
						       | (_block << HPET_TIMER_BLOCK_SHIFT) \
						       | (_timer & HPET_TIMER_MASK)))

#define DEFAULT_HPET_SYS_TIMER		MK_TIMER_ID(0, 0, 0) /* system timer (chip 0, block 0, timer 0) */
#define DEFAULT_SYS_TIMER_IRQ_NUM	32

/************************************************************************
 * The system can have multiple HPET chips. Each chip can have upto 8
 * timer blocks. Each block can have upto 32 timers.
 ************************************************************************/
struct hpet_devices {
	int nr_chips;			/* Number of physical HPET on mother board */
	struct dlist chip_list;		/* List of all such chips */
};

struct hpet_chip {
	u32 chip_id;
	u32 nr_blocks;
	struct dlist head;
	struct dlist block_list;	/* list of all blocks in chip. */
	struct hpet_devices *parent;	/* parent HPET device */
};

struct hpet_block {
	u32 block_id;
	u32 nr_timers;			/* Number of timers in this block */
	physical_addr_t pbase;		/* physical base of the block */
	virtual_addr_t vbase;		/* virtual base */
	u64 capabilities;		/* capabilities of the block */
	struct dlist head;
	struct dlist timer_list;	/* list of timers in this block */
	struct hpet_chip *parent;	/* parent hpet chip */
};

struct hpet_timer {
	u32 timer_id;
	u64 conf_cap;
	u32 is_busy;			/* If under use */
	u64 hpet_period;		/* femto sec */
	u64 hpet_freq;
	struct vmm_clockchip clkchip;	/* clock chip for this timer */
	struct vmm_clocksource clksrc;	/* clock source for this timer */
	struct dlist head;
	struct hpet_block *parent;	/* parent block of this timer */
};

int hpet_init(void);

#endif /* __HPET_H */
