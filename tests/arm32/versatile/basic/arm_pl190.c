/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file pl190.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief PL190 Vectored Interrupt Controller source
 */

#include <arm_config.h>
#include <arm_io.h>
#include <arm_pl190.h>
#include <arm_plat.h>

#define VERSATILE_VIC_MAX_NR	1

struct arm_pl190_chip_data {
	u32 irq_offset;
	virtual_addr_t cpu_base;
};

static struct arm_pl190_chip_data arm_pl190_data[VERSATILE_VIC_MAX_NR];

static inline void arm_pl190_write(u32 val, virtual_addr_t addr)
{
	arm_writel(val, (void *)(addr));
}

static inline u32 arm_pl190_read(virtual_addr_t addr)
{
	return arm_readl((void *)(addr));
}

int arm_pl190_active_irq(u32 arm_pl190_nr)
{
	u32 int_status;

	if (VERSATILE_VIC_MAX_NR <= arm_pl190_nr) {
		return -1;
	}

	int_status =
	    arm_pl190_read(arm_pl190_data[arm_pl190_nr].cpu_base +
			   PL190_IRQ_STATUS);

	if (int_status) {
		int ret;

		for (ret = 0; ret < 32; ret++) {
			if ((int_status >> ret) & 1) {
				break;
			}
		}

		ret += arm_pl190_data[arm_pl190_nr].irq_offset;

		return ret;
	} else {
		return -1;
	}
}

int arm_pl190_ack_irq(u32 arm_pl190_nr, u32 irq)
{
	u32 mask;

	if (VERSATILE_VIC_MAX_NR <= arm_pl190_nr) {
		return -1;
	}

	if (irq < arm_pl190_data[arm_pl190_nr].irq_offset) {
		return -1;
	}

	mask = 1 << (irq - arm_pl190_data[arm_pl190_nr].irq_offset);

	arm_pl190_write(mask,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_INT_ENABLE_CLEAR);

	arm_pl190_write(mask,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_INT_SOFT_CLEAR);

	arm_pl190_write(mask,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_INT_ENABLE);

	return 0;
}

int arm_pl190_mask(u32 arm_pl190_nr, u32 irq)
{
	u32 mask;

	if (VERSATILE_VIC_MAX_NR <= arm_pl190_nr) {
		return -1;
	}

	if (irq < arm_pl190_data[arm_pl190_nr].irq_offset) {
		return -1;
	}

	mask = 1 << (irq - arm_pl190_data[arm_pl190_nr].irq_offset);

	arm_pl190_write(mask,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_INT_ENABLE_CLEAR);

	return 0;
}

int arm_pl190_unmask(u32 arm_pl190_nr, u32 irq)
{
	u32 mask;

	if (VERSATILE_VIC_MAX_NR <= arm_pl190_nr) {
		return -1;
	}

	if (irq < arm_pl190_data[arm_pl190_nr].irq_offset) {
		return -1;
	}

	mask = 1 << (irq - arm_pl190_data[arm_pl190_nr].irq_offset);

	arm_pl190_write(mask,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_INT_ENABLE);

	return 0;
}

int arm_pl190_cpu_init(u32 arm_pl190_nr, virtual_addr_t base)
{
	//unsigned int i;

	if (arm_pl190_nr >= VERSATILE_VIC_MAX_NR) {
		return -1;
	}

	arm_pl190_data[arm_pl190_nr].cpu_base = base;

	/* We have 32 interrupts per PL190 */
	arm_pl190_data[arm_pl190_nr].irq_offset = 32 * arm_pl190_nr;

	arm_pl190_write(0,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_INT_SELECT);
	arm_pl190_write(0,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_INT_ENABLE);
	arm_pl190_write(~0,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_INT_ENABLE_CLEAR);
	arm_pl190_write(0,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_IRQ_STATUS);
	arm_pl190_write(0, arm_pl190_data[arm_pl190_nr].cpu_base + PL190_ITCR);
	arm_pl190_write(~0,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_INT_SOFT_CLEAR);

#if 0
	arm_pl190_write(0,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_VECT_ADDR);
	for (i = 0; i < 19; i++) {
		unsigned int value =
		    arm_pl190_read(arm_pl190_data[arm_pl190_nr].cpu_base +
				   PL190_VECT_ADDR);
		arm_pl190_write(value,
				arm_pl190_data[arm_pl190_nr].cpu_base +
				PL190_VECT_ADDR);
	}

	for (i = 0; i < 16; i++) {
		arm_pl190_write(VIC_VECT_CNTL_ENABLE | i,
				arm_pl190_data[arm_pl190_nr].cpu_base +
				PL190_VECT_CNTL0 + (i * 4));
	}

	arm_pl190_write(32,
			arm_pl190_data[arm_pl190_nr].cpu_base +
			PL190_DEF_VECT_ADDR);
#endif

	return 0;
}
